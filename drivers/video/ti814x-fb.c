/*
 * (C) Copyright 2019
 * Kron Technologies, <www.krontech.ca>
 * Owen Kirby <oskirby@gmail.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation's version 2 and any
 * later version the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <common.h>
#include "ti81xx-logo.h"

static int ti814x_prcm_enable_vps_power_and_clock(void)
{
	int repeat;
	u32 val;

	/* SW_WKUP: Start a software forced wake up transition on the domain */
	__raw_writel(0x02, CM_HDVPSS_CLKSTCTRL);
	/* wait for 10 microseconds before checking for power update */
	udelay(10);
	/* Check the power state after the wakeup transistion */
	for (repeat = 0; repeat < 5; repeat++) {
		val = __raw_readl(PM_HDVPSS_PWRSTST);
		if (val == 0x37)
			break;
		udelay(10);
	}
	if (repeat == 5)
		return -1;

	/* Enable HDVPSS Clocks */
	__raw_writel(0x02, CM_HDVPSS_HDVPSS_CLK_CTRL);
	/* Enable HDMI Clocks */
	__raw_writel(0x02, CM_HDVPSS_HDMI_CLKCTRL);
	for (repeat = 0; repeat < 5; repeat++) {
		val = __raw_readl(CM_HDVPSS_CLKSTCTRL);
		if ((val & 0x100) == 0x100)
			break;
		udelay(10);
	}
	if (repeat == 5)
		return -1;

	/* reset HDVPSS and HDMI */
	__raw_writel(0x04, RM_HDVPSS_RSTCTRL);
	udelay(10);
	__raw_writel(0x04, RM_HDVPSS_RSTST);
	udelay(10);
	/* release reset from HDVPSS and HDMI */
	__raw_writel(0x00, RM_HDVPSS_RSTCTRL);
	udelay(10);
	/* wait for SW reset to complete */
	for (repeat = 0; repeat < 5; repeat++) {
		val = __raw_readl(RM_HDVPSS_RSTST);
		if ((val & 0x4) == 0x4)
			break;
		udelay(10);
	}
	if (repeat == 5)
		return -1;

	/* put HDVPSS in ON State */
	val = __raw_readl(PM_HDVPSS_PWRSTCTRL);
	val |= 0x3;
	__raw_writel(val, PM_HDVPSS_PWRSTCTRL);
	/* wait 10 microseconds after powering on */
	udelay(10);
	/* check power status */
	for (repeat = 0; repeat < 5; repeat++) {
		val = __raw_readl(PM_HDVPSS_PWRSTST);
		if (val == 0x37)
			break;
		udelay(10);
	}
	if (repeat == 5)
		return -1;

	return 0;
}

static int ti814x_prcm_init(void)
{
	if (ti814x_prcm_enable_vps_power_and_clock() < 0)
		return -1;

	return 0;
}

/**
 * Program a PLL unit
 */
static void ti814x_pll_configure(u32 baseAddr, u32 n, u32 m, u32 m2, 
							      u32 clkCtrlValue)
{
	u32 m2nval, mn2val, clkctrl, clk_out, ref_clk, clkout_dco = 0;
	u32 status;

	m2nval = (m2 << 16) | n;
	mn2val =  m;
	ref_clk     = OSC_FREQ / (n+1);
	clkout_dco  = ref_clk * m;
	clk_out     = clkout_dco / m2;
	__raw_writel(m2nval, (baseAddr + M2NDIV));
	__raw_writel(mn2val, (baseAddr + MN2DIV));
	__raw_writel(0x1, (baseAddr + TENABLEDIV));
	__raw_writel(0x0, (baseAddr + TENABLEDIV));
	__raw_writel(0x1, (baseAddr + TENABLE));
	__raw_writel(0x0, (baseAddr + TENABLE));
	clkctrl = __raw_readl(baseAddr + CLKCTRL);
	clkctrl = (clkctrl & ~(7 << 10 | 1 << 23)) | clkCtrlValue;
	__raw_writel(clkctrl, baseAddr + CLKCTRL);
	do {
		status = __raw_readl(baseAddr + STATUS);
	} 
	while ((status & 0x00000600) != 0x00000600);
}

/**
 * Configure PLL for HDVPSS unit
 */
static void ti814x_pll_config_hdvpss(void)
{
	u32 rd_osc_src;
	rd_osc_src = __raw_readl(PLL_VIDEO2_PINMUX);
	rd_osc_src &= 0xFFFFFFFE;
	__raw_writel(rd_osc_src, PLL_VIDEO2_PINMUX);
	ti814x_pll_configure(PLL_HDVPSS_BASE, 19, 800, 4, 0x00000801);
}

/**
 * Initialize the PLLs
 */
static void ti814x_pll_init(void)
{
	ti814x_pll_config_hdvpss();
}

/**
 * Initialize HDVPSS unit
 */
static void ti814x_vps_init(void)
{
	/* enable clocks on all units */
	__raw_writel(0x01031fff, VPS_CLKC_ENABLE);
	__raw_writel(0x9000D, VPS_CLKC_VENC_CLK_SELECT);
	udelay(10);
	/* enable all video encoders */
	__raw_writel(0xD, VPS_CLKC_VENC_ENABLE);
	/* enable comp units HDMI/HDCOMP/DVO2 */
	__raw_writel(0 | (1 << 0) | (1 << 8) | (1 << 16), VPS_COMP_STATUS);
	/* set background color */
	__raw_writel(LOGO_BGCOLOR, VPS_COMP_BGCOLOR);
}

static int ti814x_pll_get_dividers(u32 req_out_clk, int hdmi, 
						struct pll_config_t* config)
{
	int32_t ret = -1;
	float ref_clk, dco_clk, clk_out;
	float best_delta;

	config->n = 0;
	config->m = 0;
	config->m2 = 0;
	config->clk_ctrl = 0;
	best_delta = 1E20;
	if(hdmi) {
		config->n = 19;
		config->m = 1485;
		config->m2 = 10;
		config->clk_ctrl = 0x200a1001;
		if(req_out_clk == 74250000) {
			config->n = 19;
			config->m = 742;
			config->m2 = 10;
			config->clk_ctrl = 0x20020801;
		}
		else if(req_out_clk == 65000000) {
			config->n = 19;
			config->m = 650;
			config->m2 = 10;
			config->clk_ctrl = 0x20020801;
		}
		else if(req_out_clk == 54000000) {
			config->n = 19;
			config->m = 540;
			config->m2 = 10;
			config->clk_ctrl = 0x20020801;
		}
		else if(req_out_clk == 27000000 ) {
			config->n = 19;
			config->m = 540;
			config->m2 = 2;
			config->clk_ctrl = 0x200A0801;
		}
		else if(req_out_clk == 33000000 ) {
			config->n = 19;
			config->m = 660;
			config->m2 = 2;
			config->clk_ctrl = 0x200A0801;
		}
		else if(req_out_clk == 148500000) {
			config->n = 7;
			config->m = 594;
			config->m2 = 10;
			config->clk_ctrl = 0x20021001;
		}
		ref_clk = 20E6f / (config->n + 1);
		dco_clk = ref_clk * config->m;
		clk_out = dco_clk / config->m2;
		ret = 0;
	}
	return 0;
}

/**
 * Configure PLL for HDMI
 */
static int ti814x_pll_config_hdmi(u32 freq)
{
	u32 rd_osc_src;
	struct pll_config_t config;

	rd_osc_src = __raw_readl(PLL_OSC_SRC_CTRL);
	__raw_writel((rd_osc_src & 0xfffbffff) | 0x0, PLL_OSC_SRC_CTRL);
	rd_osc_src = __raw_readl(PLL_VIDEO2_PINMUX);
	rd_osc_src &= 0xFFFFFFFE;
	__raw_writel(rd_osc_src, PLL_VIDEO2_PINMUX);
	if (ti814x_pll_get_dividers(freq, 1, &config) == -1)
		return -1;
	// LOIAL - configure vout0 pll as well as hdmi pll
	ti814x_pll_configure(PLL_VIDEO1_BASE, config.n, config.m, config.m2, 
							      config.clk_ctrl);
	ti814x_pll_configure(PLL_VIDEO2_BASE, config.n, config.m, config.m2, 
							      config.clk_ctrl);

	return 0;
}

/**
 * Enable HDMI output.
 */
static void ti814x_hdmi_enable(int freq)
{
	u32 temp, temp1;
	int i;

	/* wrapper soft reset */
	temp = __raw_readl(HDMI_REG_BASE + 0x0010) ;
	temp1 = ((temp & 0xFFFFFFFE) | 0x1);
	__raw_writel(temp1, HDMI_REG_BASE + 0x0300);
	temp = 0;
	udelay(10);

	/* configure HDMI PHY */
	/* 48 Mhz Clock input to HDMI ie sdio clock output from prcm */
	__raw_writel(0x2, PRCM_REG_BASE + 0x15B0);
	/* Power on the phy from wrapper */
	__raw_writel(0x8, HDMI_REG_BASE + 0x0040);
	for (i = 0;i < 1000; i++) { 
		if ((__raw_readl(HDMI_REG_BASE + 0x0040) & 0x00000003) == 2) 
			break;
		else 
			udelay(10);
	}

	__raw_writel(0x4A, HDMI_REG_BASE + 0x0040);
	for (i = 0;i < 1000; i++) { 
		if ((__raw_readl(HDMI_REG_BASE + 0x0040) & 0x000000FF ) == 0x5A)
			break;
		else 
			udelay(10);
	}

	__raw_writel(0x8A, HDMI_REG_BASE + 0x0040);
	for (i = 0;i < 1000; i++) { 
		if ((__raw_readl(HDMI_REG_BASE + 0x0040) & 0xFF) == 0xAA)
			break;
		else 
			udelay(10);
	}

	/* Dummy read to PHY base to complete the scp reset process */
	temp = __raw_readl(HDMI_REG_BASE + 0x0300);

	temp = __raw_readl(HDMI_REG_BASE + 0x0300);
	if(freq > 50000000)
		temp1 = ((temp & 0x3FFFFFFF) | (0x1 << 30));
	else
		temp1 = ((temp & 0x3FFFFFFF) | (0x0 << 30));
	__raw_writel(temp1, HDMI_REG_BASE + 0x0300);
	temp = __raw_readl(HDMI_REG_BASE + 0x030C);
	temp1 = ((temp & 0x000FFFFF) | 0x85400000);
	__raw_writel(temp1, HDMI_REG_BASE + 0x030C);
	__raw_writel(0xF0000000, HDMI_REG_BASE + 0x0304);
	udelay(10);
	/* cec clock divider config */
	temp = __raw_readl(HDMI_REG_BASE + 0x0070);
	temp1 = temp | 0x00000218;
	__raw_writel(temp1, HDMI_REG_BASE + 0x0070);

	/* wrapper debounce config */
	temp = __raw_readl(HDMI_REG_BASE + 0x0044);
	temp1 = temp | 0x00001414;
	__raw_writel(temp1, HDMI_REG_BASE + 0x0044);
	/* packing mode config */
	temp = __raw_readl(HDMI_REG_BASE + 0x0050);
	temp1 = temp | 0x105;
	__raw_writel(temp1, HDMI_REG_BASE + 0x0050);
	/* disable audio */
	__raw_writel(0x0, HDMI_REG_BASE + 0x0080);

	/* release hdmi core reset and release power down of core */
	__raw_writel(0x1, HDMI_REG_BASE + 0x0414);
	__raw_writel(0x1, HDMI_REG_BASE + 0x0424);
	/* video action  config of hdmi */
	__raw_writel(0x0, HDMI_REG_BASE + 0x0524);
	/* config input data bus width */
	__raw_writel(0x7, HDMI_REG_BASE + 0x0420);
	__raw_writel(0x0, HDMI_REG_BASE + 0x0528);  /* vid_mode */
	__raw_writel(0x1, HDMI_REG_BASE + 0x04CC);  /* data enable control */
	__raw_writel(0x37, HDMI_REG_BASE + 0x0420); /* enable vsync and hsync */
	__raw_writel(0x0, HDMI_REG_BASE + 0x04F8);  /* iadjust config to enable vsync */
	__raw_writel(0x10, HDMI_REG_BASE + 0x0520); /* csc is bt709 */
	__raw_writel(0x20, HDMI_REG_BASE + 0x09BC); /* enable dvi */
	__raw_writel(0x20, HDMI_REG_BASE + 0x0608); /* tmds_ctrl */
	__raw_writel(0x0, HDMI_REG_BASE + 0x0904);  /* disable n/cts of actrl */
	__raw_writel(0x0, HDMI_REG_BASE + 0x0950);  /* disable audio */
	__raw_writel(0x0, HDMI_REG_BASE + 0x0414);  /* keep audio  operation in reset state */
}

/**
 * Configure VENC unit
 */
static void ti814x_vps_configure_venc(u32 cfg_reg_base, int hdisp, int hsyncstart,
int hsyncend, int htotal, int vdisp, int vsyncstart, int vsyncend, int vtotal,
				int enable_invert, int hs_invert, int vs_invert)
{
	int av_start_h = htotal-hsyncstart;
	int av_start_v = vtotal-vsyncstart;
	int hs_width = hsyncend-hsyncstart;
	int vs_width = vsyncend-vsyncstart;

	/* clamp, lines (total num lines), pixels (total num pixels/line) */
	__raw_writel(0x84000000 | (vtotal << 12) | (htotal), cfg_reg_base + 0x28);
	/* hs_width, act_pix, h_blank-1 */
	__raw_writel((hs_width << 24) | (hdisp << 12) | (av_start_h - 1), cfg_reg_base + 0x30);
	/* vout_hs_wd, vout_avdhw, vout_avst_h */
	__raw_writel((hs_width << 24) | (hdisp << 12) | (av_start_h), cfg_reg_base + 0x3c);
	/* bp_pk_l (back porch peak), vout_avst_v1 (active video start field 1), vout_hs_st (hsync start) */
	__raw_writel((av_start_v << 12), cfg_reg_base + 0x40);
	/* bp_pk_h (back porch peak), vout_avst_vw (num active lines), vout_avst_v1 (active video start field 2) */
	__raw_writel((vtotal << 12), cfg_reg_base + 0x44);
	/* vout_vs_wd1, vout_vs_st1 (vsync start), vout_avd_vw2 (vs width field 2) */
	__raw_writel((vs_width << 24), cfg_reg_base + 0x48);
	/* osd_avd_hw (number of pixels per line), osd_avst_h */
	__raw_writel((hs_width << 24) | (hdisp << 12) | (av_start_h - 8), cfg_reg_base + 0x54);
	/* osd_avst_v1 (first active line), osd_hs_st (HS pos) */
	__raw_writel((av_start_v << 12), cfg_reg_base + 0x58);
	/* osd_avd_vw1 (number of active lines), osd_avst_v2 (first active line in 2nd field) */
	__raw_writel((vdisp << 12), cfg_reg_base + 0x5c);
	/* osd_vs_wd1 (vsync width), osd_vs_st1 (vsync start), osd_avd_vw2 */
	__raw_writel((vs_width<<24), cfg_reg_base + 0x60);
	/* osd_vs_wd2, osd_fid_st1, osd_vs_st2 */
	__raw_writel(0x0A004105, cfg_reg_base + 0x64);
	__raw_writel((enable_invert << 25)
		| (hs_invert << 24)
		| (vs_invert << 23)
		| (3 << 16) /* not sure what value 3 means, video out format: 10 bit, separate syncs */
		| (3 << 12) /* not sure what bit 12 is, 1 << 13 = bypass gamma correction */
		| (1 << 5)  /* bypass gamma correction */
		| (1 << 4)  /* bypass 2x upscale */
		| (1 << 0) /* 480p format */
		, cfg_reg_base);
	__raw_writel(__raw_readl(cfg_reg_base) | 0x40000000, cfg_reg_base + 0x00); /* start encoder */
}

static void ti814x_vps_configure_lcd(void) // hard coded
{
	// 0x4
	// LOIAL --- Linux Config
	// hdisp, hsyncstart, hsyncend, htotal, vdisp, vsyncstart, vsyncend, vtotal
	//   800,        842,      862,   1073,   481,        523,      533,    480
	// 0x28 - 0x8420B431       - vtotal = 523, htotal = 1073
	// 0x30 - 0x14328042       - hs_width = 20, hdisp = 808, av_start_h = 67
	// 0x3C - 0x14320045       -                hdisp = 800, av_start_h = 69
	// 0x40 - 0x0002A000       - av_start_v = 42
	// 0x44 - 0x001E0000       - vtotal = 480
	// 0x48 - 0x0A009000       - vs_width = 10  - unknown 9
	// 0x54 - 0x1432003D       - hs_width = 20, hdisp = 800, av_start_h = 69
	// 0x58 - 0x0002A001       - av_start_v = 42 - unknown 1
	// 0x5C - 0x001E1146       - vdisp = 481 - unknown 146
	// 0x60 - 0x0A001000       - vs_width = 10  - unknown 1
	// 0x64 - 0x0A004105       - unkown

	//__raw_writel(0x032001e0, 0x48105118); // LOIAL - configure the fb size, i think... 800x480
	__raw_writel(0x050002D0, 0x48105118); // LOIAL - PIP = 1280x720
	__raw_writel(0x050002D0, 0x48105104); // FB size = 1280x720

	__raw_writel((0x84<<24) | (523 << 12) | (1073), 0x4810A028); // 0x8420B431
	__raw_writel((20 << 24) | (808 << 12)         , 0x4810A030); // 0x14328042
	__raw_writel((20 << 24) | (800 << 12) | (  69), 0x4810A03C); // 0x14320045
	__raw_writel(             ( 42 << 12) | (   0), 0x4810A040); // 0x0002A000
	__raw_writel(             (480 << 12) | (   0), 0x4810A044); // 0x001E0000
	__raw_writel((10 << 24) | (  9 << 12) | (   0), 0x4810A048); // 0x0A009000
	__raw_writel((20 << 24) | (800 << 12) | (  61), 0x4810A054); // 0x1432003D
	__raw_writel(             ( 42 << 12)         , 0x4810A058); // 0x0002A001
	__raw_writel(             (480 << 12) | (   0), 0x4810A05C);
	__raw_writel(                                0, 0x4810A060);
	__raw_writel(             (  4 << 12)         , 0x4810A064);

	__raw_writel(0x04032020, 0x4810A000);
	__raw_writel(0x44032020, 0x4810A000);
}

/* Change pin mux */
static void ti814x_pll_hdmi_setwrapper_clk(void)
{
        u32 rd_osc_src;
        rd_osc_src = __raw_readl(PLL_VIDEO2_PINMUX);
        rd_osc_src |= 0x1;
        __raw_writel(rd_osc_src, PLL_VIDEO2_PINMUX);
}

static int ti814x_set_mode(int dispno,int xres, int yres)
{
	// lots of LOIAL here
	//#ifdef CONFIG_480P
	//	/*modeline "720x480" 27.000 720 736 798 858 480 489 495 525 -hsync -vsync*/
	//	if (ti814x_pll_config_hdmi(27000000) == -1)
	//		return -1;
	//	ti814x_hdmi_enable(27000000);
	//        ti814x_vps_configure_venc(VPS_REG_BASE + 0x6000, 720, 736, 798, 858, 
	//        					   480, 489, 495, 525, 0, 1, 1);
	//#else		
	//	/* ModeLine "1920x1080" 148.50 1920 2008 2052 2200 1080 1084 1088 1125 +HSync +VSync */
	//	if (ti814x_pll_config_hdmi(148500000) == -1)
	//		return -1;
	//	ti814x_hdmi_enable(148500000);
	//	ti814x_vps_configure_venc(VPS_REG_BASE + 0x6000, 1920, 2008, 2052, 2200, 1080, 1084, 1088, 1125, 0, 0, 0);
	//#endif
	if (ti814x_pll_config_hdmi(74250000) == -1) // was 33000000
		return -1;
	ti814x_hdmi_enable(74250000); // was 33000000
	// configure HDMI
	//ti814x_vps_configure_venc(VPS_REG_BASE + 0x6000, 800, 1004, 1053, 1073, 480, 490, 513, 523, 0, 0, 0);

	/* Modeline "1280x720p_at60Hz" 74.250 1280 1390 1430 1650 720 725 730 750 +hsync +vsync */
	ti814x_vps_configure_venc(VPS_REG_BASE + 0x6000, 1280, 1390, 1430, 1650, 720, 725, 730, 750, 0, 0, 0);

	//configure dvo2  - LOIAL
	// 33500,800/204/49/20,480/10/23/10,1   - LOIAL not sure if these are right
	//ti814x_vps_configure_venc(VPS_REG_BASE + 0xA000, 800, 800+204, 800+204+49, 800+204+49+20, 480, 480+10, 480+10+23, 480+10+23+10, 0, 1, 1); // LOIAL
	//       800,        842,      862,   1073,   481,        523,      533,    480
	// configure DVO2
	// disabled as i've hardcoded in the above command - ti814x_vps_configure_venc(VPS_REG_BASE + 0xA000, 800, 842, 862, 1073, 480, 522, 532, 542, 1, 0, 0); // LOIAL

	ti814x_vps_configure_lcd();
  
	ti814x_pll_hdmi_setwrapper_clk();
  
	return 0;
}

void ti814x_set_board()
{
	if(ti814x_prcm_init() == -1)
        	printf("ERROR: ti814x prcm init failed\n");
       
	ti814x_pll_init();

	ti814x_vps_init();

	if (ti814x_set_mode(1, WIDTH, HEIGHT) == -1)
		printf("ERROR: ti814x setting the display failed\n");
}
