/*
	This file was part of bbflash, is part of frser-m328lpcspi.
	Copyright (C) 2013, Hao Liu and Robert L. Thompson

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef LPC_H_
#define LPC_H_
#include "mybool.h"

bool lpc_init(void);
uint8_t lpc_test(void);
void lpc_cleanup(void);
uint8_t lpc_test(void);
int lpc_read_address(uint32_t addr);
bool lpc_write_address(uint32_t addr, uint8_t byte);

#endif /* LPC_H_ */
