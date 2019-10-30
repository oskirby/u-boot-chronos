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
#include <video_fb.h>
#include "ti81xx-logo.h"

/* Hack for now... */
#define _VPSHAL_VPDMA_FIRMWARE_H_286
#include "ti81xx_vpdma_firmware.h"

static void vps_reset_module(enum vps_module_t module, int assert_reset)
{
	u32 val = __raw_readl(VPS_CLKC_RESET);

	if (assert_reset)
		val |= (1 << module);
	else
		val &= ~(1 << module);

	__raw_writel(val, VPS_CLKC_RESET);
}

/**
 * Load firmware for VPDMA.
 */
static int vpdma_load_firmware(u32* desc_buffer)
{
	u32* firmware;
	u32 status;
	int repeat;
	u32 vpdma_firmware_size = sizeof(vpdma_firmware);

	/* allocate physical contiguous memory for firmware buffer */
	firmware = (u32*) desc_buffer;
	if (!firmware) {
		printf("unable to allocate memory for firmware\n");
		return -1;
	}
	/* copy firmware to buffer */
	memcpy(firmware, vpdma_firmware, vpdma_firmware_size);
	/* Reset VPDMA module */
	vps_reset_module(VPS_MODULE_VPDMA, 1);
	udelay(10);
	vps_reset_module(VPS_MODULE_VPDMA, 0);

	/* load firmware */
	__raw_writel((u32)firmware, VPDMA_LIST_ADDR);

	/* wait till firmware is loaded */
	for (repeat = 0; repeat < 100; repeat++) {
		status = __raw_readl(VPDMA_LIST_ATTR);
		if ((status & 0x80000) == 0x80000)
			break;
		else
			udelay(10);
	}

	if (repeat == 100)
		return -1;

	return 0;
}

static u32 dispmgr_create_grpx_conf_descriptor(int grpx_unit, u32* desc_buffer, 
					u32* payload_buffer, u32 xres, u32 yres)
{
	u32 size = 0;
	u32 *desc;
	u32 *payload;

	/* set up frame configuration descriptor */
	desc = desc_buffer;
	desc[0] = 0x1;                  /* frame configuration address */
	desc[1] = 16;                   /* data length */
	desc[2] = (u32)payload_buffer;  /* payload location */
	desc[3] = 0
		| (0xB << 27)           /* packet type */
		| (0 << 26)             /* indirect command */
		| (1 << 24)             /* block type */
		| ((grpx_unit + 1) << 16)  /* destination GRPX0..GRPX2 */
		| (1 << 0);             /* payload size in 128 bit units */
	size += 16;
	payload = payload_buffer;
	payload[0] = 0;                 /* reserved */
	payload[1] = 0
		| (xres << 16)          /* frame width */
		| (yres << 0);          /* frame height */
	payload[2] = 0;                 /* reserved */
	payload[3] = 0
		| (0 << 30)		/* progressive format */
		| (1 << 31);		/* soft reset of GRPX unit */

	return size;
}

static void vpdma_send_list(u32* desc_buffer, int desc_len, int list_no)
{
	if ((__raw_readl(VPDMA_LIST_ATTR) & 0x80000) != 0x80000) {
		printf("WARNING : vpdma_send_list while vpdma busy");
	}
	/* convert descriptor size from byte units to 128-bit units */
	desc_len = (desc_len+15) >> 4;

	/* send list */
	__raw_writel((u32)desc_buffer, VPDMA_LIST_ADDR);
	__raw_writel((list_no << 24)               /* list number */
		| (0 << 16)                        /* list type: normal */
		| (desc_len << 0)                  /* list size */
		, VPDMA_LIST_ATTR);
}

int dispmgr_wait_for_list_complete(int repeat_count)
{
	int repeat;

	for (repeat = 0; repeat < repeat_count; repeat++) {
		if ((__raw_readl(VPDMA_LIST_ATTR) & 0x80000) == 0x80000)
			return 0;
		else
			udelay(10);
	}
	return -1;
}

static u32 dispmgr_setup_layers(u32* desc_buffer, int stride,int width, int height, u32 paddr, u32 grpx, u32 peripheral)
{
	u32 size = 0;
	u32* desc;
	int chan;
	int x = 0;
	int y = 0;
	int i, time = 0 , num_desc=0;
	char *s;
	u32 layer_ctrl;
	
	desc = desc_buffer ;
	layer_ctrl = 0x0000E400; // this sets layers to be 3,2,1,0 order

	/*
	 * set up data descriptors for GRPX units
	 */

	if      (grpx == GRPX0) layer_ctrl |= 1 << (2);  /* enable GRPXx layer */
	else if (grpx == GRPX1) layer_ctrl |= 1 << (1);  /* enable GRPXx layer */
	else                    layer_ctrl |= 1 << (0);  /* enable GRPXx layer */

	if      (grpx == GRPX0) chan = GRPX0_CHANNEL_NUMBER;
	else if (grpx == GRPX1) chan = GRPX1_CHANNEL_NUMBER;
	else                    chan = GRPX2_CHANNEL_NUMBER;

	if ((s = getenv("logotime")) != NULL) {
		time = simple_strtol(s, NULL, 10);
		
    		if(time > MAX_LOGO_TIME) {
			printf("Value of logotime is large, setting to default value\n");
			time = DEFAULT_LOGO_TIME;
		}
	}
	else {
		time = DEFAULT_LOGO_TIME;
	}

	num_desc = time * LOGO_TIME_MULTIPLIER;
	
	
	/* set up data transfer descriptor */
	for (i = 0; i < num_desc; i++) {
		desc[0] = 0
			| (DATA_TYPE << 26) /* RGB-888 */
			| (0 << 25)         /* no notification */
			| (0 << 24)         /* field number */
			| (0 << 23)         /* 2-dimensional data */
			| (0 << 20)         /* +1 line skip (even) */
			| (0 << 16)         /* +1 line skip (odd) */
			| (stride);         /* line stride */
		desc[1] = 0
			| (width << 16)     /* line length in pixels */
			| (height);         /* number of rows*/
		desc[2] = paddr;
		desc[3] = 0
			| (0xA << 27)       /* packet type */
			| (0 << 26)         /* Mode (normal or tiled) */
			| (0 << 25)         /* inbound direction */
			| (chan << 16)      /* channel */
			| (0 << 9)          /* priority */
			| (chan << 0);      /* next channel */
		desc[4] = 0
			| (width << 16)     /* region width */
			| (height);         /* region height */
		desc[5] = 0
			| (x << 16)         /* horizontal start */
			| (y << 0);         /* vertical start */
		desc[6] = 0
			| (1 << 7)          /* first region */
			| (1 << 8);         /* last region */
		desc[7] = 0;
		desc += 8;
		size += 32;
	}
	__raw_writel(layer_ctrl, peripheral);
	//__raw_writel(layer_ctrl, VPS_COMP_HDMI_VOUT1);
	if (peripheral == VPS_COMP_DVO2 || peripheral == VPS_COMP_HDMI_VOUT0) {
	  __raw_writel(layer_ctrl, VPS_COMP_HDMI_VOUT0); // LOIAL
	  __raw_writel(layer_ctrl, VPS_COMP_DVO2); // LOIAL
	}

	return size;
}

/*
 * The Graphic Device
 */
GraphicDevice ti81xx_device;

void *video_hw_init()
{
	u32* desc_buffer;
	u32* lcd_desc_buffer;
	u32* lcd_payload_buffer;
	u32 size;
	u32 paddr;

	/* initialize the Graphic Device structure */

	/* frame buffer for displaying the image */
	ti81xx_device.frameAdrs = FRAME_BUFFER_ADDRESS;

	/* width according to the resolution */
	ti81xx_device.winSizeX = WIDTH;

	/* height according to the resolution */
	ti81xx_device.winSizeY = HEIGHT;

	/* support for 24 bit image */
	ti81xx_device.gdfIndex = GDF_24BIT_888RGB;
	ti81xx_device.gdfBytesPP = 3;

	/* memory address for loading vpdma firmware and descriptors */
	desc_buffer = (u32*) (VPDMA_DESC_BUFFER);

	/* set the board for displaying the logo */
	SET_BOARD();

	/* clear the frame buffer for with the backgroung color */
	memset((void *) FRAME_BUFFER_ADDRESS, LOGO_BGCOLOR, FRAME_SIZE);

	if (vpdma_load_firmware(desc_buffer) == -1)
		printf("\nERROR: loading of firmware failed");

	printf("\nfirmware : about to make descriptors!");

	paddr = ti81xx_device.frameAdrs;

	// ------------------ Buffer Locations ----------------
	lcd_desc_buffer = (u32*) (VPDMA_DESC_BUFFER + 0x0000);
	lcd_payload_buffer = (u32*) (VPDMA_DESC_BUFFER + 0x4000);

	// ------------------ LCD -----------------------------

	printf("\nfirmware : sending LCD grpx descriptor");

	/* Allocate physical memory for MV data */
	size = dispmgr_create_grpx_conf_descriptor(GRPX1, lcd_desc_buffer, lcd_payload_buffer, 800, 480);
	vpdma_send_list(lcd_desc_buffer, size, 2);

	/* wait for list complete interrupt */
	if(dispmgr_wait_for_list_complete(100) == -1)
		printf("\nERROR : wait for list complete FAILED!");

	__raw_writel((7 << 10), VPDMA_GRPX1_DATA_CSTAT);

	printf("\nfirmware : sending LCD layers descriptor");
	size = dispmgr_setup_layers(lcd_desc_buffer, WIDTH*3, 800, 480 + 20, paddr, GRPX1, VPS_COMP_HDMI_VOUT0); // VPS_COMP_DVO2
	vpdma_send_list(lcd_desc_buffer, size, 3);

	if (dispmgr_wait_for_list_complete(10000) == -1)
		printf("\nERROR : LCD wait for list complete FAILED!");

	printf("\nfirmware : finished fb descriptors\n");

	/* return the Graphic device structure */	
	return (&ti81xx_device);
}

void video_set_lut (unsigned int index,	/* color number */
	       unsigned char r,	/* red */
	       unsigned char g,	/* green */
	       unsigned char b	/* blue */
	       )
{
}
