/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2024 Huawei Technologies Co., Ltd */

#ifndef ROCE_PUB_H
#define ROCE_PUB_H

#include "rdma_context_format.h"


/* ********************** sync info ************************ */
#define ROCE_SQC_RST_STATE (0x0)
#define ROCE_SQAC_RST_STATE (0x0)
#define ROCE_RQC_RST_STATE (0x0)
#define ROCE_RCC_RST_STATE (0x0)
#define ROCE_RRWC_RST_STATE (0x0)

#define ROCE_QPC_DELED_STATE (0xCE)
#define ROCE_RC_KICKOFF_VALUE (0x5a5acece)
#define ROCE_CACHE_LINE_SIZE (0x100)

#define ROCE_CQC_MID_STATE_VALUE (0xe)
#define ROCE_CQC_MID_STATE (0xe0000000)

#define ROCE_CPU_ENDIAN_LITTLE 0

#define ROCE_MAX_ARM_SN_NUM 4

#define ROCE_MKEY_MPT_IDX_OFF (8)
#define ROCE_MKEY_GET_MPT_IDX(mkey) ((mkey) >> ROCE_MKEY_MPT_IDX_OFF)

#define ROCE_PAGE_SIZE 4096
#define ROCE_RC_ENTRY_NUM_PER_CACELINE 8
#define ROCE_MTT_BUFLEN_TOTAL(mtt_num) ((mtt_num) << 3)

#define ROCE_SQWQE_CI_MASK (0xffff)

#define ROCE_GID_LEN 16

#define ROCE_LOCAL_DEFAULT_PMTU 5
#define ROCE_LOCAL_DEFAULT_MTU_CODE 0xf

enum {
	ROCE_QP_ST_RC = 0x0,  /* 000 */
	ROCE_QP_ST_UC = 0x1,  /* 001 */
	ROCE_QP_ST_RD = 0x2,  /* 010 */
	ROCE_QP_ST_UD = 0x3,  /* 011 */
	ROCE_QP_ST_XRC = 0x6, /* 110 */
	ROCE_QP_ST_PRIV = 0x7 /* 111 */
};
/* ********************** Macro Definition ************************ */

enum ROCE_SERVICE_TYPE_E {
	ROCE_RC_TYPE = 0,
	ROCE_UC_TYPE,
	ROCE_RD_TYPE,
	ROCE_UD_TYPE,
	ROCE_RSVD_TYPE,
	ROCE_XRC_5_TYPE = 0x5,
	ROCE_XRC_TYPE = 0x6,
	ROCE_XRC_6_TYPE = 0x6
};

enum ROCE_RC_FLAG_E {
	ROCE_UC_UD = 0,
	ROCE_RC_XRC
};

/* Description of parameters in the *********ROCE table ******* */
/* the same with stp */
enum ROCE_GID_TYPE_E {
	ROCE_GID_TYPE_V2_IPV4 = 0,
	ROCE_GID_TYPE_V2_IPV6 = 1,
	ROCE_GID_TYPE_V1 = 2
};

enum ROCE_SQWQE_MSGTYPE_E {
	ROCE_TX_MSGTYPE_SEND = 0,
	ROCE_TX_MSGTYPE_WRITE,
	ROCE_TX_MSGTYPE_READ,
	ROCE_TX_MSGTYPE_ATOMIC,
	ROCE_TX_MSGTYPE_LOCAL,
	ROCE_TX_MSGTYPE_RESIZECQ = 5,
	ROCE_TX_MSGTYPE_RSVD6 = 6,
	ROCE_TX_MSGTYPE_ERR = 7
};

enum ROCE_SQWQE_OPTYPE_E {
	ROCE_TX_SEND = 0,
	ROCE_TX_SEND_INVALIDATE,
	ROCE_TX_SEND_IMMEDIATE,
	ROCE_TX_OPTYPE_RSVD3,

	ROCE_TX_WRITE = 4,
	ROCE_TX_WRITE_IMMEDIATE,
	ROCE_TX_OPTYPE_RSVD6,
	ROCE_TX_OPTYPE_RSVD7,

	ROCE_TX_READ = 8,
	ROCE_TX_OPTYPE_RSVD9,
	ROCE_TX_OPTYPE_RSVD10,
	ROCE_TX_EXT_ATOMIC_COMPARE_SWAP,

	ROCE_TX_ATOMIC_COMPARE_SWAP = 0xc,
	ROCE_TX_ATOMIC_FETCH_ADD,
	ROCE_TX_ATOMIC_MASKED_COMPARE_SWAP,
	ROCE_TX_ATOMIC_MASKED_FETCH_ADD,

	ROCE_FAST_REG_PMR = 0x10,
	ROCE_LOCAL_INVALIDATE,
	ROCE_BIND_MW_TYPE1_TYPE2,
	ROCE_REG_SIG_MR,
	ROCE_LOCAL_EXT,
	ROCE_TX_OPTYPE_RSVD15,
	ROCE_RESIZE_TYPE = 0x16,
	ROCE_TX_OPTYPE_RSVD17,
	ROCE_TX_OPTYPE_RSVD18,
	ROCE_TX_OPTYPE_RSVD19,
	ROCE_TX_OPTYPE_RSVD1A,
	ROCE_TX_OPTYPE_RSVD1B,
	ROCE_TX_OPTYPE_RSVD1C,
	ROCE_TX_OPTYPE_RSVD1D,
	ROCE_ERR_TYPE = 0x1e,
	ROCE_TX_OPTYPE_RSVD1F
};

enum ROCE_SQPC_STATE_E {
	ROCE_SQPC_STATE_SW = 0,
	ROCE_SQPC_STATE_ERR = 1,
	ROCE_SQPC_STATE_HW = 0xF
};

enum ROCE_SRQC_STATE_E {
	ROCE_SRQC_STATE_SW = 0,
	ROCE_SRQC_STATE_ERR = 1,
	ROCE_SRQC_STATE_HW = 0xF
};

enum ROCE_CQC_STATE_E {
	ROCE_CQC_STATE_SW = 0,
	ROCE_CQC_STATE_ERR = 1,
	ROCE_CQC_STATE_OVERFLOW = 2,
	ROCE_CQC_STATE_HW = 0xF
};

enum ROCE_MPT_STATE_E {
	ROCE_MPT_STATE_VALID = 1,
	ROCE_MPT_STATE_FREE = 3,
	ROCE_MPT_STATE_ABORT = 4,
	ROCE_MPT_STATE_INVALID = 0xF
};

enum ROCE_MPT_R_W_E {
	ROCE_MPT_MW = 0,
	ROCE_MPT_MR
};

enum ROCE_MPT_BQP_E {
	ROCE_MPT_BQP_TYPE1 = 0,
	ROCE_MPT_BQP_TYPE2B
};

enum ROCE_MPT_DIF_MODE_E {
	ROCE_MPT_DIF_MODE_INS = 0,
	ROCE_MPT_DIF_MODE_DEL,
	ROCE_MPT_DIF_MODE_FWD
};

enum ROCE_MPT_SECTOR_SIZE_E {
	ROCE_MPT_SECTOR_SIZE_512 = 0,
	ROCE_MPT_SECTOR_SIZE_4KB
};

enum ROCE_MPT_METEDATA_SIZE_E {
	ROCE_MPT_METEDATA_SIZE_8 = 0,
	ROCE_MPT_METEDATA_SIZE_64
};


enum ROCE_MPT_SGL_MODE_E {
	ROCE_MPT_SGL_MODE_SINGLE = 0,
	ROCE_MPT_SGL_MODE_DOUBLE
};

enum ROCE_QPC_STATE_E {
	ROCE_QPC_STATE_RST = 0,
	ROCE_QPC_STATE_INIT,
	ROCE_QPC_STATE_RTR,
	ROCE_QPC_STATE_RTS,
	ROCE_QPC_STATE_SQD,
	ROCE_QPC_STATE_SQEr,
	ROCE_QPC_STATE_ERR,
	ROCE_QPC_STATE_DRAINING,
	ROCE_QPC_STATE_RSV
};


enum ROCE_MTT_LAYER_NUM_E {
	ROCE_MTT_LAYER_NUM_0 = 0,
	ROCE_MTT_LAYER_NUM_1,
	ROCE_MTT_LAYER_NUM_2,
	ROCE_MTT_LAYER_NUM_3,
	ROCE_MTT_LAYER_NUM_RSVD
};

enum ROCE_CQE_TYPE_E {
	/* *1-Send Completion; 0-Receive Completion */
	ROCE_RQ_CQE = 0,
	ROCE_SQ_CQE
};

enum ROCE_CQE_SIZE_E {
	ROCE_CQE_SIZE_0 = 0, /* * 4B */
	ROCE_CQE_SIZE_1 = 1, /* * 8B */
	ROCE_CQE_SIZE_2 = 2, /* * 16B */
	ROCE_CQE_SIZE_3 = 3  /* * 32B, RoCE fix 3 */
};

enum ROCE_CQE_RQ_OPTYPE_E {
	ROCE_CQE_RQ_OPTYPE_WRITE_IMMEDIATE = 0,
	ROCE_CQE_RQ_OPTYPE_SEND,
	ROCE_CQE_RQ_OPTYPE_SEND_IMMEDIATE,
	ROCE_CQE_RQ_OPTYPE_SEND_INVALIDATE,
	ROCE_CQE_RQ_OPTYPE_WRITE,
	ROCE_CQE_RQ_OPTYPE_READ
};

enum ROCE_CQE_COMMON_OPTYPE_E {
	ROCE_CQE_COMMON_OPTYPE_RESIZE = 0x16,
	ROCE_CQE_COMMON_OPTYPE_ERR = 0x1e
};

enum ROCE_QPC_MTU_E {
	ROCE_PMTU_256B = 1,
	ROCE_PMTU_512B = 2,
	ROCE_PMTU_1KB = 3,
	ROCE_PMTU_2KB = 4,
	ROCE_PMTU_4KB = 5
};

enum {
	ROCE_DB_MTU_256B_SHIFT = 0,
	ROCE_DB_MTU_512B_SHIFT = 1,
	ROCE_DB_MTU_1K_SHIFT = 2,
	ROCE_DB_MTU_2K_SHIFT = 3,
	ROCE_DB_MTU_4K_SHIFT = 4,
};

enum ROCE_FLOW_ID_E {
	ROCE_STL_TIMER_FLOW = 0,
	ROCE_PKT_FLOW = 1,
	ROCE_DB_FLOW = 2,
	ROCE_TIMER_FLOW = 3,
	ROCE_ARM_FLOW = 4,
	ROCE_CMDQ_FLOW = 5,
	ROCE_NRET_FLOW = 6,
	ROCE_STL2STF_FLOW = 7,
};

#endif /* ROCE_PUB_H */
