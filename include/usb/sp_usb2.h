#ifndef __USB2_SP_H__
#define __USB2_SP_H__

#define REG_BASE		0xf8000000
#define REG_BASE_AO		0xf8800000

#define RF_GRP(_grp, _reg) ((((_grp) * 32 + (_reg)) * 4) + REG_BASE)
#define RF_GRP_AO(_grp, _reg) ((((_grp) * 32 + (_reg)) * 4) + REG_BASE_AO)

#define RF_MASK_V(_mask, _val)       (((_mask) << 16) | (_val))
#define RF_MASK_V_SET(_mask)         (((_mask) << 16) | (_mask))
#define RF_MASK_V_CLR(_mask)         (((_mask) << 16) | 0)

// usb spec 2.0 Table 7-3  VHSDSC (min, max) = (525, 625)
// default = 576.3 mV (374 + 7 * 28.9)
#define DEFAULT_UPHY_DISC	0x7   // 7 (= 576.3mv)
#define DEFAULT_SQ_CT		0x3

extern int clk_usbc0_cnt;

struct uphy_rn_regs {
	u32 cfg[28];		       // 150.0
	u32 gctrl[3];		       // 150.28
	u32 gsts;		       // 150.31
};
#define UPHY0_RN_REG ((volatile struct uphy_rn_regs *)RF_GRP(149, 0))
#define UPHY1_RN_REG ((volatile struct uphy_rn_regs *)RF_GRP(150, 0))

struct moon0_regs {
	unsigned int stamp;            // 0.0
	unsigned int reset[15];        // 0.1 -  0.12
	unsigned int rsvd[15];         // 0.13 - 0.30
	unsigned int hw_cfg;           // 0.31
};
#define MOON0_REG ((volatile struct moon0_regs *)RF_GRP_AO(0, 0))

struct moon1_regs {
	unsigned int sft_cfg[32];
};
#define MOON1_REG ((volatile struct moon1_regs *)RF_GRP_AO(1, 0))

struct moon2_regs {
	unsigned int sft_cfg[32];
};
#define MOON2_REG ((volatile struct moon2_regs *)RF_GRP_AO(2, 0))

struct moon3_regs {
	unsigned int sft_cfg[32];
};
#define MOON3_REG ((volatile struct moon3_regs *)RF_GRP_AO(3, 0))

struct moon4_regs {
	unsigned int sft_cfg[32];
};
#define MOON4_REG ((volatile struct moon4_regs *)RF_GRP_AO(4, 0))

struct moon5_regs {
	unsigned int sft_cfg[32];
};
#define MOON5_REG ((volatile struct moon5_regs *)RF_GRP_AO(5, 0))

struct hb_gp_regs {
	unsigned int hb_otp_data[8];
	unsigned int reserved_8[24];
};
#define OTP_REG	((volatile struct hb_gp_regs *)RF_GRP_AO(71, 0))

#endif /* __USB2_SP_H__ */
