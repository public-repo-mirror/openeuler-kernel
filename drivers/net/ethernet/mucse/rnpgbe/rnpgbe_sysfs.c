// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2022 - 2024 Mucse Corporation. */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/netdevice.h>
#include <linux/hwmon.h>
#include <linux/ctype.h>

#include "rnpgbe.h"
#include "rnpgbe_common.h"
#include "rnpgbe_type.h"
#include "rnpgbe_mbx.h"
#include "rnpgbe_mbx_fw.h"

struct maintain_req {
	int magic;
#define MAINTAIN_MAGIC 0xa6a7a8a9

	int cmd;
	int arg0;
	int req_data_bytes;
	int reply_bytes;
	char data[0];
} __attribute__((packed));

struct maintain_reply {
	int magic;
#define MAINTAIN_REPLY_MAGIC 0xB6B7B8B9
	int cmd;
	int arg0;
	int data_bytes;
	int rev;
	int data[0];
} __attribute__((packed));

struct ucfg_mac_sn {
	unsigned char macaddr[64];
	unsigned char sn[32];
	int magic;
#define MAC_SN_MAGIC 0x87654321
	char rev[52];
	unsigned char pn[32];
} __attribute__((packed, aligned(4)));

static int print_desc(char *buf, void *data, int len)
{
	u8 *ptr = (u8 *)data;
	int ret = 0;
	int i = 0;

	for (i = 0; i < len; i++)
		ret += sprintf(buf + ret, "%02x ", *(ptr + i));

	return ret;
}

#ifdef RNP_HWMON
static ssize_t rnpgbe_hwmon_show_location(struct device __always_unused *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct hwmon_attr *rnpgbe_attr =
		container_of(attr, struct hwmon_attr, dev_attr);

	return snprintf(buf, PAGE_SIZE, "loc%u\n",
			rnpgbe_attr->sensor->location);
}

static ssize_t rnpgbe_hwmon_show_name(struct device __always_unused *dev,
				      struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "rnp\n");
}

static ssize_t rnpgbe_hwmon_show_temp(struct device __always_unused *dev,
				      struct device_attribute *attr, char *buf)
{
	struct hwmon_attr *rnpgbe_attr =
		container_of(attr, struct hwmon_attr, dev_attr);
	unsigned int value;

	/* reset the temp field */
	rnpgbe_attr->hw->ops.get_thermal_sensor_data(rnpgbe_attr->hw);

	value = rnpgbe_attr->sensor->temp;
	/* display millidegree */
	value *= 1000;

	return snprintf(buf, PAGE_SIZE, "%u\n", value);
}

static ssize_t
rnpgbe_hwmon_show_cautionthresh(struct device __always_unused *dev,
				struct device_attribute *attr, char *buf)
{
	struct hwmon_attr *rnpgbe_attr =
		container_of(attr, struct hwmon_attr, dev_attr);
	unsigned int value = rnpgbe_attr->sensor->caution_thresh;
	/* display millidegree */
	value *= 1000;

	return snprintf(buf, PAGE_SIZE, "%u\n", value);
}

static ssize_t rnpgbe_hwmon_show_maxopthresh(struct device __always_unused *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct hwmon_attr *rnpgbe_attr =
		container_of(attr, struct hwmon_attr, dev_attr);
	unsigned int value = rnpgbe_attr->sensor->max_op_thresh;

	/* display millidegree */
	value *= 1000;

	return snprintf(buf, PAGE_SIZE, "%u\n", value);
}

/**
 * rnpgbe_add_hwmon_attr - Create hwmon attr table for a hwmon sysfs file.
 * @adapter: pointer to the adapter structure
 * @offset: offset in the eeprom sensor data table
 * @type: type of sensor data to display
 *
 * For each file we want in hwmon's sysfs interface we need a device_attribute
 * This is included in our hwmon_attr struct that contains the references to
 * the data structures we need to get the data to display.
 */
static int rnpgbe_add_hwmon_attr(struct rnpgbe_adapter *adapter,
				 unsigned int offset, int type)
{
	unsigned int n_attr;
	struct hwmon_attr *rnpgbe_attr;

	n_attr = adapter->rnpgbe_hwmon_buff->n_hwmon;
	rnpgbe_attr = &adapter->rnpgbe_hwmon_buff->hwmon_list[n_attr];

	switch (type) {
	case RNP_HWMON_TYPE_LOC:
		rnpgbe_attr->dev_attr.show = rnpgbe_hwmon_show_location;
		snprintf(rnpgbe_attr->name, sizeof(rnpgbe_attr->name),
			 "temp%u_label", offset + 1);
		break;
	case RNP_HWMON_TYPE_NAME:
		rnpgbe_attr->dev_attr.show = rnpgbe_hwmon_show_name;
		snprintf(rnpgbe_attr->name, sizeof(rnpgbe_attr->name), "name");
		break;
	case RNP_HWMON_TYPE_TEMP:
		rnpgbe_attr->dev_attr.show = rnpgbe_hwmon_show_temp;
		snprintf(rnpgbe_attr->name, sizeof(rnpgbe_attr->name),
			 "temp%u_input", offset + 1);
		break;
	case RNP_HWMON_TYPE_CAUTION:
		rnpgbe_attr->dev_attr.show = rnpgbe_hwmon_show_cautionthresh;
		snprintf(rnpgbe_attr->name, sizeof(rnpgbe_attr->name),
			 "temp%u_max", offset + 1);
		break;
	case RNP_HWMON_TYPE_MAX:
		rnpgbe_attr->dev_attr.show = rnpgbe_hwmon_show_maxopthresh;
		snprintf(rnpgbe_attr->name, sizeof(rnpgbe_attr->name),
			 "temp%u_crit", offset + 1);
		break;
	default:
		return -EPERM;
	}

	/* These always the same regardless of type */
	rnpgbe_attr->sensor = &adapter->hw.thermal_sensor_data.sensor[offset];
	rnpgbe_attr->hw = &adapter->hw;
	rnpgbe_attr->dev_attr.store = NULL;
	rnpgbe_attr->dev_attr.attr.mode = 0444;
	rnpgbe_attr->dev_attr.attr.name = rnpgbe_attr->name;

	sysfs_attr_init(&rnpgbe_attr->dev_attr.attr);

	adapter->rnpgbe_hwmon_buff->attrs[n_attr] = &rnpgbe_attr->dev_attr.attr;

	++adapter->rnpgbe_hwmon_buff->n_hwmon;

	return 0;
}
#endif /* RNP_HWMON */

#define to_net_device(n) container_of(n, struct net_device, dev)
static ssize_t maintain_read(struct file *filp, struct kobject *kobj,
			     struct bin_attribute *attr, char *buf, loff_t off,
			     size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	int rbytes = count;

	if (!adapter->maintain_buf)
		return 0;

	if (off + count > adapter->maintain_buf_len)
		rbytes = adapter->maintain_buf_len - off;
	memcpy(buf, adapter->maintain_buf + off, rbytes);

	if ((off + rbytes) >= adapter->maintain_buf_len) {
		kfree(adapter->maintain_buf);
		adapter->maintain_buf = NULL;
		adapter->maintain_buf_len = 0;
	}
	return rbytes;
}

static void n500_exchange_share_ram(struct rnpgbe_hw *hw, u32 *buf, int flag, int len)
{
	int i;
	struct rnpgbe_mbx_info *mbx = &hw->mbx;
	u32 addr = mbx->cpu_vf_share_ram;

	if (len > mbx->share_size)
		return;
	if (flag) {
		for (i = 0; i < len; i = i + 4)
			rnpgbe_wr_reg(hw->hw_addr + addr + i,
				      *(buf + i / 4));
	} else {
		for (i = 0; i < len; i = i + 4)
			*(buf + i / 4) = rnpgbe_rd_reg(hw->hw_addr + addr + i);
	}
}

static ssize_t maintain_write(struct file *filp, struct kobject *kobj,
			      struct bin_attribute *attr, char *buf, loff_t off,
			      size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	int err = -EINVAL;
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	struct rnpgbe_hw *hw = &adapter->hw;
	struct maintain_req *req;
	void *dma_buf = NULL;
	dma_addr_t dma_phy;
	int bytes;

	if (off == 0) {
		if (count < sizeof(*req))
			return -EINVAL;
		req = (struct maintain_req *)buf;
		if (req->magic != MAINTAIN_MAGIC)
			return -EINVAL;
		bytes = max_t(int, req->req_data_bytes, req->reply_bytes);
		bytes += sizeof(*req);

		kfree(adapter->maintain_buf);
		adapter->maintain_buf = NULL;
		adapter->maintain_buf_len = 0;

		dma_buf = dma_alloc_coherent(&hw->pdev->dev, bytes, &dma_phy,
					     GFP_ATOMIC);
		if (!dma_buf)
			return -ENOMEM;

		adapter->maintain_dma_buf = dma_buf;
		adapter->maintain_dma_phy = dma_phy;
		adapter->maintain_dma_size = bytes;
		adapter->maintain_in_bytes = req->req_data_bytes + sizeof(*req);

		memcpy(dma_buf + off, buf, count);

		if (count < adapter->maintain_in_bytes)
			return count;
	}

	dma_buf = adapter->maintain_dma_buf;
	dma_phy = adapter->maintain_dma_phy;
	req = (struct maintain_req *)dma_buf;

	memcpy(dma_buf + off, buf, count);

	/* all data got, send req */
	if ((off + count) >= adapter->maintain_in_bytes) {
		int reply_bytes = req->reply_bytes;
		int offset;
		struct rnpgbe_mbx_info *mbx = &hw->mbx;

		if (req->cmd) {
			int data_len;
			int ram_size = mbx->share_size;

			offset = 0;
			while (offset < req->req_data_bytes) {
				data_len = (req->req_data_bytes - offset) >
					ram_size ? ram_size :
					(req->req_data_bytes - offset);
				/* copy to ram */
				n500_exchange_share_ram(hw,
					(u32 *)(dma_buf + offset + sizeof(*req)),
					1, data_len);
				err = rnpgbe_maintain_req(hw,
					req->cmd, req->arg0, offset, 0, 0);
				if (err != 0)
					goto err_quit;

				offset += data_len;
			}
		} else {
			int data_len;
			int ram_size = mbx->share_size;
			struct maintain_reply reply;
			/* it is a read */
			adapter->maintain_buf =
				kmalloc(adapter->maintain_buf_len, GFP_KERNEL);
			if (!adapter->maintain_buf) {
				err = -ENOMEM;

				goto err_quit;
			}
			reply.magic = MAINTAIN_REPLY_MAGIC;
			reply.cmd = req->cmd;
			reply.arg0 = req->arg0;
			reply.data_bytes = req->reply_bytes;
			memcpy(adapter->maintain_buf, &reply,
			       sizeof(struct maintain_reply));
			/* copy req first */
			offset = 0;
			while (offset < reply_bytes) {
				data_len = (reply_bytes - offset) > ram_size ?
							 ram_size :
							 (reply_bytes - offset);
				err = rnpgbe_maintain_req(hw,
					req->cmd, req->arg0, 0, offset, 0);
				if (err != 0)
					goto err_quit;
				n500_exchange_share_ram(hw,
					(u32 *)(adapter->maintain_buf + offset +
					sizeof(*req)), 0, data_len);
				offset += data_len;
			}
		}
		if (dma_buf) {
			dma_free_coherent(&hw->pdev->dev,
					  adapter->maintain_dma_size, dma_buf,
					  dma_phy);
		}
		adapter->maintain_dma_buf = NULL;
	}

	return count;
err_quit:
	if (dma_buf) {
		dma_free_coherent(&hw->pdev->dev, adapter->maintain_dma_size,
				  dma_buf, dma_phy);
		adapter->maintain_dma_buf = NULL;
	}
	return err;
}

static BIN_ATTR(maintain, 0644, maintain_read, maintain_write, 1 * 1024 * 1024);
static ssize_t rx_desc_info_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	u32 rx_ring_num = adapter->sysfs_rx_ring_num;
	u32 rx_desc_num = adapter->sysfs_rx_desc_num;
	struct rnpgbe_ring *ring = adapter->rx_ring[rx_ring_num];
	int ret = 0;
	union rnpgbe_rx_desc *desc;

	desc = RNP_RX_DESC(ring, rx_desc_num);
	ret += sprintf(buf + ret, "rx ring %d desc %d:\n", rx_ring_num,
		       rx_desc_num);
	ret += print_desc(buf + ret, desc, sizeof(*desc));
	ret += sprintf(buf + ret, "\n");

	return ret;
}

static ssize_t rx_desc_info_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	int ret = count;

	u32 rx_desc_num = adapter->sysfs_rx_desc_num;
	u32 rx_ring_num = adapter->sysfs_rx_ring_num;

	struct rnpgbe_ring *ring = adapter->rx_ring[rx_ring_num];

	if (kstrtou32(buf, 0, &rx_desc_num) != 0)
		return -EINVAL;
	if (rx_desc_num < ring->count)
		adapter->sysfs_rx_desc_num = rx_desc_num;
	else
		ret = -EINVAL;

	return ret;
}

static ssize_t tcp_sync_info_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	int ret = 0;

	if (adapter->priv_flags & RNP_PRIV_FLAG_TCP_SYNC) {
		ret += sprintf(buf + ret,
			       "tcp sync remap on queue %d prio %s\n",
			       adapter->tcp_sync_queue,
			       (adapter->priv_flags & RNP_PRIV_FLAG_TCP_SYNC_PRIO) ?
			       "NO" : "OFF");
	} else {
		ret += sprintf(buf + ret, "tcp sync remap off\n");
	}

	return ret;
}

static ssize_t tcp_sync_info_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	struct rnpgbe_hw *hw = &adapter->hw;
	int ret = count;
	u32 tcp_sync_queue;

	if (kstrtou32(buf, 0, &tcp_sync_queue) != 0)
		return -EINVAL;

	if (tcp_sync_queue < adapter->num_rx_queues) {
		adapter->tcp_sync_queue = tcp_sync_queue;
		adapter->priv_flags |= RNP_PRIV_FLAG_TCP_SYNC;

		if (adapter->priv_flags & RNP_PRIV_FLAG_TCP_SYNC_PRIO) {
			hw->ops.set_tcp_sync_remapping(hw,
				adapter->tcp_sync_queue, true, true);
		} else {
			hw->ops.set_tcp_sync_remapping(hw,
				adapter->tcp_sync_queue, true, false);
		}

	} else {
		adapter->priv_flags &= ~RNP_PRIV_FLAG_TCP_SYNC;

		hw->ops.set_tcp_sync_remapping(hw, adapter->tcp_sync_queue,
					       false, false);
	}

	return ret;
}

static ssize_t rx_skip_info_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	int ret = 0;

	if (adapter->priv_flags & RNP_PRIV_FLAG_RX_SKIP_EN)
		ret += sprintf(buf + ret, "rx skip bytes: %d\n",
			       16 * (adapter->priv_skip_count + 1));
	else
		ret += sprintf(buf + ret, "rx skip off\n");

	return ret;
}

static ssize_t rx_drop_info_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	int ret = 0;

	ret += sprintf(buf + ret, "rx_drop_status %llx\n",
		       adapter->rx_drop_status);

	return ret;
}

static ssize_t rx_drop_info_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	struct rnpgbe_hw *hw = &adapter->hw;
	int ret = count;
	u64 rx_drop_status;

	if (kstrtou64(buf, 0, &rx_drop_status) != 0)
		return -EINVAL;

	adapter->rx_drop_status = rx_drop_status;

	hw->ops.update_rx_drop(hw);

	return ret;
}

static ssize_t outer_vlan_info_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	int ret = 0;

	if (adapter->priv_flags & RNP_PRIV_FLAG_DOUBLE_VLAN)
		ret += sprintf(buf + ret, "double vlan on\n");
	else
		ret += sprintf(buf + ret, "double vlan off\n");

	switch (adapter->outer_vlan_type) {
	case outer_vlan_type_88a8:
		ret += sprintf(buf + ret, "outer vlan 0x88a8\n");

		break;
	case outer_vlan_type_9100:
		ret += sprintf(buf + ret, "outer vlan 0x9100\n");

		break;
	case outer_vlan_type_9200:
		ret += sprintf(buf + ret, "outer vlan 0x9200\n");

		break;
	default:
		ret += sprintf(buf + ret, "outer vlan error\n");
		break;
	}
	return ret;
}

static ssize_t outer_vlan_info_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	struct rnpgbe_hw *hw = &adapter->hw;
	int ret = count;
	u32 outer_vlan_type;

	if (kstrtou32(buf, 0, &outer_vlan_type) != 0)
		return -EINVAL;
	if (outer_vlan_type < outer_vlan_type_max)
		adapter->outer_vlan_type = outer_vlan_type;
	else
		ret = -EINVAL;
	if (hw->ops.set_outer_vlan_type)
		hw->ops.set_outer_vlan_type(hw, outer_vlan_type);

	return ret;
}

static ssize_t tx_stags_info_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	int ret = 0;

	if (adapter->flags2 & RNP_FLAG2_VLAN_STAGS_ENABLED)
		ret += sprintf(buf + ret, "tx stags on\n");
	else
		ret += sprintf(buf + ret, "tx stags off\n");

	ret += sprintf(buf + ret, "vid 0x%x\n", adapter->stags_vid);

	return ret;
}

static ssize_t tx_stags_info_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	struct rnpgbe_hw *hw = &adapter->hw;

	struct rnpgbe_eth_info *eth = &hw->eth;
	int ret = count;
	u16 tx_stags;

	if (kstrtou16(buf, 0, &tx_stags) != 0)
		return -EINVAL;
	if (tx_stags < VLAN_N_VID)
		adapter->stags_vid = tx_stags;
	else
		ret = -EINVAL;

	eth->ops.set_vfta(eth, adapter->stags_vid, true);

	return ret;
}

static ssize_t gephy_test_info_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	int ret = 0;

	if (adapter->gephy_test_mode)
		ret += sprintf(buf + ret, "gephy_test on: %d\n",
			       adapter->gephy_test_mode);
	else
		ret += sprintf(buf + ret, "gephy_test off\n");

	return ret;
}

static ssize_t gephy_test_info_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	struct rnpgbe_hw *hw = &adapter->hw;
	int ret = count;
	u32 test_mode;

#define MAX_MODE (5)
	if (kstrtou32(buf, 0, &test_mode) != 0)
		return -EINVAL;
	if (test_mode < 5)
		adapter->gephy_test_mode = test_mode;
	else
		ret = -EINVAL;

	rnpgbe_mbx_gephy_test_set(hw, test_mode);

	return ret;
}

static ssize_t tx_desc_info_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	u32 tx_ring_num = adapter->sysfs_tx_ring_num;
	u32 tx_desc_num = adapter->sysfs_tx_desc_num;
	struct rnpgbe_ring *ring = adapter->tx_ring[tx_ring_num];
	int ret = 0;
	struct rnpgbe_tx_desc *desc;

	desc = RNP_TX_DESC(ring, tx_desc_num);
	ret += sprintf(buf + ret, "tx ring %d desc %d:\n", tx_ring_num,
		       tx_desc_num);
	ret += print_desc(buf + ret, desc, sizeof(*desc));
	ret += sprintf(buf + ret, "\n");

	return ret;
}

static ssize_t tx_desc_info_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	int ret = count;

	u32 tx_desc_num = adapter->sysfs_tx_desc_num;
	u32 tx_ring_num = adapter->sysfs_tx_ring_num;

	struct rnpgbe_ring *ring = adapter->tx_ring[tx_ring_num];

	if (kstrtou32(buf, 0, &tx_desc_num) != 0)
		return -EINVAL;
	if (tx_desc_num < ring->count)
		adapter->sysfs_tx_desc_num = tx_desc_num;
	else
		ret = -EINVAL;

	return ret;
}

static ssize_t rx_ring_info_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	u32 rx_ring_num = adapter->sysfs_rx_ring_num;
	struct rnpgbe_ring *ring = adapter->rx_ring[rx_ring_num];
	int ret = 0;
	union rnpgbe_rx_desc *rx_desc;

	ret += sprintf(buf + ret, "queue %d info:\n", rx_ring_num);

	ret += sprintf(buf + ret, "next_to_use %d\n", ring->next_to_use);
	ret += sprintf(buf + ret, "next_to_clean %d\n", ring->next_to_clean);

	rx_desc = RNP_RX_DESC(ring, ring->next_to_clean);
	ret += sprintf(buf + ret, "next_to_clean desc: ");
	ret += print_desc(buf + ret, rx_desc, sizeof(*rx_desc));
	ret += sprintf(buf + ret, "\n");

	return ret;
}

static ssize_t rx_ring_info_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	int ret = count;

	u32 rx_ring_num = adapter->sysfs_rx_ring_num;

	if (kstrtou32(buf, 0, &rx_ring_num) != 0)
		return -EINVAL;
	if (rx_ring_num < adapter->num_rx_queues)
		adapter->sysfs_rx_ring_num = rx_ring_num;
	else
		ret = -EINVAL;

	return ret;
}

static ssize_t mii_reg_info_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	int ret = count;

	u32 reg_num;

	if (kstrtou32(buf, 0, &reg_num) != 0)
		return -EINVAL;
	adapter->sysfs_mii_reg = reg_num;

	return ret;
}

static ssize_t mii_control_info_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	int ret = count;

	u32 reg_num;

	if (kstrtou32(buf, 0, &reg_num) != 0)
		return -EINVAL;
	adapter->sysfs_mii_control = reg_num;

	return ret;
}

static ssize_t mii_value_info_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	int ret = count;

	u32 reg_value;

	if (kstrtou32(buf, 0, &reg_value) != 0)
		return -EINVAL;
	adapter->sysfs_mii_value = reg_value;

	return ret;
}

static int rnpgbe_mdio_read(struct net_device *netdev, int prtad, int devad,
			    u32 addr, u32 *phy_value)
{
	int rc = -EIO;
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	struct rnpgbe_hw *hw = &adapter->hw;
	u16 value;

	rc = hw->ops.phy_read_reg(hw, addr, 0, &value);
	*phy_value = value;

	return rc;
}

static int rnpgbe_mdio_write(struct net_device *netdev, int prtad, int devad,
			     u16 addr, u16 value)
{
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	struct rnpgbe_hw *hw = &adapter->hw;

	return hw->ops.phy_write_reg(hw, addr, 0, value);
}

static ssize_t mii_info_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	u32 reg_num = adapter->sysfs_mii_reg;
	u32 reg_value = adapter->sysfs_mii_value;
	int ret = 0;
	u32 value;

	if (adapter->sysfs_mii_control) {
		rnpgbe_mdio_write(netdev, 0, 0, reg_num, reg_value);
		ret += sprintf(buf + ret, "write reg %x : %x\n", reg_num,
			       reg_value);

	} else {
		rnpgbe_mdio_read(netdev, 0, 0, reg_num, &value);
		ret += sprintf(buf + ret, "read reg %x : %x\n", reg_num, value);
	}

	return ret;
}

static ssize_t tx_ring_info_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	u32 tx_ring_num = adapter->sysfs_tx_ring_num;
	struct rnpgbe_ring *ring = adapter->tx_ring[tx_ring_num];
	int ret = 0;
	struct rnpgbe_tx_buffer *tx_buffer;
	struct rnpgbe_tx_desc *eop_desc;

	ret += sprintf(buf + ret, "queue %d info:\n", tx_ring_num);
	ret += sprintf(buf + ret, "next_to_use %d\n", ring->next_to_use);
	ret += sprintf(buf + ret, "next_to_clean %d\n", ring->next_to_clean);

	tx_buffer = &ring->tx_buffer_info[ring->next_to_clean];
	eop_desc = tx_buffer->next_to_watch;
	/* if have watch desc */
	if (eop_desc) {
		ret += sprintf(buf + ret, "next_to_watch:\n");
		ret += print_desc(buf + ret, eop_desc, sizeof(*eop_desc));
		ret += sprintf(buf + ret, "\n");
	} else {
		ret += sprintf(buf + ret, "no next_to_watch data\n");
	}

	return ret;
}

static ssize_t tx_ring_info_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	int ret = count;

	u32 tx_ring_num = adapter->sysfs_tx_ring_num;

	if (kstrtou32(buf, 0, &tx_ring_num) != 0)
		return -EINVAL;

	if (tx_ring_num < adapter->num_tx_queues)
		adapter->sysfs_tx_ring_num = tx_ring_num;
	else
		ret = -EINVAL;

	return ret;
}

static ssize_t queue_mapping_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int i;
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	struct rnpgbe_ring *ring;
	struct rnpgbe_q_vector *q_vector;

	ret += sprintf(buf + ret, "tx_queue count %d\n",
		       adapter->num_tx_queues);
	ret += sprintf(buf + ret, "queue-mapping :\n");
	for (i = 0; i < adapter->num_tx_queues; i++) {
		ring = adapter->tx_ring[i];
		ret += sprintf(buf + ret, "tx queue %d <---> ring %d\n", i,
			       ring->rnpgbe_queue_idx);
	}
	ret += sprintf(buf + ret, "rx_queue count %d\n",
		       adapter->num_rx_queues);
	ret += sprintf(buf + ret, "queue-mapping :\n");
	for (i = 0; i < adapter->num_rx_queues; i++) {
		ring = adapter->rx_ring[i];
		ret += sprintf(buf + ret, "rx queue %d <---> ring %d\n", i,
			       ring->rnpgbe_queue_idx);
	}
	ret += sprintf(buf + ret, "vector-queue mapping:\n");
	for (i = 0; i < adapter->num_q_vectors; i++) {
		q_vector = adapter->q_vector[i];
		ret += sprintf(buf + ret, "---vector %d---\n", i);
		rnpgbe_for_each_ring(ring, q_vector->tx) {
			ret += sprintf(buf + ret, "tx ring %d\n",
				       ring->rnpgbe_queue_idx);
		}
		rnpgbe_for_each_ring(ring, q_vector->rx) {
			ret += sprintf(buf + ret, "rx ring %d\n",
				       ring->rnpgbe_queue_idx);
		}
	}

	return ret;
}

static ssize_t queue_mapping_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	return count;
}

static ssize_t active_vid_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	u16 vid;
	u16 current_vid = 0;
	int ret = 0;
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	struct rnpgbe_hw *hw = &adapter->hw;
	u8 vfnum = hw->max_vfs - 1;

	if ((adapter->flags & RNP_FLAG_SRIOV_ENABLED)) {
		current_vid = rd32(hw, RNP_DMA_PORT_VEB_VID_TBL(adapter->port,
								vfnum));
	}

	for_each_set_bit(vid, adapter->active_vlans, VLAN_N_VID) {
		ret += sprintf(buf + ret, "%u%s ", vid,
			       (current_vid == vid ? "*" : ""));
	}
	ret += sprintf(buf + ret, "\n");
	return ret;
}

static ssize_t active_vid_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	u16 vid;
	int err = -EINVAL;
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	struct rnpgbe_hw *hw = &adapter->hw;
	u8 vfnum = hw->max_vfs - 1;

	if (!(adapter->flags & RNP_FLAG_SRIOV_ENABLED))
		return -EIO;

	if (kstrtou16(buf, 0, &vid) != 0)
		return -EINVAL;

	if (vid < 4096 && test_bit(vid, adapter->active_vlans)) {
		if (rd32(hw, RNP_DMA_VERSION) >= 0x20201231) {
			wr32(hw, RNP_DMA_PORT_VEB_VID_TBL(0, vfnum), vid);
		} else {
			wr32(hw, RNP_DMA_PORT_VEB_VID_TBL(adapter->port, vfnum),
			     vid);
		}
		err = 0;
	}

	return err ? err : count;
}

static ssize_t port_idx_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	int ret = 0;
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);

	ret += sprintf(buf, "%d\n", adapter->portid_of_card);
	return ret;
}

static DEVICE_ATTR_RO(port_idx);

static ssize_t pci_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	int err = -EINVAL;
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	struct rnpgbe_hw *hw = &adapter->hw;
	int gen = 3, lanes = 8;

	if (count > 30)
		return -EINVAL;

	if (sscanf(buf, "gen%dx%d", &gen, &lanes) != 2)
		return -EINVAL;

	if (gen > 3 || lanes > 8)
		return -EINVAL;

	err = rnpgbe_set_lane_fun(hw, LANE_FUN_PCI_LANE, gen, lanes, 0, 0);

	return err ? err : count;
}

static ssize_t pci_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int ret = 0;
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	struct rnpgbe_hw *hw = &adapter->hw;

	if (rnpgbe_mbx_get_lane_stat(hw) != 0)
		ret += sprintf(buf, " IO Error\n");
	else
		ret += sprintf(buf, "gen%dx%d\n", hw->pci_gen, hw->pci_lanes);

	return ret;
}
static DEVICE_ATTR_RW(pci);

static ssize_t temperature_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	struct rnpgbe_hw *hw = &adapter->hw;
	int ret = 0, temp = 0, voltage = 0;

	temp = rnpgbe_mbx_get_temp(hw, &voltage);

	ret += sprintf(buf, "temp:%d oC\n", temp);

	return ret;
}

static struct pci_dev *pcie_find_root_port_old(struct pci_dev *dev)
{
	while (1) {
		if (!pci_is_pcie(dev))
			break;
		if (pci_pcie_type(dev) == PCI_EXP_TYPE_ROOT_PORT)
			return dev;
		if (!dev->bus->self)
			break;
		dev = dev->bus->self;
	}
	return NULL;
}

static ssize_t root_slot_info_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct net_device *netdev = to_net_device(dev);
	struct rnpgbe_adapter *adapter = netdev_priv(netdev);
	int ret = 0;
	struct pci_dev *root_pdev = pcie_find_root_port_old(adapter->pdev);

	if (root_pdev) {
		ret += sprintf(buf + ret, "%02x:%02x.%x\n",
			       root_pdev->bus->number,
			       PCI_SLOT(root_pdev->devfn),
			       PCI_FUNC(root_pdev->devfn));
	}
	return ret;
}

static DEVICE_ATTR_RO(root_slot_info);
static DEVICE_ATTR_RO(temperature);
static DEVICE_ATTR_RW(active_vid);
static DEVICE_ATTR_RW(queue_mapping);
static DEVICE_ATTR_RW(tx_ring_info);
static DEVICE_ATTR_RO(mii_info);
static DEVICE_ATTR_WO(mii_reg_info);
static DEVICE_ATTR_WO(mii_control_info);
static DEVICE_ATTR_WO(mii_value_info);
static DEVICE_ATTR_RW(rx_ring_info);
static DEVICE_ATTR_RW(tx_desc_info);
static DEVICE_ATTR_RW(rx_desc_info);
static DEVICE_ATTR_RW(rx_drop_info);
static DEVICE_ATTR_RW(outer_vlan_info);
static DEVICE_ATTR_RW(tcp_sync_info);
static DEVICE_ATTR_RO(rx_skip_info);
static DEVICE_ATTR_RW(tx_stags_info);
static DEVICE_ATTR_RW(gephy_test_info);

static struct attribute *dev_attrs[] = {
	&dev_attr_tx_stags_info.attr,
	&dev_attr_gephy_test_info.attr,
	&dev_attr_root_slot_info.attr,
	&dev_attr_active_vid.attr,
	&dev_attr_queue_mapping.attr,
	&dev_attr_rx_drop_info.attr,
	&dev_attr_outer_vlan_info.attr,
	&dev_attr_tcp_sync_info.attr,
	&dev_attr_rx_skip_info.attr,
	&dev_attr_tx_ring_info.attr,
	&dev_attr_mii_info.attr,
	&dev_attr_mii_control_info.attr,
	&dev_attr_mii_reg_info.attr,
	&dev_attr_mii_value_info.attr,
	&dev_attr_rx_ring_info.attr,
	&dev_attr_tx_desc_info.attr,
	&dev_attr_rx_desc_info.attr,
	&dev_attr_port_idx.attr,
	&dev_attr_temperature.attr,
	&dev_attr_pci.attr,
	NULL,
};

static struct bin_attribute *dev_bin_attrs[] = {
	&bin_attr_maintain,
	NULL,
};

static struct attribute_group dev_attr_grp = {
	.attrs = dev_attrs,
	.bin_attrs = dev_bin_attrs,
};

static void
rnpgbe_sysfs_del_adapter(struct rnpgbe_adapter __maybe_unused *adapter)
{
}

/* called from rnpgbe_main.c */
void rnpgbe_sysfs_exit(struct rnpgbe_adapter *adapter)
{
	rnpgbe_sysfs_del_adapter(adapter);
	sysfs_remove_group(&adapter->netdev->dev.kobj, &dev_attr_grp);

	kfree(adapter->maintain_buf);
	adapter->maintain_buf = NULL;
	adapter->maintain_buf_len = 0;
}

/* called from rnpgbe_main.c */
int rnpgbe_sysfs_init(struct rnpgbe_adapter *adapter)
{
	int rc = 0;
	int flag;
#ifdef RNP_HWMON
	struct hwmon_buff *rnpgbe_hwmon;
	struct device *hwmon_dev;
	unsigned int i;
#endif /* RNP_HWMON */

	flag = sysfs_create_group(&adapter->netdev->dev.kobj, &dev_attr_grp);
	if (flag != 0) {
		dev_err(&adapter->netdev->dev,
			"sysfs_create_group failed:flag:%d\n", flag);
		return flag;
	}
#ifdef RNP_HWMON
	/* If this method isn't defined we don't support thermals */
	if (!adapter->hw.ops.init_thermal_sensor_thresh)
		goto no_thermal;

	/* Don't create thermal hwmon interface if no sensors present */
	if (adapter->hw.ops.init_thermal_sensor_thresh(&adapter->hw))
		goto no_thermal;

	rnpgbe_hwmon = devm_kzalloc(&adapter->pdev->dev, sizeof(*rnpgbe_hwmon),
				    GFP_KERNEL);

	if (!rnpgbe_hwmon) {
		rc = -ENOMEM;
		goto exit;
	}

	adapter->rnpgbe_hwmon_buff = rnpgbe_hwmon;

	for (i = 0; i < RNP_MAX_SENSORS; i++) {
		/* Only create hwmon sysfs entries for sensors that have
		 * meaningful data for.
		 */
		if (adapter->hw.thermal_sensor_data.sensor[i].location == 0)
			continue;

		/* Bail if any hwmon attr struct fails to initialize */
		rc = rnpgbe_add_hwmon_attr(adapter, i, RNP_HWMON_TYPE_CAUTION);
		if (rc)
			goto err;
		rc = rnpgbe_add_hwmon_attr(adapter, i, RNP_HWMON_TYPE_LOC);
		if (rc)
			goto err;
		rc = rnpgbe_add_hwmon_attr(adapter, i, RNP_HWMON_TYPE_TEMP);
		if (rc)
			goto err;
		rc = rnpgbe_add_hwmon_attr(adapter, i, RNP_HWMON_TYPE_MAX);
		if (rc)
			goto err;
	}

	rnpgbe_hwmon->groups[0] = &rnpgbe_hwmon->group;
	rnpgbe_hwmon->group.attrs = rnpgbe_hwmon->attrs;

	hwmon_dev = devm_hwmon_device_register_with_groups(
		&adapter->pdev->dev, "rnp", rnpgbe_hwmon, rnpgbe_hwmon->groups);

	if (IS_ERR(hwmon_dev)) {
		rc = PTR_ERR(hwmon_dev);
		goto exit;
	}

no_thermal:
#endif /* RNP_HWMON */
	goto exit;

err:
	rnpgbe_sysfs_exit(adapter);
exit:
	return rc;
}
