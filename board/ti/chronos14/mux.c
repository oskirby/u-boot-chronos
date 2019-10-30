/*
 * mux.c
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <common.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/hardware.h>
#include <asm/arch/mux.h>
#include <asm/io.h>
#include <i2c.h>
#include "evm.h"

static struct module_pin_mux uart0_pin_mux[] = {
	{OFFSET(pincntl70), PULLUP_EN | MODE(0x01)},	/* UART0_RXD */
	{OFFSET(pincntl71), PULLUP_EN | MODE(0x01)},	/* UART0_TXD */
	{-1},
};

static struct module_pin_mux uart4_pin_mux[] = {
	{OFFSET(pincntl251), PULLUP_EN | MODE(0x20)},	/* UART4_RXD */
	{OFFSET(pincntl252), PULLUP_EN | MODE(0x20)},	/* UART4_TXD */
	{-1},
};

static struct module_pin_mux mmc1_pin_mux[] = {
	{OFFSET(pincntl1), PULLUP_EN | MODE(0x01)},	/* SD1_CLK */
	{OFFSET(pincntl2), PULLUP_EN | MODE(0x01)},	/* SD1_CMD */
	{OFFSET(pincntl3), PULLUP_EN | MODE(0x01)},	/* SD1_DAT[0] */
	{OFFSET(pincntl4), PULLUP_EN | MODE(0x01)},	/* SD1_DAT[1] */
	{OFFSET(pincntl5), PULLUP_EN | MODE(0x01)},	/* SD1_DAT[2] */
	{OFFSET(pincntl6), PULLUP_EN | MODE(0x01)},	/* SD1_DAT[3] */
	{OFFSET(pincntl74), PULLUP_EN | MODE(0x40)},	/* SD1_POW */
	{OFFSET(pincntl75), MODE(0x40)},		/* SD1_SDWP */
	{OFFSET(pincntl80), PULLUP_EN | MODE(0x02)},	/* SD1_SDCD */
	{-1},
};

static struct module_pin_mux enet_pin_mux[] = {
	{OFFSET(pincntl233), PULLUP_EN | MODE(0x01)},	/* MDCLK */
	{OFFSET(pincntl234), PULLUP_EN | MODE(0x01)},	/* MDIO */
	{OFFSET(pincntl235), MODE(0x01)},	/* EMAC[0]_RXCLK */
	{OFFSET(pincntl236), MODE(0x01)},	/* EMAC[0]_RXCTL */
	{OFFSET(pincntl237), MODE(0x01)},	/* EMAC[0]_RXD[2] */
	{OFFSET(pincntl238), MODE(0x01)},				/* EMAC[0]_TXCTL */
	{OFFSET(pincntl239), MODE(0x01)},				/* EMAC[0]_TXCLK */
	{OFFSET(pincntl240), MODE(0x01)},				/* EMAC[0]_TXD[0] */
	{OFFSET(pincntl241), MODE(0x01)},	/* EMAC[0]_RXD[0] */
	{OFFSET(pincntl242), MODE(0x01)},	/* EMAC[0]_RXD[1] */
	{OFFSET(pincntl244), MODE(0x01)},	/* EMAC[0]_RXD[3] */
	{OFFSET(pincntl245), MODE(0x01)},				/* EMAC[0]_TXD[3] */
	{OFFSET(pincntl246), MODE(0x01)},				/* EMAC[0]_TXD[2] */
	{OFFSET(pincntl247), MODE(0x01)},				/* EMAC[0]_TXD[1] */
};

static struct module_pin_mux lcd_pin_mux[] = {
	{OFFSET(pincntl176), MODE(0x01)},	/* VOUT[0]_CLK */
	{OFFSET(pincntl177), MODE(0x01)},	/* VOUT[0]_HSYNC */
	{OFFSET(pincntl178), MODE(0x01)},	/* VOUT[0]_VSYNC */
	{OFFSET(pincntl179), MODE(0x01)},	/* VOUT[0]_AVID */
	/* LCD Red Channel */
	{OFFSET(pincntl196), MODE(0x01)},	/* VOUT[0]_R_CR[2] */
	{OFFSET(pincntl197), MODE(0x01)},	/* VOUT[0]_R_CR[3] */
	{OFFSET(pincntl198), MODE(0x01)},	/* VOUT[0]_R_CR[4] */
	{OFFSET(pincntl199), MODE(0x01)},	/* VOUT[0]_R_CR[5] */
	{OFFSET(pincntl200), MODE(0x01)},	/* VOUT[0]_R_CR[6] */
	{OFFSET(pincntl201), MODE(0x01)},	/* VOUT[0]_R_CR[7] */
	{OFFSET(pincntl202), MODE(0x01)},	/* VOUT[0]_R_CR[8] */
	{OFFSET(pincntl203), MODE(0x01)},	/* VOUT[0]_R_CR[9] */
	/* LCD Green Channel */
	{OFFSET(pincntl188), MODE(0x01)},	/* VOUT[0]_G_Y_YC[2] */
	{OFFSET(pincntl189), MODE(0x01)},	/* VOUT[0]_G_Y_YC[3] */
	{OFFSET(pincntl190), MODE(0x01)},	/* VOUT[0]_G_Y_YC[4] */
	{OFFSET(pincntl191), MODE(0x01)},	/* VOUT[0]_G_Y_YC[5] */
	{OFFSET(pincntl192), MODE(0x01)},	/* VOUT[0]_G_Y_YC[6] */
	{OFFSET(pincntl193), MODE(0x01)},	/* VOUT[0]_G_Y_YC[7] */
	{OFFSET(pincntl194), MODE(0x01)},	/* VOUT[0]_G_Y_YC[8] */
	{OFFSET(pincntl195), MODE(0x01)},	/* VOUT[0]_G_Y_YC[9] */
	/* LCD Blue Channel */
	{OFFSET(pincntl180), MODE(0x01)},	/* VOUT[0]_B_CB_C[2] */
	{OFFSET(pincntl181), MODE(0x01)},	/* VOUT[0]_B_CB_C[3] */
	{OFFSET(pincntl182), MODE(0x01)},	/* VOUT[0]_B_CB_C[4] */
	{OFFSET(pincntl183), MODE(0x01)},	/* VOUT[0]_B_CB_C[5] */
	{OFFSET(pincntl184), MODE(0x01)},	/* VOUT[0]_B_CB_C[6] */
	{OFFSET(pincntl185), MODE(0x01)},	/* VOUT[0]_B_CB_C[7] */
	{OFFSET(pincntl186), MODE(0x01)},	/* VOUT[0]_B_CB_C[8] */
	{OFFSET(pincntl187), MODE(0x01)},	/* VOUT[0]_B_CB_C[9] */
	/* Misc GPIOs */
	{OFFSET(pincntl39),  PULLUP_EN | MODE(0x80)},	/* LCD DispOn */
	{OFFSET(pincntl40),  PULLUP_EN | MODE(0x80)},	/* LCD ScanDir */
	{OFFSET(pincntl47),  PULLUP_EN | MODE(0x80)},	/* BacklightControl */
};

void enable_uart0_pin_mux(void)
{
	configure_module_pin_mux(uart0_pin_mux);
}

void enable_uart4_pin_mux(void)
{
	configure_module_pin_mux(uart4_pin_mux);
}

void enable_mmc1_pin_mux(void)
{
	configure_module_pin_mux(mmc1_pin_mux);
}

void enable_enet_pin_mux(void)
{
	configure_module_pin_mux(enet_pin_mux);
}

void enable_lcd_pin_mux(void)
{
	configure_module_pin_mux(lcd_pin_mux);
}