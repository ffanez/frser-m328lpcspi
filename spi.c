/*
 * This file is part of the frser-m328lpcspi project.
 *
 * Copyright (C) 2015 Urja Rannikko <urjaman@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "main.h"
#include "flash.h"
#include "spi.h"
#include "uart.h"
#include "frser.h"

static int spi_initialized = 0;

static uint8_t spi_set_spd = 0; /* Max speed - F_CPU/2 */

const uint8_t PROGMEM spd_table[7] = {
	0x80, // SPI2X is 0x80 in this table because i say so. - div 2
	0, // div 4
	0x80 | _BV(SPR0), // div 8
	_BV(SPR0), // div 16
	0x80 | _BV(SPR1), // div 32
	_BV(SPR1), // div64
	_BV(SPR1) | _BV(SPR0), //div128
};

const uint32_t PROGMEM spd_hz_table[7] = {
	F_CPU/2, F_CPU/4, F_CPU/8, F_CPU/16, F_CPU/32, F_CPU/64, F_CPU/128
};

uint32_t spi_set_speed(uint32_t hz) {
	/* We can set F_CPU / : 2,4,8,16,32,64,128 */

	uint8_t spd;
	uint32_t hz_spd;

	/* Range check. */
	if (hz<=(F_CPU/128)) {
		spd = 6;
		hz_spd = F_CPU/128;
	} else {
		for (uint8_t spd=0;spd<7;spd++) {
			hz_spd = pgm_read_dword(&(spd_hz_table[spd]));
			if (hz >= hz_spd) break;
		}
	}
	spi_set_spd = spd;
	if (spi_initialized) { // change speed
		spi_init(); // re-init
	}
	return hz_spd;
}

static void spi_select(void) {
	DDRB |=_BV(0);
}

void spi_deselect(void) {
	DDRB &= ~_BV(0);
	_delay_us(1); // slow pullup
}

void spi_init(void) {
	/* DDR and PORT settings come from flash.c */
	uint8_t spdv = pgm_read_byte(&(spd_table[spi_set_spd]));
	SPCR = _BV(SPE) | _BV(MSTR) | (spdv & 0x03);
	if (spdv&0x80) SPSR |= _BV(SPI2X);
	else SPSR &= ~_BV(SPI2X);
}

uint8_t spi_uninit(void) {
	if (spi_initialized) {
		SPCR = 0;
		spi_initialized = 0;
		return 1;
	}
	return 0;
}

static uint8_t spi_txrx(const uint8_t c) {
	uint8_t r;
	SPDR = c;
	loop_until_bit_is_set(SPSR,SPIF);
	r = SPDR;
	return r;
}

void spi_init_cond(void) {
	if (!spi_initialized) {
		spi_init();
		spi_initialized = 1;
	}
}

static void spi_localop_start(uint8_t sbytes, const uint8_t* sarr) {
	uint8_t i;
	spi_select();
	for(i=0;i<sbytes;i++) spi_txrx(sarr[i]);
}


static void spi_localop_end(uint8_t rbytes, uint8_t* rarr) {
	uint8_t i;
	for(i=0;i<rbytes;i++) rarr[i] = spi_txrx(0xFF);
	spi_deselect();
}

static void spi_localop(uint8_t sbytes, const uint8_t* sarr, uint8_t rbytes, uint8_t* rarr) {
	spi_localop_start(sbytes,sarr);
	spi_localop_end(rbytes,rarr);
}

static void spi_spiop_start(uint32_t sbytes) {
	spi_select();
	while (sbytes--) spi_txrx(RECEIVE());
}

static void spi_spiop_end(uint32_t rbytes) {
	while (rbytes--) SEND(spi_txrx(0xFF));
	spi_deselect();
}

void spi_spiop(uint32_t sbytes, uint32_t rbytes) {
	spi_spiop_start(sbytes);
	SEND(S_ACK);
	spi_spiop_end(rbytes);
}

uint8_t oddparity(uint8_t val) {
	val = (val ^ (val >> 4)) & 0xf;
	val = (val ^ (val >> 2)) & 0x3;
	return (val ^ (val >> 1)) & 0x1;
}

uint8_t spi_probe_rdid(uint8_t *id) {
	const uint8_t out[1] = { 0x9F };
	uint8_t in[3];
	spi_localop(1,out,3,in);
	if (!oddparity(in[0])) return 0;
	if ((in[0] == 0xFF)&&(in[1] == 0xFF)&&(in[2] == 0xFF)) return 0;
	if ((in[0] == 0)&&(in[1] == 0)&&(in[2] == 0)) return 0;
	if (id) memcpy(id,in,3);
	return 1;
}

uint8_t spi_probe_rems(uint8_t *id) {
	const uint8_t out[4] = { 0x90, 0, 0, 0 };
	uint8_t in[2];
	spi_localop(4,out,2,in);
	if ((in[0] == 0xFF)&&(in[1] == 0xFF)) return 0;
	if ((in[0] == 0)&&(in[1] == 0)) return 0;
	if (id) memcpy(id,in,2);
	return 1;
}

uint8_t spi_probe_res(uint8_t *id) {
	const uint8_t out[4] = { 0xAB, 0, 0, 0 };
	uint8_t in[1];
	spi_localop(4,out,1,in);
	if (in[0] == 0xFF) return 0;
	if (in[0] == 0) return 0;
	if (id) *id = in[0];
	return 1;
}

uint8_t spi_test(void) {
	spi_init();
	if (spi_probe_rdid(NULL)) return 1;
	if (spi_probe_rems(NULL)) return 1;
	if (spi_probe_res(NULL)) return 1;
	spi_uninit();
	return 0;
}

uint8_t spi_read(uint32_t addr) {
	uint8_t sarr[4];
	uint8_t rarrv;
	sarr[0] = 0x03; /* Read */
	sarr[1] = (addr>>16)&0xFF;
	sarr[2] = (addr>> 8)&0xFF;
	sarr[3] = (addr    )&0xFF;
	spi_localop(4,sarr,1,&rarrv);
	return rarrv;
}

void spi_readn(uint32_t addr, uint32_t len) {
	uint8_t sarr[4];
	sarr[0] = 0x03;
	sarr[1] = (addr>>16)&0xFF;
	sarr[2] = (addr>> 8)&0xFF;
	sarr[3] = (addr    )&0xFF;
	spi_localop_start(4,sarr);
	spi_spiop_end(len);
}
