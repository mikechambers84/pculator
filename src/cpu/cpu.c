/*
  PCulator: A portable, open-source x86 PC emulator.
  Copyright (C)2025 Mike Chambers

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "tinyfpu.h"
#include "../config.h"
#include "../memory.h"
#include "../debuglog.h"

const uint8_t byteregtable[8] = { regal, regcl, regdl, regbl, regah, regch, regdh, regbh };

const uint8_t parity[0x100] = {
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1
};

void op_ext_illegal(CPU_t* cpu);

uint32_t firstip;
int showops = 0;

#include "cpuhelper.h"

static FUNC_FORCE_INLINE void op_adc8(CPU_t* cpu) {
	cpu->res8 = cpu->oper1b + cpu->oper2b + cpu->cf;
	flag_adc8(cpu, cpu->oper1b, cpu->oper2b, cpu->cf);
}

static FUNC_FORCE_INLINE void op_adc16(CPU_t* cpu) {
	cpu->res16 = cpu->oper1 + cpu->oper2 + cpu->cf;
	flag_adc16(cpu, cpu->oper1, cpu->oper2, cpu->cf);
}

static FUNC_FORCE_INLINE void op_adc32(CPU_t* cpu) {
	cpu->res32 = cpu->oper1_32 + cpu->oper2_32 + (uint32_t)cpu->cf;
	flag_adc32(cpu, cpu->oper1_32, cpu->oper2_32, cpu->cf);
}

static FUNC_FORCE_INLINE void op_add8(CPU_t* cpu) {
	cpu->res8 = cpu->oper1b + cpu->oper2b;
	flag_add8(cpu, cpu->oper1b, cpu->oper2b);
}

static FUNC_FORCE_INLINE void op_add16(CPU_t* cpu) {
	cpu->res16 = cpu->oper1 + cpu->oper2;
	flag_add16(cpu, cpu->oper1, cpu->oper2);
}

static FUNC_FORCE_INLINE void op_add32(CPU_t* cpu) {
	cpu->res32 = cpu->oper1_32 + cpu->oper2_32;
	flag_add32(cpu, cpu->oper1_32, cpu->oper2_32);
}

static FUNC_FORCE_INLINE void op_and8(CPU_t* cpu) {
	cpu->res8 = cpu->oper1b & cpu->oper2b;
	flag_log8(cpu, cpu->res8);
}

static FUNC_FORCE_INLINE void op_and16(CPU_t* cpu) {
	cpu->res16 = cpu->oper1 & cpu->oper2;
	flag_log16(cpu, cpu->res16);
}

static FUNC_FORCE_INLINE void op_and32(CPU_t* cpu) {
	cpu->res32 = cpu->oper1_32 & cpu->oper2_32;
	flag_log32(cpu, cpu->res32);
}

static FUNC_FORCE_INLINE void op_or8(CPU_t* cpu) {
	cpu->res8 = cpu->oper1b | cpu->oper2b;
	flag_log8(cpu, cpu->res8);
}

static FUNC_FORCE_INLINE void op_or16(CPU_t* cpu) {
	cpu->res16 = cpu->oper1 | cpu->oper2;
	flag_log16(cpu, cpu->res16);
}

static FUNC_FORCE_INLINE void op_or32(CPU_t* cpu) {
	cpu->res32 = cpu->oper1_32 | cpu->oper2_32;
	flag_log32(cpu, cpu->res32);
}

static FUNC_FORCE_INLINE void op_xor8(CPU_t* cpu) {
	cpu->res8 = cpu->oper1b ^ cpu->oper2b;
	flag_log8(cpu, cpu->res8);
}

static FUNC_FORCE_INLINE void op_xor16(CPU_t* cpu) {
	cpu->res16 = cpu->oper1 ^ cpu->oper2;
	flag_log16(cpu, cpu->res16);
}

static FUNC_FORCE_INLINE void op_xor32(CPU_t* cpu) {
	cpu->res32 = cpu->oper1_32 ^ cpu->oper2_32;
	flag_log32(cpu, cpu->res32);
}

static FUNC_FORCE_INLINE void op_sub8(CPU_t* cpu) {
	cpu->res8 = cpu->oper1b - cpu->oper2b;
	flag_sub8(cpu, cpu->oper1b, cpu->oper2b);
}

static FUNC_FORCE_INLINE void op_sub16(CPU_t* cpu) {
	cpu->res16 = cpu->oper1 - cpu->oper2;
	flag_sub16(cpu, cpu->oper1, cpu->oper2);
}

static FUNC_FORCE_INLINE void op_sub32(CPU_t* cpu) {
	cpu->res32 = cpu->oper1_32 - cpu->oper2_32;
	flag_sub32(cpu, cpu->oper1_32, cpu->oper2_32);
}

static FUNC_FORCE_INLINE void op_sbb8(CPU_t* cpu) {
	cpu->res8 = cpu->oper1b - (cpu->oper2b + cpu->cf);
	flag_sbb8(cpu, cpu->oper1b, cpu->oper2b, cpu->cf);
}

static FUNC_FORCE_INLINE void op_sbb16(CPU_t* cpu) {
	cpu->res16 = cpu->oper1 - (cpu->oper2 + cpu->cf);
	flag_sbb16(cpu, cpu->oper1, cpu->oper2, cpu->cf);
}

static FUNC_FORCE_INLINE void op_sbb32(CPU_t* cpu) {
	cpu->res32 = cpu->oper1_32 - (cpu->oper2_32 + (uint32_t)cpu->cf);
	flag_sbb32(cpu, cpu->oper1_32, cpu->oper2_32, cpu->cf);
}

void cpu_reset(CPU_t* cpu) {
	uint16_t i;
	cpu->usegdt = 0;
	cpu->protected_mode = 0;
	cpu->paging = 0;
	memset(cpu->segis32, 0, sizeof(cpu->segis32));
	cpu->a20_gate = 0;
	putsegreg(cpu, regcs, 0xFFFF);
	cpu->ip = 0x0000;
	cpu->hltstate = 0;
	cpu->trap_toggle = 0;
	if (!cpu->fpu) {
		cpu->fpu = fpu_new();
	}
	cpu->have387 = cpu->fpu != NULL;
	cpu->cr[0] = 0x00000010 | ((cpu->have387 ^ 1) << 2);
	cpu->interrupt_inhibit = 0;
	memory_tlb_flush(cpu);
	if (cpu->fpu) {
		fpu_reset(cpu->fpu);
	}
}

static FUNC_FORCE_INLINE uint8_t op_grp2_8(CPU_t* cpu, uint8_t cnt) {

	uint16_t	s;
	uint16_t	shift;
	uint16_t	oldcf;
	uint16_t	msb;

	s = cpu->oper1b;
	oldcf = cpu->cf;
	cnt &= 0x1F;
	if (!cnt) {
		return s & 0xFF;
	}
	switch (cpu->reg) {
	case 0: /* ROL r/m8 */
		cnt &= 7;
		if (!cnt) {
			return s & 0xFF;
		}
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x80) {
				cpu->cf = 1;
			}
			else {
				cpu->cf = 0;
			}

			s = s << 1;
			s = s | cpu->cf;
		}

		if (cnt == 1) {
			cpu->of = ((s >> 7) ^ cpu->cf) & 1;
		}
		break;

	case 1: /* ROR r/m8 */
		cnt &= 7;
		if (!cnt) {
			return s & 0xFF;
		}
		for (shift = 1; shift <= cnt; shift++) {
			cpu->cf = s & 1;
			s = (s >> 1) | (cpu->cf << 7);
		}

		if (cnt == 1) {
			cpu->of = (s >> 7) ^ ((s >> 6) & 1);
		}
		break;

	case 2: /* RCL r/m8 */
		cnt %= 9;
		if (!cnt) {
			return s & 0xFF;
		}
		for (shift = 1; shift <= cnt; shift++) {
			oldcf = cpu->cf;
			if (s & 0x80) {
				cpu->cf = 1;
			}
			else {
				cpu->cf = 0;
			}

			s = s << 1;
			s = s | oldcf;
		}

		if (cnt == 1) {
			cpu->of = cpu->cf ^ ((s >> 7) & 1);
		}
		break;

	case 3: /* RCR r/m8 */
		cnt %= 9;
		if (!cnt) {
			return s & 0xFF;
		}
		for (shift = 1; shift <= cnt; shift++) {
			oldcf = cpu->cf;
			cpu->cf = s & 1;
			s = (s >> 1) | (oldcf << 7);
		}

		if (cnt == 1) {
			cpu->of = (s >> 7) ^ ((s >> 6) & 1);
		}
		break;

	case 4:
	case 6: /* SHL r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x80) {
				cpu->cf = 1;
			}
			else {
				cpu->cf = 0;
			}

			s = (s << 1) & 0xFF;
		}

		if (cnt == 1) { //((cnt == 1) && (cpu->cf == (s >> 7))) {
			cpu->of = ((s >> 7) ^ cpu->cf) & 1; //0;
		}
		/*else {
			cpu->of = 1;
		}*/

		flag_szp8(cpu, (uint8_t)s);
		break;

	case 5: /* SHR r/m8 */
		if ((cnt == 1) && (s & 0x80)) {
			cpu->of = 1;
		}
		else {
			cpu->of = 0;
		}

		for (shift = 1; shift <= cnt; shift++) {
			cpu->cf = s & 1;
			s = s >> 1;
		}

		flag_szp8(cpu, (uint8_t)s);
		break;

	case 7: /* SAR r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			msb = s & 0x80;
			cpu->cf = s & 1;
			s = (s >> 1) | msb;
		}

		cpu->of = 0;
		flag_szp8(cpu, (uint8_t)s);
		break;
	}

	return s & 0xFF;
}

static FUNC_FORCE_INLINE uint16_t op_grp2_16(CPU_t* cpu, uint8_t cnt) {

	uint32_t	s;
	uint32_t	shift;
	uint32_t	oldcf;
	uint32_t	msb;

	s = cpu->oper1;
	oldcf = cpu->cf;
	cnt &= 0x1F;
	if (!cnt) {
		return (uint16_t)s & 0xFFFF;
	}
	switch (cpu->reg) {
	case 0: /* ROL r/m8 */
		cnt &= 0x0F;
		if (!cnt) {
			return (uint16_t)s & 0xFFFF;
		}
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x8000) {
				cpu->cf = 1;
			}
			else {
				cpu->cf = 0;
			}

			s = s << 1;
			s = s | cpu->cf;
		}

		if (cnt == 1) {
			cpu->of = cpu->cf ^ ((s >> 15) & 1);
		}
		break;

	case 1: /* ROR r/m8 */
		cnt &= 0x0F;
		if (!cnt) {
			return (uint16_t)s & 0xFFFF;
		}
		for (shift = 1; shift <= cnt; shift++) {
			cpu->cf = s & 1;
			s = (s >> 1) | (cpu->cf << 15);
		}

		if (cnt == 1) {
			cpu->of = (s >> 15) ^ ((s >> 14) & 1);
		}
		break;

	case 2: /* RCL r/m8 */
		cnt %= 17;
		if (!cnt) {
			return (uint16_t)s & 0xFFFF;
		}
		for (shift = 1; shift <= cnt; shift++) {
			oldcf = cpu->cf;
			if (s & 0x8000) {
				cpu->cf = 1;
			}
			else {
				cpu->cf = 0;
			}

			s = s << 1;
			s = s | oldcf;
		}

		if (cnt == 1) {
			cpu->of = cpu->cf ^ ((s >> 15) & 1);
		}
		break;

	case 3: /* RCR r/m8 */
		cnt %= 17;
		if (!cnt) {
			return (uint16_t)s & 0xFFFF;
		}
		for (shift = 1; shift <= cnt; shift++) {
			oldcf = cpu->cf;
			cpu->cf = s & 1;
			s = (s >> 1) | (oldcf << 15);
		}

		if (cnt == 1) {
			cpu->of = (s >> 15) ^ ((s >> 14) & 1);
		}
		break;

	case 4:
	case 6: /* SHL r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x8000) {
				cpu->cf = 1;
			}
			else {
				cpu->cf = 0;
			}

			s = (s << 1) & 0xFFFF;
		}

		if (cnt == 1) { //((cnt == 1) && (cpu->cf == (s >> 15))) {
			cpu->of = ((s >> 15) ^ cpu->cf) & 1; //0;
		}
		/*else {
			cpu->of = 1;
		}*/

		flag_szp16(cpu, (uint16_t)s);
		break;

	case 5: /* SHR r/m8 */
		if ((cnt == 1) && (s & 0x8000)) {
			cpu->of = 1;
		}
		else {
			cpu->of = 0;
		}

		for (shift = 1; shift <= cnt; shift++) {
			cpu->cf = s & 1;
			s = s >> 1;
		}

		flag_szp16(cpu, (uint16_t)s);
		break;

	case 7: /* SAR r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			msb = s & 0x8000;
			cpu->cf = s & 1;
			s = (s >> 1) | msb;
		}

		cpu->of = 0;
		flag_szp16(cpu, (uint16_t)s);
		break;
	}

	return (uint16_t)s & 0xFFFF;
}

static FUNC_FORCE_INLINE uint32_t op_grp2_32(CPU_t* cpu, uint8_t cnt) {

	uint64_t	s;
	uint64_t	shift;
	uint64_t	oldcf;
	uint64_t	msb;

	s = cpu->oper1_32;
	oldcf = cpu->cf;
	cnt &= 0x1F;
	if (!cnt) {
		return (uint32_t)s;
	}
	switch (cpu->reg) {
	case 0: /* ROL r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x80000000) {
				cpu->cf = 1;
			}
			else {
				cpu->cf = 0;
			}

			s = s << 1;
			s = s | cpu->cf;
		}

		if (cnt == 1) {
			cpu->of = cpu->cf ^ ((s >> 31) & 1);
		}
		break;

	case 1: /* ROR r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			cpu->cf = s & 1;
			s = (s >> 1) | ((uint64_t)cpu->cf << 31);
		}

		if (cnt == 1) {
			cpu->of = (s >> 31) ^ ((s >> 30) & 1);
		}
		break;

	case 2: /* RCL r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			oldcf = cpu->cf;
			if (s & 0x80000000) {
				cpu->cf = 1;
			}
			else {
				cpu->cf = 0;
			}

			s = s << 1;
			s = s | oldcf;
		}

		if (cnt == 1) {
			cpu->of = cpu->cf ^ ((s >> 31) & 1);
		}
		break;

	case 3: /* RCR r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			oldcf = cpu->cf;
			cpu->cf = s & 1;
			s = (s >> 1) | (oldcf << 31);
		}

		if (cnt == 1) {
			cpu->of = (s >> 31) ^ ((s >> 30) & 1);
		}
		break;

	case 4:
	case 6: /* SHL r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x80000000) {
				cpu->cf = 1;
			}
			else {
				cpu->cf = 0;
			}

			s = (s << 1) & 0xFFFFFFFF;
		}

		//if ((cnt == 1) && (cpu->cf == (s >> 31))) {
		if (cnt == 1) {
			cpu->of = ((s >> 31) ^ cpu->cf) & 1; //0;
		}
		/*else {
			cpu->of = 1;
		}*/

		flag_szp32(cpu, (uint32_t)s);
		break;

	case 5: /* SHR r/m8 */
		if ((cnt == 1) && (s & 0x80000000)) {
			cpu->of = 1;
		}
		else {
			cpu->of = 0;
		}

		for (shift = 1; shift <= cnt; shift++) {
			cpu->cf = s & 1;
			s = s >> 1;
		}

		flag_szp32(cpu, (uint32_t)s);
		break;

	case 7: /* SAR r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			msb = s & 0x80000000;
			cpu->cf = s & 1;
			s = (s >> 1) | msb;
		}

		cpu->of = 0;
		flag_szp32(cpu, (uint32_t)s);
		break;
	}

	return (uint32_t)s;
}

static FUNC_FORCE_INLINE void op_div8(CPU_t* cpu, uint16_t valdiv, uint8_t divisor) {
	if (divisor == 0) {
		//cpu_intcall(cpu, 0, INT_SOURCE_EXCEPTION, 0);
		exception(cpu, 0, 0); //DE
		return;
	}

	if ((valdiv / (uint16_t)divisor) > 0xFF) {
		//cpu_intcall(cpu, 0, INT_SOURCE_EXCEPTION, 0);
		exception(cpu, 0, 0); //DE
		return;
	}

	cpu->regs.byteregs[regah] = valdiv % (uint16_t)divisor;
	cpu->regs.byteregs[regal] = valdiv / (uint16_t)divisor;
}

static FUNC_FORCE_INLINE void op_idiv8(CPU_t* cpu, uint16_t valdiv, uint8_t divisor) {
	int32_t dividend;
	int32_t signed_divisor;
	int32_t quotient;
	int32_t remainder;

	if (divisor == 0) {
		//cpu_intcall(cpu, 0, INT_SOURCE_EXCEPTION, 0);
		exception(cpu, 0, 0); //DE
		return;
	}

	dividend = (int16_t)valdiv;
	signed_divisor = (int8_t)divisor;
	quotient = dividend / signed_divisor;
	remainder = dividend % signed_divisor;
	if ((quotient < -128) || (quotient > 127)) {
		//cpu_intcall(cpu, 0, INT_SOURCE_EXCEPTION, 0);
		exception(cpu, 0, 0); //DE
		return;
	}

	cpu->regs.byteregs[regah] = (uint8_t)remainder;
	cpu->regs.byteregs[regal] = (uint8_t)quotient;
}

static FUNC_FORCE_INLINE void op_grp3_8(CPU_t* cpu) {
	//cpu->oper1 = signext(cpu->oper1b);
	//cpu->oper2 = signext(cpu->oper2b);
	switch (cpu->reg) {
	case 0:
	case 1: /* TEST */
		flag_log8(cpu, cpu->oper1b & getmem8(cpu, cpu->segcache[regcs], cpu->ip));
		StepIP(cpu, 1);
		break;

	case 2: /* NOT */
		cpu->res8 = ~cpu->oper1b;
		break;

	case 3: /* NEG */
		cpu->res8 = (~cpu->oper1b) + 1;
		flag_sub8(cpu, 0, cpu->oper1b);
		if (cpu->res8 == 0) {
			cpu->cf = 0;
		}
		else {
			cpu->cf = 1;
		}
		break;

	case 4: /* MUL */
		cpu->temp1 = (uint32_t)cpu->oper1b * (uint32_t)cpu->regs.byteregs[regal];
		cpu->regs.wordregs[regax] = cpu->temp1 & 0xFFFF;
		//flag_szp8(cpu, (uint8_t)cpu->temp1); //TODO: undefined?
		if (cpu->regs.byteregs[regah]) {
			cpu->cf = 1;
			cpu->of = 1;
		}
		else {
			cpu->cf = 0;
			cpu->of = 0;
		}
		break;

	case 5: /* IMUL */
	{
		int16_t result = (int16_t)((int8_t)cpu->regs.byteregs[regal] * (int8_t)cpu->oper1b);
		cpu->regs.wordregs[regax] = (uint16_t)result;
		if ((result < -128) || (result > 127)) {
			cpu->cf = 1;
			cpu->of = 1;
		}
		else {
			cpu->cf = 0;
			cpu->of = 0;
		}
		break;
	}

	case 6: /* DIV */
		op_div8(cpu, cpu->regs.wordregs[regax], cpu->oper1b);
		break;

	case 7: /* IDIV */
		op_idiv8(cpu, cpu->regs.wordregs[regax], cpu->oper1b);
		break;
	}
}

static FUNC_FORCE_INLINE void op_div16(CPU_t* cpu, uint32_t valdiv, uint16_t divisor) {
	if (divisor == 0) {
		//cpu_intcall(cpu, 0, INT_SOURCE_EXCEPTION, 0);
		exception(cpu, 0, 0); //DE
		return;
	}

	if ((valdiv / (uint32_t)divisor) > 0xFFFF) {
		//cpu_intcall(cpu, 0, INT_SOURCE_EXCEPTION, 0);
		exception(cpu, 0, 0); //DE
		return;
	}

	cpu->regs.wordregs[regdx] = valdiv % (uint32_t)divisor;
	cpu->regs.wordregs[regax] = valdiv / (uint32_t)divisor;
}

static FUNC_FORCE_INLINE void op_div32(CPU_t* cpu, uint64_t valdiv, uint32_t divisor) {
	if (divisor == 0) {
		//cpu_intcall(cpu, 0, INT_SOURCE_EXCEPTION, 0);
		exception(cpu, 0, 0); //DE
		return;
	}

	if ((valdiv / (uint64_t)divisor) > 0xFFFFFFFF) {
		//cpu_intcall(cpu, 0, INT_SOURCE_EXCEPTION, 0);
		exception(cpu, 0, 0); //DE
		return;
	}

	cpu->regs.longregs[regedx] = valdiv % (uint32_t)divisor;
	cpu->regs.longregs[regeax] = valdiv / (uint32_t)divisor;
}

static FUNC_FORCE_INLINE void op_idiv16(CPU_t* cpu, uint32_t valdiv, uint16_t divisor) {
	int64_t dividend;
	int64_t signed_divisor;
	int64_t quotient;
	int64_t remainder;

	if (divisor == 0) {
		//cpu_intcall(cpu, 0, INT_SOURCE_EXCEPTION, 0);
		exception(cpu, 0, 0); //DE
		return;
	}

	dividend = (int32_t)valdiv;
	signed_divisor = (int16_t)divisor;
	quotient = dividend / signed_divisor;
	remainder = dividend % signed_divisor;
	if ((quotient < -32768) || (quotient > 32767)) {
		//cpu_intcall(cpu, 0, INT_SOURCE_EXCEPTION, 0);
		exception(cpu, 0, 0); //DE
		return;
	}

	cpu->regs.wordregs[regax] = (uint16_t)quotient;
	cpu->regs.wordregs[regdx] = (uint16_t)remainder;
}

static FUNC_FORCE_INLINE void op_idiv32(CPU_t* cpu, uint64_t valdiv, uint32_t divisor) {
	int64_t dividend;
	int64_t signed_divisor;
	int64_t quotient;
	int64_t remainder;

	if (divisor == 0) {
		//cpu_intcall(cpu, 0, INT_SOURCE_EXCEPTION, 0);
		exception(cpu, 0, 0); //DE
		return;
	}

	dividend = (int64_t)valdiv;
	signed_divisor = (int32_t)divisor;
	if ((dividend == ((int64_t)1 << 63)) && (signed_divisor == -1)) {
		//cpu_intcall(cpu, 0, INT_SOURCE_EXCEPTION, 0);
		exception(cpu, 0, 0); //DE
		return;
	}

	quotient = dividend / signed_divisor;
	remainder = dividend % signed_divisor;
	if ((quotient < (-2147483647 - 1)) || (quotient > 2147483647)) {
		//cpu_intcall(cpu, 0, INT_SOURCE_EXCEPTION, 0);
		exception(cpu, 0, 0); //DE
		return;
	}

	cpu->regs.longregs[regeax] = (uint32_t)quotient;
	cpu->regs.longregs[regedx] = (uint32_t)remainder;
}

static FUNC_FORCE_INLINE void op_grp3_16(CPU_t* cpu) {
	switch (cpu->reg) {
	case 0:
	case 1: /* TEST */
		flag_log16(cpu, cpu->oper1 & getmem16(cpu, cpu->segcache[regcs], cpu->ip));
		StepIP(cpu, 2);
		break;

	case 2: /* NOT */
		cpu->res16 = ~cpu->oper1;
		break;

	case 3: /* NEG */
		cpu->res16 = (~cpu->oper1) + 1;
		flag_sub16(cpu, 0, cpu->oper1);
		if (cpu->res16) {
			cpu->cf = 1;
		}
		else {
			cpu->cf = 0;
		}
		break;

	case 4: /* MUL */
		cpu->temp1 = (uint32_t)cpu->oper1 * (uint32_t)cpu->regs.wordregs[regax];
		cpu->regs.wordregs[regax] = cpu->temp1 & 0xFFFF;
		cpu->regs.wordregs[regdx] = cpu->temp1 >> 16;
		//flag_szp16(cpu, (uint16_t)cpu->temp1); //TODO: undefined?
		if (cpu->regs.wordregs[regdx]) {
			cpu->cf = 1;
			cpu->of = 1;
		}
		else {
			cpu->cf = 0;
			cpu->of = 0;
		}
		break;

	case 5: /* IMUL */
	{
		int32_t result = (int32_t)(int16_t)cpu->regs.wordregs[regax] * (int32_t)(int16_t)cpu->oper1;
		cpu->regs.wordregs[regax] = (uint16_t)result;	/* into register ax */
		cpu->regs.wordregs[regdx] = (uint16_t)(result >> 16);	/* into register dx */
		if ((result < -32768) || (result > 32767)) {
			cpu->cf = 1;
			cpu->of = 1;
		}
		else {
			cpu->cf = 0;
			cpu->of = 0;
		}
		break;
	}

	case 6: /* DIV */
		op_div16(cpu, ((uint32_t)cpu->regs.wordregs[regdx] << 16) + cpu->regs.wordregs[regax], cpu->oper1);
		break;

	case 7: /* DIV */
		op_idiv16(cpu, ((uint32_t)cpu->regs.wordregs[regdx] << 16) + cpu->regs.wordregs[regax], cpu->oper1);
		break;
	}
}

static FUNC_FORCE_INLINE void op_grp3_32(CPU_t* cpu) {
	switch (cpu->reg) {
	case 0:
	case 1: /* TEST */
		flag_log32(cpu, cpu->oper1_32 & getmem32(cpu, cpu->segcache[regcs], cpu->ip));
		StepIP(cpu, 4);
		break;

	case 2: /* NOT */
		cpu->res32 = ~cpu->oper1_32;
		break;

	case 3: /* NEG */
		cpu->res32 = (~cpu->oper1_32) + 1;
		flag_sub32(cpu, 0, cpu->oper1_32);
		if (cpu->res32) {
			cpu->cf = 1;
		}
		else {
			cpu->cf = 0;
		}
		break;

	case 4: /* MUL */
		cpu->temp64 = (uint64_t)cpu->oper1_32 * (uint64_t)cpu->regs.longregs[regeax];
		cpu->regs.longregs[regeax] = cpu->temp64 & 0xFFFFFFFF;
		cpu->regs.longregs[regedx] = cpu->temp64 >> 32;
		//flag_szp32(cpu, (uint32_t)cpu->temp64); //TODO: undefined?
		if (cpu->regs.longregs[regedx]) {
			cpu->cf = 1;
			cpu->of = 1;
		}
		else {
			cpu->cf = 0;
			cpu->of = 0;
		}
		break;

	case 5: /* IMUL */
	{
		int64_t result = (int64_t)(int32_t)cpu->regs.longregs[regeax] * (int64_t)(int32_t)cpu->oper1_32;
		cpu->regs.longregs[regeax] = (uint32_t)result;	/* into register ax */
		cpu->regs.longregs[regedx] = (uint32_t)(result >> 32);	/* into register dx */
		if ((result < (-2147483647LL - 1)) || (result > 2147483647LL)) {
			cpu->cf = 1;
			cpu->of = 1;
		}
		else {
			cpu->cf = 0;
			cpu->of = 0;
		}
		break;
	}

	case 6: /* DIV */
		op_div32(cpu, ((uint64_t)cpu->regs.longregs[regedx] << 32) + cpu->regs.longregs[regeax], cpu->oper1_32);
		break;

	case 7: /* DIV */
		op_idiv32(cpu, ((uint64_t)cpu->regs.longregs[regedx] << 32) + cpu->regs.longregs[regeax], cpu->oper1_32);
		break;
	}
}

static FUNC_FORCE_INLINE void op_grp5(CPU_t* cpu) {
	switch (cpu->reg) {
	case 0: /* INC Ev */
		cpu->oper2 = 1;
		cpu->tempcf = cpu->cf;
		op_add16(cpu);
		cpu->cf = cpu->tempcf;
		writerm16(cpu, cpu->rm, cpu->res16);
		break;

	case 1: /* DEC Ev */
		cpu->oper2 = 1;
		cpu->tempcf = cpu->cf;
		op_sub16(cpu);
		cpu->cf = cpu->tempcf;
		writerm16(cpu, cpu->rm, cpu->res16);
		break;

	case 2: /* CALL Ev */
		push(cpu, cpu->ip);
		cpu->ip = cpu->oper1;
		break;

	case 3: /* CALL Mp */
	{
		uint32_t new_ip;
		uint16_t new_cs;
		if (cpu->mode == 3) {
			cpu->ip = firstip;
			exception(cpu, 6, 0); //UD
			break;
		}
		//push(cpu, cpu->segregs[regcs]);
		//push(cpu, cpu->ip);
		getea(cpu, cpu->rm);
		//cpu->ip = (uint16_t)cpu_read(cpu, cpu->ea) + (uint16_t)cpu_read(cpu, cpu->ea + 1) * 256;
		//putsegreg(cpu, regcs, (uint16_t)cpu_read(cpu, cpu->ea + 2) + (uint16_t)cpu_read(cpu, cpu->ea + 3) * 256);
		new_ip = (uint16_t)cpu_read(cpu, cpu->ea) + (uint16_t)cpu_read(cpu, cpu->ea + 1) * 256;
		new_cs = (uint16_t)cpu_read(cpu, cpu->ea + 2) + (uint16_t)cpu_read(cpu, cpu->ea + 3) * 256;
		cpu_callf(cpu, new_cs, new_ip);
		break;
	}

	case 4: /* JMP Ev */
		cpu->ip = cpu->oper1;
		break;

	case 5: /* JMP Mp */
		if (cpu->mode == 3) {
			cpu->ip = firstip;
			exception(cpu, 6, 0); //UD
			break;
		}
		getea(cpu, cpu->rm);
		cpu_jmpf(cpu,
			(uint16_t)cpu_read(cpu, cpu->ea + 2) + (uint16_t)cpu_read(cpu, cpu->ea + 3) * 256,
			(uint16_t)cpu_read(cpu, cpu->ea) + (uint16_t)cpu_read(cpu, cpu->ea + 1) * 256);
		break;

	case 6: /* PUSH Ev */
		push(cpu, cpu->oper1);
		break;
	}
}

static FUNC_FORCE_INLINE void op_grp5_32(CPU_t* cpu) {
	//printf("op_grp5_32 reg = %u\n", cpu->reg);
	switch (cpu->reg) {
	case 0: /* INC Ev */
		cpu->oper2_32 = 1;
		cpu->tempcf = cpu->cf;
		op_add32(cpu);
		cpu->cf = cpu->tempcf;
		writerm32(cpu, cpu->rm, cpu->res32);
		break;

	case 1: /* DEC Ev */
		cpu->oper2_32 = 1;
		cpu->tempcf = cpu->cf;
		op_sub32(cpu);
		cpu->cf = cpu->tempcf;
		writerm32(cpu, cpu->rm, cpu->res32);
		break;

	case 2: /* CALL Ev */
		push(cpu, cpu->ip);
		cpu->ip = cpu->oper1_32;
		break;

	case 3: /* CALL Mp */ //TODO: is this right?
	{
		uint32_t new_ip;
		uint16_t new_cs;
		if (cpu->mode == 3) {
			cpu->ip = firstip;
			exception(cpu, 6, 0); //UD
			break;
		}
		//push(cpu, cpu->segregs[regcs]);
		//push(cpu, cpu->ip);
		getea(cpu, cpu->rm);
		//cpu->ip = (uint32_t)cpu_read(cpu, cpu->ea) | ((uint32_t)cpu_read(cpu, cpu->ea + 1) << 8) | ((uint32_t)cpu_read(cpu, cpu->ea + 2) << 16) | ((uint32_t)cpu_read(cpu, cpu->ea + 3) << 24);
		//putsegreg(cpu, regcs, (uint16_t)cpu_read(cpu, cpu->ea + 4) | ((uint16_t)cpu_read(cpu, cpu->ea + 5) << 8));
		new_ip = (uint32_t)cpu_read(cpu, cpu->ea) | ((uint32_t)cpu_read(cpu, cpu->ea + 1) << 8) | ((uint32_t)cpu_read(cpu, cpu->ea + 2) << 16) | ((uint32_t)cpu_read(cpu, cpu->ea + 3) << 24);
		new_cs = (uint16_t)cpu_read(cpu, cpu->ea + 4) | ((uint16_t)cpu_read(cpu, cpu->ea + 5) << 8);
		cpu_callf(cpu, new_cs, new_ip);
		break;
	}

	case 4: /* JMP Ev */
		cpu->ip = cpu->oper1_32;
		//printf("GRP5_32 JMP EA = %08X, disp32 = %08X, mod = %u, reg = %u, rm = %u, SIB index = %u, SIB base = %u\n", cpu->ea, cpu->disp32, cpu->mode, cpu->reg, cpu->rm, cpu->sib_index, cpu->sib_base);
		break;

	case 5: /* JMP Mp */ //TODO: is this right?
		if (cpu->mode == 3) {
			cpu->ip = firstip;
			exception(cpu, 6, 0); //UD
			break;
		}
		getea(cpu, cpu->rm);
		cpu_jmpf(cpu,
			(uint16_t)cpu_read(cpu, cpu->ea + 4) | ((uint16_t)cpu_read(cpu, cpu->ea + 5) << 8),
			(uint32_t)cpu_read(cpu, cpu->ea) | ((uint32_t)cpu_read(cpu, cpu->ea + 1) << 8) | ((uint32_t)cpu_read(cpu, cpu->ea + 2) << 16) | ((uint32_t)cpu_read(cpu, cpu->ea + 3) << 24));
		break;

	case 6: /* PUSH Ev */
		push(cpu, cpu->oper1_32);
		break;
	}
}

void cpu_int15_handler(CPU_t* cpu) {
	printf("Int 15h AX = %04X\n", cpu->regs.wordregs[regax]);
	switch (cpu->regs.byteregs[regah]) {
	case 0x24:
		switch (cpu->regs.byteregs[regal]) {
		case 0x00:
			cpu->a20_gate = 0;
			cpu->cf = 0;
			break;
		case 0x01:
			cpu->a20_gate = 1;
			cpu->cf = 0;
			break;
		case 0x02:
			cpu->regs.byteregs[regah] = 0;
			cpu->regs.byteregs[regal] = cpu->a20_gate;
			cpu->cf = 0;
			break;
		case 0x03:
			cpu->regs.byteregs[regah] = 0;
			cpu->regs.wordregs[regbx] = 1;
			cpu->cf = 0;
			break;
		}
		return;
	case 0x87:
	{
		uint32_t source, dest, len, table, i;
		uint8_t old_a20 = cpu->a20_gate;
		cpu->a20_gate = 1;
		table = (uint32_t)cpu->segregs[reges] * 16 + (uint32_t)cpu->regs.wordregs[regsi];
		source = cpu_readl(cpu, table + 0x12) & 0xFFFFFF;
		dest = cpu_readl(cpu, table + 0x1A) & 0xFFFFFF;
		len = (uint32_t)cpu_readw(cpu, table + 0x10) + 1; // (uint32_t)cpu->regs.wordregs[regcx] * 2;
		printf("Copy from %08X -> %08X (len %lu)\n", source, dest, len);
		for (i = 0; i < len; i++) {
			uint8_t val;
			val = cpu_read(cpu, source + i);
			cpu_write(cpu, dest + i, val);
			//printf("%c", val);
		}
		cpu->cf = 0;
		cpu->regs.byteregs[regah] = 0;
		cpu->a20_gate = old_a20;
		return;
	}
	case 0x88:
		cpu->cf = 0;
		cpu->regs.wordregs[regax] = (MEMORY_RANGE / 1024) - 1024;
		return;
	case 0x53:
		cpu->cf = 1;
		cpu->regs.byteregs[regah] = 0x86;
		break;
	default:
		printf("Other int 15h call: %02X\n", cpu->regs.byteregs[regah]);
		cpu->cf = 1;
	}
}

void cpu_intcall(CPU_t* cpu, uint8_t intnum, uint8_t source, uint32_t err) {
	GATEDESC_t gate;
	CODETARGET_t target;
	uint32_t new_esp, old_esp, old_flags, push_eip;
	uint16_t new_ss, old_ss;
	int has_err;
	int include_vm86;
	int frame_size;

	if (!cpu->protected_mode) { // real mode
		uint32_t real_flags = makeflagsword(cpu);
		uint16_t target_cs;
		uint16_t target_ip;

		//Call HLE interrupt, if one is assigned
		/*if (strcmp(usemachine, "generic_xt") == 0) if (intnum == 0x15) { //hack to get an int 15h with the generic XT BIOS
			cpu_int15_handler(cpu);
			return;
		}*/
		/*if (intnum == 0x15 && cpu->regs.wordregs[regax] == 0xE820) {
			uint32_t addr32;
			addr32 = cpu->segcache[reges] + cpu->regs.wordregs[regdi];
			printf("0xE820 EBX = %08X, ECX = %08X, buffer = %08X\n", cpu->regs.longregs[regebx], cpu->regs.longregs[regecx], addr32);
			switch (cpu->regs.longregs[regebx]) {
			case 0:
				cpu_writel(cpu, addr32, 0);
				cpu_writel(cpu, addr32 + 4, 0);
				cpu_writel(cpu, addr32 + 8, 0x9FC00);
				cpu_writel(cpu, addr32 + 12, 0);
				cpu_writew(cpu, addr32 + 16, 1);
				cpu->regs.longregs[regebx] = 0x02938475;
				break;
			case 0x02938475:
				cpu_writel(cpu, addr32, 0x9FC00);
				cpu_writel(cpu, addr32 + 4, 0);
				cpu_writel(cpu, addr32 + 8, 0x400);
				cpu_writel(cpu, addr32 + 12, 0);
				cpu_writew(cpu, addr32 + 16, 2);
				cpu->regs.longregs[regebx] = 0x91827364;
				break;
			case 0x91827364:
				cpu_writel(cpu, addr32, 0xF0000);
				cpu_writel(cpu, addr32 + 4, 0);
				cpu_writel(cpu, addr32 + 8, 0x10000);
				cpu_writel(cpu, addr32 + 12, 0);
				cpu_writew(cpu, addr32 + 16, 2);
				cpu->regs.longregs[regebx] = 0x56473829;
				break;
			case 0x56473829:
				cpu_writel(cpu, addr32, 0x100000);
				cpu_writel(cpu, addr32 + 4, 0);
				cpu_writel(cpu, addr32 + 8, 0xFF00000);
				cpu_writel(cpu, addr32 + 12, 0);
				cpu_writew(cpu, addr32 + 16, 1);
				cpu->regs.longregs[regebx] = 0;
				break;
			}
			cpu->regs.longregs[regecx] = 0x14;
			cpu->regs.longregs[regeax] = 0x534D4150;
			cpu->cf = 0;
			return;
		}*/
		//debug_log(DEBUG_DETAIL, "Int %02Xh, real mode\n", intnum);
		//Otherwise, do a real interrupt call
		target_cs = getmem16(cpu, 0, (uint16_t)intnum * 4 + 2);
		target_ip = getmem16(cpu, 0, (uint16_t)intnum * 4);
		pushw(cpu, (uint16_t)real_flags);
		pushw(cpu, cpu->segregs[regcs]);
		pushw(cpu, cpu->ip);
		putsegreg(cpu, regcs, target_cs);
		cpu->ip = target_ip;
		cpu->ifl = 0;
		cpu->tf = 0;
		return;
	}

	/*debug_log(DEBUG_DETAIL, "Int %02Xh, protected mode (IDTR %08X, ", intnum, cpu->idtr);
	switch (source) {
	case INT_SOURCE_EXCEPTION: debug_log(DEBUG_DETAIL, "source = exception)\n"); break;
	case INT_SOURCE_SOFTWARE: debug_log(DEBUG_DETAIL, "source = software)\n"); break;
	case INT_SOURCE_HARDWARE: debug_log(DEBUG_DETAIL, "source = hardware)\n"); break;
	}*/

	if (source == INT_SOURCE_EXCEPTION) {
		/*switch (intnum) {
		case 0:	case 1:	case 2:	case 3:	case 4:	case 5:	case 6:	case 7:
		case 8:	case 9: case 10: case 11: case 12: case 13: case 14: case 16:
		case 17: case 18: case 19:
			push_eip = cpu->saveip;
			break;
		default:
			push_eip = cpu->ip;
		}*/
		push_eip = cpu->exceptionip;
		cpu->nowrite = 0;
	}
	else {
		push_eip = cpu->ip;
	}
	old_flags = makeflagsword(cpu);

	if (cpu->v86f && (source == INT_SOURCE_SOFTWARE) && (cpu->iopl < 3)) {
		exception(cpu, 13, 0); //GP(0)
		return;
	}

	if (!cpu_read_idt_gate(cpu, intnum, &gate)) {
		exception(cpu, 13, cpu_idt_error_code(intnum, source));
		return;
	}
	if ((cpu->cpl > gate.dpl) &&
		((source == INT_SOURCE_SOFTWARE) || (source == INT_SOURCE_INT3) || (source == INT_SOURCE_INTO))) {
		exception(cpu, 13, cpu_idt_error_code(intnum, source)); //GP
		return;
	}
	if (!cpu_gate_is_interrupt_or_trap(&gate) && (gate.type != 0x5)) {
		exception(cpu, 13, cpu_idt_error_code(intnum, source));
		return;
	}
	if (!gate.present) {
		exception(cpu, 11, cpu_idt_error_code(intnum, source)); //NP
		return;
	}

	if (gate.type == 0x5) { // Task Gate
		task_switch(cpu, gate.target_selector, TASK_SWITCH_REASON_GATE);
		return;
	}

	if (!cpu_validate_gate_target_code(cpu, gate.target_selector, &target)) {
		return;
	}

	has_err = (source == INT_SOURCE_EXCEPTION) && cpu_exception_has_error_code(intnum);
	if (target.outer || (old_flags & EFLAGS_VM)) {
		if (!cpu_fetch_tss_stack(cpu, target.target_cpl, &new_ss, &new_esp)) {
			return;
		}

		old_esp = cpu->regs.longregs[regesp];
		old_ss = cpu->segregs[regss];
		include_vm86 = ((old_flags & EFLAGS_VM) != 0) && cpu_gate_is_32bit(&gate);

		if (cpu_gate_is_32bit(&gate)) {
			frame_size = cpu_interrupt_frame_size32(1, include_vm86, has_err);
			// Entering a protected-mode handler from VM86 must load the new SS/CS
			// through descriptor semantics, not the VM86 real-mode path.
			if (old_flags & EFLAGS_VM) {
				cpu->v86f = 0;
			}
			if (!cpu_commit_validated_segment(cpu, regss, new_ss)) return;
			cpu->regs.longregs[regesp] = new_esp;
			cpu_push_interrupt_frame32_sys(cpu, 1, include_vm86, old_ss, old_esp, old_flags, cpu->segregs[regcs], push_eip, has_err, err);
			if (cpu_gate_is_interrupt(&gate)) cpu->ifl = 0;
			cpu->nt = 0;
			cpu->tf = 0;
			putsegreg(cpu, regcs, target.selector);
			cpu->ip = gate.offset;
			cpu->cpl = target.target_cpl;
			return;
		}

		frame_size = cpu_interrupt_frame_size16(1, has_err);
		// Entering a protected-mode handler from VM86 must load the new SS/CS
		// through descriptor semantics, not the VM86 real-mode path.
		if (old_flags & EFLAGS_VM) {
			cpu->v86f = 0;
		}
		if (!cpu_commit_validated_segment(cpu, regss, new_ss)) return;
		cpu->regs.longregs[regesp] = new_esp;
		cpu_push_interrupt_frame16_sys(cpu, 1, old_ss, old_esp, old_flags, cpu->segregs[regcs], push_eip, has_err, err);
		if (cpu_gate_is_interrupt(&gate)) cpu->ifl = 0;
		cpu->nt = 0;
		cpu->iopl = 0;
		cpu->tf = 0;
		putsegreg(cpu, regcs, target.selector);
		cpu->ip = gate.offset & 0xFFFF;
		cpu->cpl = target.target_cpl;
		return;
	}

	if (cpu_gate_is_32bit(&gate)) {
		frame_size = cpu_interrupt_frame_size32(0, 0, has_err);
		cpu_push_interrupt_frame32(cpu, 0, 0, cpu->segregs[regss], cpu->regs.longregs[regesp], old_flags, cpu->segregs[regcs], push_eip, has_err, err);
		if (cpu_gate_is_interrupt(&gate)) cpu->ifl = 0;
		cpu->nt = 0;
		cpu->tf = 0;
		putsegreg(cpu, regcs, target.selector);
		cpu->cpl = target.target_cpl;
		cpu->ip = gate.offset;
		return;
	}

	frame_size = cpu_interrupt_frame_size16(0, has_err);
	cpu_push_interrupt_frame16(cpu, 0, cpu->segregs[regss], cpu->regs.longregs[regesp], old_flags, cpu->segregs[regcs], push_eip, has_err, err);
	if (cpu_gate_is_interrupt(&gate)) cpu->ifl = 0;
	cpu->nt = 0;
	cpu->tf = 0;
	putsegreg(cpu, regcs, target.selector);
	cpu->ip = gate.offset & 0xFFFF;
}

static FUNC_FORCE_INLINE void cpu_callf(CPU_t* cpu, uint16_t selector, uint32_t ip) {
	SEGDESC_t desc;
	GATEDESC_t gate;
	CODETARGET_t target;
	uint32_t new_esp, old_esp, old_ss_base;
	uint16_t new_ss, old_ss;
	uint16_t task_selector;
	int old_stack_is32;
	uint8_t epl;

	if (!cpu->protected_mode || cpu->v86f) {
		if (cpu->isoper32) {
			pushl(cpu, cpu->segregs[regcs]);
			pushl(cpu, cpu->ip);
		}
		else {
			pushw(cpu, cpu->segregs[regcs]);
			pushw(cpu, cpu->ip);
		}
		cpu->ip = cpu->isoper32 ? ip : (ip & 0xFFFF);
		putsegreg(cpu, regcs, selector);
		return;
	}

	old_ss = cpu->segregs[regss];
	old_esp = cpu->regs.longregs[regesp];
	old_ss_base = cpu->segcache[regss];
	old_stack_is32 = cpu->segis32[regss];

	if (cpu_selector_offset(selector) == 0) {
		exception(cpu, 13, 0); //GP(0)
		return;
	}

	if (!cpu_read_segdesc(cpu, selector, &desc)) {
		exception(cpu, 13, cpu_selector_error_code(selector));
		return;
	}

	if (cpu_desc_is_code(&desc)) {
		if (!cpu_validate_direct_call_target(cpu, selector, &target)) {
			return;
		}
		if (cpu->isoper32) {
			cpu_push_far_return32(cpu, cpu->segregs[regcs], cpu->ip);
		}
		else {
			cpu_push_far_return16(cpu, cpu->segregs[regcs], cpu->ip);
		}
		putsegreg(cpu, regcs, target.selector);
		cpu->ip = cpu->isoper32 ? ip : (ip & 0xFFFF);
		return;
	}

	if (cpu_desc_is_task_gate(&desc) || cpu_desc_is_tss(&desc)) {
		if (!cpu_resolve_task_switch_target(cpu, selector, &desc, &task_selector)) {
			return;
		}
		task_switch(cpu, task_selector, TASK_SWITCH_REASON_CALL);
		return;
	}

	if (!cpu_desc_is_call_gate(&desc)) {
		exception(cpu, 13, cpu_selector_error_code(selector));
		return;
	}

	cpu_load_gate_desc(cpu, desc.addr, desc.access, desc.flags, &gate);
	epl = ((selector & 3) > cpu->cpl) ? (selector & 3) : cpu->cpl;
	if (epl > gate.dpl) {
		exception(cpu, 13, cpu_selector_error_code(selector));
		return;
	}
	if (!gate.present) {
		exception(cpu, 11, cpu_selector_error_code(selector));
		return;
	}
	if (!cpu_validate_gate_target_code(cpu, gate.target_selector, &target)) {
		return;
	}

	if (!target.outer) {
		if (cpu_gate_is_32bit(&gate)) {
			cpu_push_far_return32(cpu, cpu->segregs[regcs], cpu->ip);
			putsegreg(cpu, regcs, target.selector);
			cpu->ip = gate.offset;
		}
		else {
			cpu_push_far_return16(cpu, cpu->segregs[regcs], cpu->ip);
			putsegreg(cpu, regcs, target.selector);
			cpu->ip = gate.offset & 0xFFFF;
		}
		cpu->cpl = target.target_cpl;
		return;
	}

	if (!cpu_fetch_tss_stack(cpu, target.target_cpl, &new_ss, &new_esp)) {
		return;
	}

	if (!cpu_commit_validated_segment(cpu, regss, new_ss)) return;
	cpu->regs.longregs[regesp] = new_esp;
	cpu_copy_call_gate_params_sys(cpu, old_ss_base, old_esp, old_stack_is32, gate.param_count, cpu_gate_is_32bit(&gate));
	if (cpu_gate_is_32bit(&gate)) {
		cpu_writel_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 4) : (cpu->regs.wordregs[regsp] -= 4))), old_ss);
		cpu_writel_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 4) : (cpu->regs.wordregs[regsp] -= 4))), old_esp);
		cpu_push_far_return32_sys(cpu, cpu->segregs[regcs], cpu->ip);
		putsegreg(cpu, regcs, target.selector);
		cpu->ip = gate.offset;
	}
	else {
		cpu_writew_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 2) : (cpu->regs.wordregs[regsp] -= 2))), old_ss);
		cpu_writew_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 2) : (cpu->regs.wordregs[regsp] -= 2))), (uint16_t)old_esp);
		cpu_push_far_return16_sys(cpu, cpu->segregs[regcs], cpu->ip);
		putsegreg(cpu, regcs, target.selector);
		cpu->ip = gate.offset & 0xFFFF;
	}
	cpu->cpl = target.target_cpl;
}

static FUNC_FORCE_INLINE void cpu_jmpf(CPU_t* cpu, uint16_t selector, uint32_t ip) {
	SEGDESC_t desc;
	GATEDESC_t gate;
	CODETARGET_t target;
	uint16_t task_selector;
	uint8_t epl;

	if (!cpu->protected_mode || cpu->v86f) {
		cpu->ip = cpu->isoper32 ? ip : (ip & 0xFFFF);
		putsegreg(cpu, regcs, selector);
		return;
	}

	if (cpu_selector_offset(selector) == 0) {
		exception(cpu, 13, 0); //GP(0)
		return;
	}

	if (!cpu_read_segdesc(cpu, selector, &desc)) {
		exception(cpu, 13, cpu_selector_error_code(selector));
		return;
	}

	if (cpu_desc_is_code(&desc)) {
		if (!cpu_validate_direct_call_target(cpu, selector, &target)) {
			return;
		}
		putsegreg(cpu, regcs, target.selector);
		cpu->ip = cpu->isoper32 ? ip : (ip & 0xFFFF);
		return;
	}

	if (cpu_desc_is_task_gate(&desc) || cpu_desc_is_tss(&desc)) {
		if (!cpu_resolve_task_switch_target(cpu, selector, &desc, &task_selector)) {
			return;
		}
		task_switch(cpu, task_selector, TASK_SWITCH_REASON_JMP);
		return;
	}

	if (!cpu_desc_is_call_gate(&desc)) {
		exception(cpu, 13, cpu_selector_error_code(selector));
		return;
	}

	cpu_load_gate_desc(cpu, desc.addr, desc.access, desc.flags, &gate);
	epl = ((selector & 3) > cpu->cpl) ? (selector & 3) : cpu->cpl;
	if (epl > gate.dpl) {
		exception(cpu, 13, cpu_selector_error_code(selector));
		return;
	}
	if (!gate.present) {
		exception(cpu, 11, cpu_selector_error_code(selector));
		return;
	}
	if (!cpu_validate_gate_target_code(cpu, gate.target_selector, &target)) {
		return;
	}
	if (target.outer) {
		exception(cpu, 13, cpu_selector_error_code(gate.target_selector));
		return;
	}

	putsegreg(cpu, regcs, target.selector);
	cpu->ip = cpu_gate_is_32bit(&gate) ? gate.offset : (gate.offset & 0xFFFF);
	cpu->cpl = target.target_cpl;
}

static FUNC_FORCE_INLINE void cpu_retf(CPU_t* cpu, uint32_t adjust) {
	RETURNCS_t retcs;
	uint32_t new_ip, new_esp;
	uint32_t stack_ptr;
	uint16_t new_cs, new_ss;

	if (!cpu->protected_mode || cpu->v86f) {
		if (cpu->isoper32) {
			cpu->ip = popl(cpu);
			putsegreg(cpu, regcs, popl(cpu));
		}
		else {
			cpu->ip = popw(cpu);
			putsegreg(cpu, regcs, popw(cpu));
		}
		if (cpu->segis32[regss]) {
			cpu->regs.longregs[regesp] += adjust;
		}
		else {
			cpu->regs.wordregs[regsp] += (uint16_t)adjust;
		}
		return;
	}

	if (cpu->isoper32) {
		stack_ptr = cpu->segis32[regss] ? cpu->regs.longregs[regesp] : (uint32_t)cpu->regs.wordregs[regsp];
		new_ip = cpu_readl(cpu, cpu->segcache[regss] + stack_ptr);
		new_cs = cpu_readw(cpu, cpu->segcache[regss] + (cpu->segis32[regss] ? (stack_ptr + 4) : ((stack_ptr + 4) & 0xFFFF)));
		if (!cpu_validate_return_cs(cpu, new_cs, &retcs)) {
			return;
		}

		if (!retcs.outer) {
			putsegreg(cpu, regcs, new_cs);
			cpu->ip = new_ip;
			if (cpu->segis32[regss]) {
				cpu->regs.longregs[regesp] += 8 + adjust;
			}
			else {
				cpu->regs.wordregs[regsp] += (uint16_t)(8 + adjust);
			}
			return;
		}

		new_esp = cpu_readl(cpu, cpu->segcache[regss] + (cpu->segis32[regss] ? (stack_ptr + 8) : ((stack_ptr + 8) & 0xFFFF)));
		new_ss = cpu_readw(cpu, cpu->segcache[regss] + (cpu->segis32[regss] ? (stack_ptr + 12) : ((stack_ptr + 12) & 0xFFFF)));
		if (!cpu_validate_return_ss(cpu, new_ss, retcs.target_cpl)) {
			return;
		}

		if (!cpu_commit_validated_segment(cpu, regss, new_ss)) return;
		if (cpu->segis32[regss]) {
			cpu->regs.longregs[regesp] = new_esp + adjust;
		}
		else {
			cpu->regs.wordregs[regsp] = (uint16_t)(new_esp + adjust);
		}
		putsegreg(cpu, regcs, new_cs);
		cpu->ip = new_ip;
		cpu->cpl = retcs.target_cpl;
		cpu_cleanup_outer_return_segments(cpu, retcs.target_cpl);
	}
	else {
		stack_ptr = cpu->segis32[regss] ? cpu->regs.longregs[regesp] : (uint32_t)cpu->regs.wordregs[regsp];
		new_ip = cpu_readw(cpu, cpu->segcache[regss] + stack_ptr);
		new_cs = cpu_readw(cpu, cpu->segcache[regss] + (cpu->segis32[regss] ? (stack_ptr + 2) : ((stack_ptr + 2) & 0xFFFF)));
		if (!cpu_validate_return_cs(cpu, new_cs, &retcs)) {
			return;
		}

		if (!retcs.outer) {
			putsegreg(cpu, regcs, new_cs);
			cpu->ip = new_ip;
			if (cpu->segis32[regss]) {
				cpu->regs.longregs[regesp] += 4 + adjust;
			}
			else {
				cpu->regs.wordregs[regsp] += (uint16_t)(4 + adjust);
			}
			return;
		}

		new_esp = cpu_readw(cpu, cpu->segcache[regss] + (cpu->segis32[regss] ? (stack_ptr + 4) : ((stack_ptr + 4) & 0xFFFF)));
		new_ss = cpu_readw(cpu, cpu->segcache[regss] + (cpu->segis32[regss] ? (stack_ptr + 6) : ((stack_ptr + 6) & 0xFFFF)));
		if (!cpu_validate_return_ss(cpu, new_ss, retcs.target_cpl)) {
			return;
		}

		if (!cpu_commit_validated_segment(cpu, regss, new_ss)) return;
		if (cpu->segis32[regss]) {
			cpu->regs.longregs[regesp] = new_esp + adjust;
		}
		else {
			cpu->regs.wordregs[regsp] = (uint16_t)(new_esp + adjust);
		}
		putsegreg(cpu, regcs, new_cs);
		cpu->ip = new_ip;
		cpu->cpl = retcs.target_cpl;
		cpu_cleanup_outer_return_segments(cpu, retcs.target_cpl);
	}
}

static FUNC_FORCE_INLINE void cpu_iret(CPU_t* cpu) {
	uint8_t current_cpl = cpu->cpl;
	uint32_t old_esp, old_eflags, merged_flags, new_esp, new_cs, new_eip, new_eflags, new_ss, new_es, new_ds, new_fs, new_gs;
	RETURNCS_t retcs;

	if (!cpu->protected_mode) { //real mode
		old_eflags = makeflagsword(cpu);
		if (cpu->isoper32) {
			new_eip = popl(cpu);
			new_cs = popl(cpu);
			new_eflags = popl(cpu);
			putsegreg(cpu, regcs, new_cs);
			cpu->ip = new_eip;
			cpu_restore_iret_flags(cpu, new_eflags, current_cpl, current_cpl);
			return;
		}
		new_eip = popw(cpu);
		new_cs = popw(cpu);
		new_eflags = (uint32_t)popw(cpu) | (old_eflags & 0xFFFF0000u);
		putsegreg(cpu, regcs, new_cs);
		cpu->ip = new_eip;
		cpu_restore_iret_flags(cpu, new_eflags, current_cpl, current_cpl);
		return;
	}

	//protected mode
	if (cpu->v86f) {
		old_eflags = makeflagsword(cpu);
		if (cpu->iopl < 3) {
			exception(cpu, 13, 0); //GP(0)
			return;
		}
		if (cpu->isoper32) {
			new_eip = cpu_readl(cpu, cpu->segcache[regss] + (cpu->regs.wordregs[regsp] & 0xFFFF));
			new_cs = cpu_readl(cpu, cpu->segcache[regss] + ((cpu->regs.wordregs[regsp] + 4) & 0xFFFF));
			new_eflags = cpu_readl(cpu, cpu->segcache[regss] + ((cpu->regs.wordregs[regsp] + 8) & 0xFFFF));
			putsegreg(cpu, regcs, new_cs);
			cpu->ip = new_eip;
			cpu->regs.wordregs[regsp] += 12;
			cpu_restore_iret_flags(cpu, new_eflags, current_cpl, 3);
			return;
		}
		new_eip = cpu_readw(cpu, cpu->segcache[regss] + (cpu->regs.wordregs[regsp] & 0xFFFF));
		new_cs = cpu_readw(cpu, cpu->segcache[regss] + ((cpu->regs.wordregs[regsp] + 2) & 0xFFFF));
		new_eflags = (uint32_t)cpu_readw(cpu, cpu->segcache[regss] + ((cpu->regs.wordregs[regsp] + 4) & 0xFFFF)) | (old_eflags & 0xFFFF0000u);
		putsegreg(cpu, regcs, new_cs);
		cpu->ip = new_eip;
		cpu->regs.wordregs[regsp] += 6;
		cpu_restore_iret_flags(cpu, new_eflags, current_cpl, 3);
		return;
	}

	if (cpu->nt) {
		uint16_t backlink = cpu_readw(cpu, cpu->trbase + 0x00);
		task_switch(cpu, backlink, TASK_SWITCH_REASON_IRET);
		return;
	}

	if (cpu->isoper32) {
		new_eip = cpu_readl(cpu, cpu->segcache[regss] + cpu->regs.longregs[regesp]);
		new_cs = cpu_readw(cpu, cpu->segcache[regss] + cpu->regs.longregs[regesp] + 4);
		new_eflags = cpu_readl(cpu, cpu->segcache[regss] + cpu->regs.longregs[regesp] + 8);
		old_eflags = makeflagsword(cpu);

		if (new_eflags & 0x20000) { //if V86 flag set in new flags, we're returning to V86 mode
			if (current_cpl != 0) {
				exception(cpu, 13, 0); //GP(0)
				return;
			}
			new_esp = cpu_readl(cpu, cpu->segcache[regss] + cpu->regs.longregs[regesp] + 12);
			new_ss = cpu_readl(cpu, cpu->segcache[regss] + cpu->regs.longregs[regesp] + 16);
			new_es = cpu_readl(cpu, cpu->segcache[regss] + cpu->regs.longregs[regesp] + 20);
			new_ds = cpu_readl(cpu, cpu->segcache[regss] + cpu->regs.longregs[regesp] + 24);
			new_fs = cpu_readl(cpu, cpu->segcache[regss] + cpu->regs.longregs[regesp] + 28);
			new_gs = cpu_readl(cpu, cpu->segcache[regss] + cpu->regs.longregs[regesp] + 32);
			merged_flags = old_eflags;
			merged_flags = (merged_flags & ~(EFLAGS_CF | EFLAGS_PF | EFLAGS_AF | EFLAGS_ZF | EFLAGS_SF | EFLAGS_TF | EFLAGS_IF | EFLAGS_DF | EFLAGS_OF | EFLAGS_IOPL | EFLAGS_NT | EFLAGS_VM | (1u << 18) | (1u << 21))) |
				(new_eflags & (EFLAGS_CF | EFLAGS_PF | EFLAGS_AF | EFLAGS_ZF | EFLAGS_SF | EFLAGS_TF | EFLAGS_IF | EFLAGS_DF | EFLAGS_OF | EFLAGS_IOPL | EFLAGS_NT | EFLAGS_VM | (1u << 18) | (1u << 21)));
			merged_flags = (merged_flags & ~EFLAGS_RF) | (old_eflags & EFLAGS_RF);
			cpu->cf = (merged_flags & EFLAGS_CF) ? 1 : 0;
			cpu->pf = (merged_flags & EFLAGS_PF) ? 1 : 0;
			cpu->af = (merged_flags & EFLAGS_AF) ? 1 : 0;
			cpu->zf = (merged_flags & EFLAGS_ZF) ? 1 : 0;
			cpu->sf = (merged_flags & EFLAGS_SF) ? 1 : 0;
			cpu->tf = (merged_flags & EFLAGS_TF) ? 1 : 0;
			cpu->ifl = (merged_flags & EFLAGS_IF) ? 1 : 0;
			cpu->df = (merged_flags & EFLAGS_DF) ? 1 : 0;
			cpu->of = (merged_flags & EFLAGS_OF) ? 1 : 0;
			cpu->iopl = (uint8_t)((merged_flags & EFLAGS_IOPL) >> 12);
			cpu->nt = (merged_flags & EFLAGS_NT) ? 1 : 0;
			cpu->rf = (old_eflags & EFLAGS_RF) ? 1 : 0;
			cpu->v86f = 1;
			cpu->acf = (merged_flags & (1u << 18)) ? 1 : 0;
			cpu->idf = (merged_flags & (1u << 21)) ? 1 : 0;
			cpu->cpl = 3;
			putsegreg(cpu, regss, new_ss);
			cpu->regs.longregs[regesp] = new_esp;
			putsegreg(cpu, reges, new_es);
			putsegreg(cpu, regds, new_ds);
			putsegreg(cpu, regfs, new_fs);
			putsegreg(cpu, reggs, new_gs);
			putsegreg(cpu, regcs, new_cs);
			cpu->ip = new_eip & 0xFFFF;
			return;
		}
	}
	else { //16-bit mode
		new_eip = cpu_readw(cpu, cpu->segcache[regss] + (cpu->regs.wordregs[regsp] & 0xFFFF));
		new_cs = cpu_readw(cpu, cpu->segcache[regss] + ((cpu->regs.wordregs[regsp] + 2) & 0xFFFF));
		new_eflags = cpu_readw(cpu, cpu->segcache[regss] + ((cpu->regs.wordregs[regsp] + 4) & 0xFFFF)) | (makeflagsword(cpu) & 0xFFFF0000);
	}

	if (!cpu_validate_return_cs(cpu, (uint16_t)new_cs, &retcs)) {
		return;
	}
	if (!retcs.outer) { //return same level
		putsegreg(cpu, regcs, new_cs);
		cpu->cpl = retcs.target_cpl;
		cpu->ip = new_eip;
		cpu_restore_iret_flags(cpu, new_eflags, current_cpl, retcs.target_cpl);
		cpu->regs.longregs[regesp] += cpu->isoper32 ? 12 : 6;
		return;
	}

	//return outer level
	if (cpu->isoper32) { //32-bit
		new_esp = cpu_readl(cpu, cpu->segcache[regss] + cpu->regs.longregs[regesp] + 12);
		new_ss = cpu_readw(cpu, cpu->segcache[regss] + cpu->regs.longregs[regesp] + 16);
	}
	else { //16-bit
		new_esp = cpu_readw(cpu, cpu->segcache[regss] + ((cpu->regs.longregs[regesp] + 6) & 0xFFFF));
		new_ss = cpu_readw(cpu, cpu->segcache[regss] + ((cpu->regs.longregs[regesp] + 8) & 0xFFFF));
	}
	if (!cpu_validate_return_ss(cpu, (uint16_t)new_ss, retcs.target_cpl)) {
		return;
	}

	putsegreg(cpu, regcs, new_cs);
	cpu->ip = new_eip;
	cpu_restore_iret_flags(cpu, new_eflags, current_cpl, retcs.target_cpl);
	putsegreg(cpu, regss, new_ss);
	cpu->regs.longregs[regesp] = new_esp;
	cpu->cpl = retcs.target_cpl;
	cpu_cleanup_outer_return_segments(cpu, retcs.target_cpl);
	return;

	printf("Fell through IRET code!!\n");
}

static void cpu_begin_interrupt_shadow(CPU_t* cpu) {
	// STI, MOV/POP SS, and LSS inhibit maskable interrupts until after the next instruction.
	cpu->interrupt_inhibit = 1;
}

int cpu_interruptCheck(CPU_t* cpu, I8259_t* i8259, int slave) {
	//if (cpu->protected) return;
	/* get next interrupt from the i8259, if any */
	if (!cpu->trap_toggle && (cpu->ifl && (i8259->irr & (~i8259->imr)))) {
		//if (i8259->irr & (~i8259->imr) & (~i8259->isr)) {
		uint8_t irqnum = i8259_nextintr(i8259);
		uint8_t intnum;

		if ((irqnum == 2) && !slave && (i8259->slave != NULL)) {
			I8259_t* slave_pic = (I8259_t*)i8259->slave;

			if (slave_pic->irr & (~slave_pic->imr)) {
				irqnum = i8259_nextintr(slave_pic);
				intnum = irqnum + slave_pic->intoffset;
			}
			else {
				i8259->isr &= ~(1 << 2);
				return 0;
			}
		}
		else {
			intnum = irqnum + i8259->intoffset;
		}
		//if (cpu->protected) intnum += 0x20; else intnum += 8;
		//printf("Intnum: %02X\n", intnum);
		cpu->hltstate = 0;
		cpu_intcall(cpu, intnum, INT_SOURCE_HARDWARE, 0);
		return 1;
	}
	return 0;
}


void op_ext_00(CPU_t* cpu) {
	modregrm(cpu);
	//debug_log(DEBUG_DETAIL, "0F 00 reg %u", cpu->reg);
	//printf("0F 00 reg %u", cpu->reg);
	switch (cpu->reg) {
	case 0: //SLDT
		writerm16(cpu, cpu->rm, cpu->ldt_selector);
		break;
	case 1: //STR
		writerm16(cpu, cpu->rm, cpu->tr_selector);
		break;
	case 2: //LLDT
		cpu->temp16 = readrm16(cpu, cpu->rm);
		cpu->ldt_selector = cpu->temp16;
		cpu->tempaddr32 = cpu->gdtr + (uint32_t)(cpu->temp16 & ~7);
		cpu->ldtl = (uint32_t)cpu_readw_sys(cpu, cpu->tempaddr32) | ((uint32_t)(cpu_read_sys(cpu, cpu->tempaddr32 + 6) & 0xF) << 16);
		if (cpu_read_sys(cpu, cpu->tempaddr32 + 6) & 0x80) {
			cpu->ldtl <<= 12;
			cpu->ldtl |= 0xFFF;
		}
		cpu->ldtr = cpu_readw_sys(cpu, cpu->tempaddr32 + 2) | ((uint32_t)cpu_read_sys(cpu, cpu->tempaddr32 + 4) << 16) | ((uint32_t)cpu_read_sys(cpu, cpu->tempaddr32 + 7) << 24);
		//debug_log(DEBUG_DETAIL, "Loaded LDT from %08X (LDT selector %04X (%04X), location is %08X, limit %u)", cpu->tempaddr32, cpu->ldt_selector, cpu->ldt_selector>>3, cpu->ldtr, cpu->ldtl);
		break;
	case 3: //LTR
	{
		uint8_t access_byte, dpl, present, type, cpl;
		cpu->temp16 = readrm16(cpu, cpu->rm);
		cpu->tr_selector = cpu->temp16;
		cpu->tempaddr32 = cpu->gdtr + ((cpu->temp16 >> 3) * 8);
		access_byte = cpu_read_sys(cpu, cpu->tempaddr32 + 5);
		type = access_byte & 0x0F;
		dpl = (access_byte >> 5) & 0x03;
		cpl = cpu->segregs[regcs] & 3;
		present = (access_byte >> 7) & 0x01;

		if ((type != 0x9) && (type != 0xB) && (type != 0x3) && (type != 0x7)) {
			//cpu_exception(cpu, 13, cpu->temp16); //GP
			//return;
		}
		else if (!present) {
			//cpu_intcall(cpu, 11, INT_SOURCE_EXCEPTION, cpu->temp16); //NP
			exception(cpu, 11, cpu->temp16); //NP
			return;
		}
		else if (cpl > dpl) {
			//debug_log(DEBUG_DETAIL, "cpl > dpl check failed");
			exception(cpu, 13, cpu->temp16); //GP
			return;
		}
		//TODO: TSS busy check
		else {
			uint8_t new_type;
			cpu->trtype = type;
			cpu->trlimit = cpu_readw_sys(cpu, cpu->tempaddr32) + 1;
			cpu->trbase = (uint32_t)cpu_read_sys(cpu, cpu->tempaddr32 + 2) |
				((uint32_t)cpu_read_sys(cpu, cpu->tempaddr32 + 3) << 8) |
				((uint32_t)cpu_read_sys(cpu, cpu->tempaddr32 + 4) << 16) |
				((uint32_t)cpu_read_sys(cpu, cpu->tempaddr32 + 7) << 24);
			new_type = (type == 0x9) ? 0xB : 0x7;
			cpu_write_sys(cpu, cpu->tempaddr32 + 5, (access_byte & 0xF0) | new_type);
			//debug_log(DEBUG_DETAIL, "Loaded TR from %08X (TR location is %08X, limit %u)", cpu->tempaddr32, cpu->trbase, cpu->trlimit);
		}
		break;
	}
	case 4: //VERR
		//exit(0);
		printf("VERR");
		cpu->zf = 1;
		break;
	case 5: //VERW
		//exit(0);
		printf("VERW");
		cpu->zf = 1;
		break;
	default:
		//cpu_intcall(cpu, 6, INT_SOURCE_EXCEPTION, 0);
		exception(cpu, 6, 0); //UD
	}
}


void op_ext_01(CPU_t* cpu) {
	modregrm(cpu);
	//debug_log(DEBUG_DETAIL, "0F 01 reg %u", cpu->reg);
	//printf("0F 01 reg %u", cpu->reg);
	switch (cpu->reg) {
	case 0: //SGDT
		getea(cpu, cpu->rm);
		cpu_writew(cpu, cpu->ea, cpu->gdtl);
		if (cpu->isoper32) {
			cpu_writel(cpu, cpu->ea + 2, cpu->gdtr);
		}
		else {
			cpu_writew(cpu, cpu->ea + 2, (uint16_t)cpu->gdtr);
			cpu_write(cpu, cpu->ea + 4, (uint8_t)(cpu->gdtr >> 16));
			//cpu_write(cpu, cpu->ea + 5, (uint8_t)(cpu->gdtr >> 24));
			//cpu_write(cpu, cpu->ea + 5, 0);
		}
		break;
	case 1: //SIDT
		getea(cpu, cpu->rm);
		cpu_writew(cpu, cpu->ea, cpu->idtl);
		if (cpu->isoper32) {
			cpu_writel(cpu, cpu->ea + 2, cpu->idtr);
		}
		else {
			cpu_writew(cpu, cpu->ea + 2, (uint16_t)cpu->idtr);
			cpu_write(cpu, cpu->ea + 4, (uint8_t)(cpu->idtr >> 16));
			//cpu_write(cpu, cpu->ea + 5, (uint8_t)(cpu->idtr >> 24));
			//cpu_write(cpu, cpu->ea + 5, 0);
		}
		break;
	case 2: //LGDT
		if (cpu->protected_mode && (cpu->cpl > 0)) {
			debug_log(DEBUG_INFO, "Attempted to use LGDT when CPU was already in protected mode and CPL>0!");
			exception(cpu, 13, 0); //GP
			break;
		}
		getea(cpu, cpu->rm);
		cpu->gdtl = cpu_readw(cpu, cpu->ea);
		if (cpu->isoper32) {
			cpu->gdtr = cpu_readl(cpu, cpu->ea + 2);
		}
		else {
			cpu->gdtr = cpu_readw(cpu, cpu->ea + 2) | ((uint32_t)cpu_read(cpu, cpu->ea + 4) << 16);
		}
		//debug_log(DEBUG_DETAIL, "Loaded GDT from %08X (GDT location is %08X, limit %u)", cpu->ea, cpu->gdtr, cpu->gdtl);
		break;
	case 3: //LIDT
		getea(cpu, cpu->rm);
		cpu->idtl = cpu_readw(cpu, cpu->ea);
		if (cpu->isoper32) {
			cpu->idtr = cpu_readl(cpu, cpu->ea + 2);
		}
		else {
			cpu->idtr = cpu_readw(cpu, cpu->ea + 2) | ((uint32_t)cpu_read(cpu, cpu->ea + 4) << 16);
		}
		//showops = 1;
		//debug_log(DEBUG_DETAIL, "Loaded IDT from %08X (IDT location is %08X, limit %u)", cpu->ea, cpu->idtr, cpu->idtl);
		break;
	case 4: //SMSW
		writerm16(cpu, cpu->rm, ((uint16_t)cpu->cr[0] | (cpu->have387 ? 0 : 4))); //still use 32-bit write, upper bits are zeros in destination
		break;
	case 6: //LMSW
	{
		uint32_t oldpm = cpu->cr[0] & 1;
		cpu->cr[0] = (cpu->cr[0] & 0xFFFFFFE1) | (readrm16(cpu, cpu->rm) & 0x1E);
		cpu->cr[0] |= readrm16(cpu, cpu->rm) & 0x11;
		if ((cpu->cr[0] & 1) && !oldpm) {
			cpu->protected_mode = 1;
			cpu->ifl = 0;
			//debug_log(DEBUG_DETAIL, "Entered protected mode");
		}

		break;
	}
	case 7: //INVLPG
		if (cpu->cpl > 0) {
			exception(cpu, 13, 0); //GP(0)
			break;
		}
		if (cpu->mode == 3) {
			exception(cpu, 6, 0); //UD
			break;
		}
		getea(cpu, cpu->rm);
		if (memory_paging_enabled(cpu)) {
			memory_tlb_invalidate_page(cpu, cpu->ea);
		}
		break;
	}
}

//LAR
void op_ext_02(CPU_t* cpu) {
	SEGDESC_t desc;
	uint16_t selector;
	uint8_t epl;
	uint32_t lar;

	if (!cpu->protected_mode || cpu->v86f) {
		exception(cpu, 6, 0); //UD
		return;
	}

	modregrm(cpu);
	selector = readrm16(cpu, cpu->rm);
	if (cpu->doexception) {
		return;
	}

	cpu->zf = 0;
	if (cpu_selector_offset(selector) == 0) {
		return;
	}
	if (!cpu_read_segdesc(cpu, selector, &desc)) {
		return;
	}

	if (cpu_desc_is_system(&desc)) {
		switch (cpu_desc_type(&desc)) {
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xB:
		case 0xC:
		case 0xE:
		case 0xF:
			break;
		default:
			return;
		}
	}

	epl = ((selector & 3) > cpu->cpl) ? (selector & 3) : cpu->cpl;
	if (!(cpu_desc_is_code(&desc) && cpu_desc_is_conforming_code(&desc)) && (desc.dpl < epl)) {
		return;
	}

	lar = ((uint32_t)desc.access << 8) | (((uint32_t)desc.flags & 0xF0) << 16);
	if (cpu->isoper32) {
		putreg32(cpu, cpu->reg, lar);
	}
	else {
		putreg16(cpu, cpu->reg, (uint16_t)lar);
	}
	cpu->zf = 1;
}

//LSL
void op_ext_03(CPU_t* cpu) {
	SEGDESC_t desc;
	uint16_t selector;
	uint8_t epl;
	uint32_t limit;

	if (!cpu->protected_mode || cpu->v86f) {
		exception(cpu, 6, 0); //UD
		return;
	}

	modregrm(cpu);
	selector = readrm16(cpu, cpu->rm);
	if (cpu->doexception) {
		return;
	}

	cpu->zf = 0;
	if (cpu_selector_offset(selector) == 0) {
		return;
	}
	if (!cpu_read_segdesc(cpu, selector, &desc)) {
		return;
	}

	if (cpu_desc_is_system(&desc)) {
		switch (cpu_desc_type(&desc)) {
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x9:
		case 0xB:
			break;
		default:
			return;
		}
	}

	epl = ((selector & 3) > cpu->cpl) ? (selector & 3) : cpu->cpl;
	if (!(cpu_desc_is_code(&desc) && cpu_desc_is_conforming_code(&desc)) && (desc.dpl < epl)) {
		return;
	}

	limit = (uint32_t)cpu_readw_sys(cpu, desc.addr) | ((uint32_t)(desc.flags & 0x0F) << 16);
	if (desc.flags & 0x80) {
		limit <<= 12;
		limit |= 0xFFF;
	}
	if (cpu->isoper32) {
		putreg32(cpu, cpu->reg, limit);
	}
	else {
		putreg16(cpu, cpu->reg, (uint16_t)limit);
	}
	cpu->zf = 1;
}

//CLTS
void op_ext_06(CPU_t* cpu) {
	if (cpu->cpl > 0) {
		exception(cpu, 13, 0); //GP(0)
		return;
	}
	cpu->cr[0] &= ~8;
}

//INVD
//WBINVD
void op_ext_08_09(CPU_t* cpu) {

}

//MOV r32, CRn
void op_ext_20(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->v86f || (cpu->cpl > 0)) {
		exception(cpu, 13, 0); //GP
		return;
	}
	if ((cpu->mode != 3) || ((cpu->reg != 0) && (cpu->reg != 2) && (cpu->reg != 3) && (cpu->reg != 4))) {
		cpu->ip = firstip;
		exception(cpu, 6, 0); //UD
		return;
	}
	cpu->regs.longregs[cpu->rm] = cpu->cr[cpu->reg];
	//if (cpu->reg == 0) cpu->regs.longregs[cpu->rm] |= (cpu->have387 ? 0 : 4); //TODO: use this?
}

//MOV CRn, r32
void op_ext_22(CPU_t* cpu) {
	uint32_t new_cr;
	int old_paging_enabled;

	modregrm(cpu);
	if (cpu->v86f || (cpu->cpl > 0)) {
		exception(cpu, 13, 0); //GP
		return;
	}
	if ((cpu->mode != 3) || ((cpu->reg != 0) && (cpu->reg != 2) && (cpu->reg != 3) && (cpu->reg != 4))) {
		cpu->ip = firstip;
		exception(cpu, 6, 0); //UD
		return;
	}
	//debug_log(DEBUG_DETAIL, "CR%u <- %08X", cpu->reg, cpu->regs.longregs[cpu->rm]);
	//cpu->cr[0] &= 0x00000037; //mask out bits 29 (NW) and 30 (CD) which are not present on 386
	switch (cpu->reg) {
	case 0: //CR0
		old_paging_enabled = memory_paging_enabled(cpu);
		new_cr = cpu->regs.longregs[cpu->rm];
		if ((new_cr & 0x80000000) && ((new_cr & 1) == 0)) {
			exception(cpu, 13, 0); //GP(0)
			return;
		}
		cpu->cr[0] = new_cr;
		if (cpu->cr[0] & 1) {
			cpu->protected_mode = 1;
			//debug_log(DEBUG_DETAIL, "Entered protected mode");
			cpu->paging = memory_paging_enabled(cpu) ? 1 : 0;
		}
		else {
			cpu->protected_mode = 0;
			cpu->paging = 0;
			cpu->usegdt = 0;
			cpu->isoper32 = 0;
			cpu->isaddr32 = 0;
			cpu->isCS32 = 0;
			memset(cpu->segis32, 0, sizeof(cpu->segis32));
			//debug_log(DEBUG_DETAIL, "Entered real mode");
		}
		if (old_paging_enabled != memory_paging_enabled(cpu)) {
			memory_tlb_flush(cpu);
		}
		break;
	case 3: //CR3
		cpu->cr[3] = cpu->regs.longregs[cpu->rm];
		memory_tlb_flush(cpu);
		break;
	case 4: //CR4
		cpu->cr[4] = cpu->regs.longregs[cpu->rm];
		break;
	default:
		cpu->cr[cpu->reg] = cpu->regs.longregs[cpu->rm];
		break;
	}
}

//MOV r32, DBn
void op_ext_21(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->v86f || (cpu->cpl > 0)) {
		exception(cpu, 13, 0); //GP
		return;
	}
	if (cpu->mode != 3) {
		cpu->ip = firstip;
		exception(cpu, 6, 0); //UD
		return;
	}
	cpu->regs.longregs[cpu->rm] = cpu->dr[cpu->reg];
}

//MOV DBn, r32
void op_ext_23(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->v86f || (cpu->cpl > 0)) {
		exception(cpu, 13, 0); //GP
		return;
	}
	if (cpu->mode != 3) {
		cpu->ip = firstip;
		exception(cpu, 6, 0); //UD
		return;
	}
	cpu->dr[cpu->reg] = cpu->regs.longregs[cpu->rm];
}


void op_ext_24_26(CPU_t* cpu) {
	modregrm(cpu);
}

//WRMSR
void op_ext_30(CPU_t* cpu) {

}

//RDTSC (not accurate, it should really be clock cycles)
void op_ext_31(CPU_t* cpu) {
	cpu->regs.longregs[regedx] = cpu->totalexec >> 32;
	cpu->regs.longregs[regeax] = (uint32_t)cpu->totalexec;
}

//RDMSR
void op_ext_32(CPU_t* cpu) {
	uint64_t msr_value;

	if (cpu->v86f || (cpu->cpl > 0)) {
		exception(cpu, 13, 0); //GP
		return;
	}

	switch (cpu->regs.longregs[regecx]) {
	case 0x10: // TSC
		msr_value = cpu->totalexec;
		break;
	case 0x8B: // IA32_BIOS_SIGN_ID / microcode signature
		msr_value = 0;
		break;
	default:
		msr_value = 0;
		break;
	}

	cpu->regs.longregs[regeax] = (uint32_t)msr_value;
	cpu->regs.longregs[regedx] = (uint32_t)(msr_value >> 32);
}


//CMOVO
void op_ext_40(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->of) {
		if (cpu->isoper32) {
			putreg32(cpu, cpu->reg, readrm32(cpu, cpu->rm));
		}
		else {
			putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
		}
	}
}

//CMOVNO
void op_ext_41(CPU_t* cpu) {
	modregrm(cpu);
	if (!cpu->of) {
		if (cpu->isoper32) {
			putreg32(cpu, cpu->reg, readrm32(cpu, cpu->rm));
		}
		else {
			putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
		}
	}
}

//CMOVB
void op_ext_42(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->cf) {
		if (cpu->isoper32) {
			putreg32(cpu, cpu->reg, readrm32(cpu, cpu->rm));
		}
		else {
			putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
		}
	}
}

//CMOVNB
void op_ext_43(CPU_t* cpu) {
	modregrm(cpu);
	if (!cpu->cf) {
		if (cpu->isoper32) {
			putreg32(cpu, cpu->reg, readrm32(cpu, cpu->rm));
		}
		else {
			putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
		}
	}
}

//CMOVZ
void op_ext_44(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->zf) {
		if (cpu->isoper32) {
			putreg32(cpu, cpu->reg, readrm32(cpu, cpu->rm));
		}
		else {
			putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
		}
	}
}

//CMOVNZ
void op_ext_45(CPU_t* cpu) {
	modregrm(cpu);
	if (!cpu->zf) {
		if (cpu->isoper32) {
			putreg32(cpu, cpu->reg, readrm32(cpu, cpu->rm));
		}
		else {
			putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
		}
	}
}

//CMOVBE
void op_ext_46(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->cf || cpu->zf) {
		if (cpu->isoper32) {
			putreg32(cpu, cpu->reg, readrm32(cpu, cpu->rm));
		}
		else {
			putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
		}
	}
}

//CMOVNBE
void op_ext_47(CPU_t* cpu) {
	modregrm(cpu);
	if (!cpu->cf && !cpu->zf) {
		if (cpu->isoper32) {
			putreg32(cpu, cpu->reg, readrm32(cpu, cpu->rm));
		}
		else {
			putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
		}
	}
}

//CMOVS
void op_ext_48(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->sf) {
		if (cpu->isoper32) {
			putreg32(cpu, cpu->reg, readrm32(cpu, cpu->rm));
		}
		else {
			putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
		}
	}
}

//CMOVNS
void op_ext_49(CPU_t* cpu) {
	modregrm(cpu);
	if (!cpu->sf) {
		if (cpu->isoper32) {
			putreg32(cpu, cpu->reg, readrm32(cpu, cpu->rm));
		}
		else {
			putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
		}
	}
}

//CMOVP
void op_ext_4A(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->pf) {
		if (cpu->isoper32) {
			putreg32(cpu, cpu->reg, readrm32(cpu, cpu->rm));
		}
		else {
			putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
		}
	}
}

//CMOVNP
void op_ext_4B(CPU_t* cpu) {
	modregrm(cpu);
	if (!cpu->pf) {
		if (cpu->isoper32) {
			putreg32(cpu, cpu->reg, readrm32(cpu, cpu->rm));
		}
		else {
			putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
		}
	}
}

//CMOVL
void op_ext_4C(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->sf != cpu->of) {
		if (cpu->isoper32) {
			putreg32(cpu, cpu->reg, readrm32(cpu, cpu->rm));
		}
		else {
			putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
		}
	}
}

//CMOVNL
void op_ext_4D(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->sf == cpu->of) {
		if (cpu->isoper32) {
			putreg32(cpu, cpu->reg, readrm32(cpu, cpu->rm));
		}
		else {
			putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
		}
	}
}

//CMOVLE
void op_ext_4E(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->zf || (cpu->sf != cpu->of)) {
		if (cpu->isoper32) {
			putreg32(cpu, cpu->reg, readrm32(cpu, cpu->rm));
		}
		else {
			putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
		}
	}
}

//CMOVNLE
void op_ext_4F(CPU_t* cpu) {
	modregrm(cpu);
	if (!cpu->zf && (cpu->sf == cpu->of)) {
		if (cpu->isoper32) {
			putreg32(cpu, cpu->reg, readrm32(cpu, cpu->rm));
		}
		else {
			putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
		}
	}
}

// 80 JO Jb
void op_ext_80(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->temp32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
	}
	else {
		cpu->temp32 = (int32_t)(int16_t)getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
	}
	if (cpu->of) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

// 81 JNO Jb
void op_ext_81(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->temp32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
	}
	else {
		cpu->temp32 = (int32_t)(int16_t)getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
	}
	if (!cpu->of) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

// 82 JB Jb
void op_ext_82(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->temp32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
	}
	else {
		cpu->temp32 = (int32_t)(int16_t)getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
	}
	if (cpu->cf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

// 83 JNB Jb
void op_ext_83(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->temp32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
	}
	else {
		cpu->temp32 = (int32_t)(int16_t)getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
	}
	if (!cpu->cf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

// 84 JZ Jb
void op_ext_84(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->temp32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
	}
	else {
		cpu->temp32 = (int32_t)(int16_t)getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
	}
	if (cpu->zf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

// 85 JNZ Jb
void op_ext_85(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->temp32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
	}
	else {
		cpu->temp32 = (int32_t)(int16_t)getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
	}
	if (!cpu->zf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

// 86 JBE Jb
void op_ext_86(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->temp32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
	}
	else {
		cpu->temp32 = (int32_t)(int16_t)getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
	}
	if (cpu->cf || cpu->zf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

// 87 JA Jb
void op_ext_87(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->temp32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
	}
	else {
		cpu->temp32 = (int32_t)(int16_t)getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
	}
	if (!cpu->cf && !cpu->zf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

// 88 JS Jb
void op_ext_88(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->temp32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
	}
	else {
		cpu->temp32 = (int32_t)(int16_t)getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
	}
	if (cpu->sf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

// 89 JNS Jb
void op_ext_89(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->temp32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
	}
	else {
		cpu->temp32 = (int32_t)(int16_t)getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
	}
	if (!cpu->sf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

// 8A JPE Jb
void op_ext_8A(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->temp32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
	}
	else {
		cpu->temp32 = (int32_t)(int16_t)getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
	}
	if (cpu->pf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

// 8B JPO Jb
void op_ext_8B(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->temp32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
	}
	else {
		cpu->temp32 = (int32_t)(int16_t)getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
	}
	if (!cpu->pf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

// 8C JL Jb
void op_ext_8C(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->temp32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
	}
	else {
		cpu->temp32 = (int32_t)(int16_t)getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
	}
	if (cpu->sf != cpu->of) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

// 8D JGE Jb
void op_ext_8D(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->temp32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
	}
	else {
		cpu->temp32 = (int32_t)(int16_t)getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
	}
	if (cpu->sf == cpu->of) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

// 8E JLE Jb
void op_ext_8E(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->temp32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
	}
	else {
		cpu->temp32 = (int32_t)(int16_t)getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
	}
	if ((cpu->sf != cpu->of) || cpu->zf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

// 8F JG Jb
void op_ext_8F(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->temp32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
	}
	else {
		cpu->temp32 = (int32_t)(int16_t)getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
	}
	if (!cpu->zf && (cpu->sf == cpu->of)) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

// 90 SETO Jb
void op_ext_90(CPU_t* cpu) {
	modregrm(cpu);
	writerm8(cpu, cpu->rm, (cpu->of) ? 1 : 0);
}

// 91 SETNO Jb
void op_ext_91(CPU_t* cpu) {
	modregrm(cpu);
	writerm8(cpu, cpu->rm, (!cpu->of) ? 1 : 0);
}

// 92 SETB Jb
void op_ext_92(CPU_t* cpu) {
	modregrm(cpu);
	writerm8(cpu, cpu->rm, (cpu->cf) ? 1 : 0);
}

// 93 SETNB Jb
void op_ext_93(CPU_t* cpu) {
	modregrm(cpu);
	writerm8(cpu, cpu->rm, (!cpu->cf) ? 1 : 0);
}

// 94 SETZ Jb
void op_ext_94(CPU_t* cpu) {
	modregrm(cpu);
	writerm8(cpu, cpu->rm, (cpu->zf) ? 1 : 0);
}

// 95 SETNZ Jb
void op_ext_95(CPU_t* cpu) {
	modregrm(cpu);
	writerm8(cpu, cpu->rm, (!cpu->zf) ? 1 : 0);
}

// 96 SETBE Jb
void op_ext_96(CPU_t* cpu) {
	modregrm(cpu);
	writerm8(cpu, cpu->rm, (cpu->cf || cpu->zf) ? 1 : 0);
}

// 97 SETA Jb
void op_ext_97(CPU_t* cpu) {
	modregrm(cpu);
	writerm8(cpu, cpu->rm, (!cpu->cf && !cpu->zf) ? 1 : 0);
}

// 98 SETS Jb
void op_ext_98(CPU_t* cpu) {
	modregrm(cpu);
	writerm8(cpu, cpu->rm, (cpu->sf) ? 1 : 0);
}

// 99 SETNS Jb
void op_ext_99(CPU_t* cpu) {
	modregrm(cpu);
	writerm8(cpu, cpu->rm, (!cpu->sf) ? 1 : 0);
}

// 9A SETPE Jb
void op_ext_9A(CPU_t* cpu) {
	modregrm(cpu);
	writerm8(cpu, cpu->rm, (cpu->pf) ? 1 : 0);
}

// 9B SETPO Jb
void op_ext_9B(CPU_t* cpu) {
	modregrm(cpu);
	writerm8(cpu, cpu->rm, (!cpu->pf) ? 1 : 0);
}

// 9C SETL Jb
void op_ext_9C(CPU_t* cpu) {
	modregrm(cpu);
	writerm8(cpu, cpu->rm, (cpu->sf != cpu->of) ? 1 : 0);
}

// 9D SETGE Jb
void op_ext_9D(CPU_t* cpu) {
	modregrm(cpu);
	writerm8(cpu, cpu->rm, (cpu->sf == cpu->of) ? 1 : 0);
}

// 9E SETLE Jb
void op_ext_9E(CPU_t* cpu) {
	modregrm(cpu);
	writerm8(cpu, cpu->rm, ((cpu->sf != cpu->of) || cpu->zf) ? 1 : 0);
}

// 9F SETG Jb
void op_ext_9F(CPU_t* cpu) {
	modregrm(cpu);
	writerm8(cpu, cpu->rm, (!cpu->zf && (cpu->sf == cpu->of)) ? 1 : 0);
}

//PUSH FS
void op_ext_A0(CPU_t* cpu) {
	push(cpu, getsegreg(cpu, regfs));
}

//POP FS
void op_ext_A1(CPU_t* cpu) {
	putsegreg(cpu, regfs, pop(cpu));
}

//CPUID
void op_ext_A2(CPU_t* cpu) {
	switch (cpu->regs.longregs[regeax]) {
	case 0:
		cpu->regs.longregs[regeax] = 1;
		cpu->regs.longregs[regebx] = 'G' | ('e' << 8) | ('n' << 16) | ('u' << 24);
		cpu->regs.longregs[regedx] = 'i' | ('n' << 8) | ('e' << 16) | ('I' << 24);
		cpu->regs.longregs[regecx] = 'n' | ('t' << 8) | ('e' << 16) | ('l' << 24);
		break;
	case 1:
		cpu->regs.longregs[regeax] =
			/*(0 << 0)  // Stepping
			| (0 << 4)  // Model
			| (4 << 8); // Family (i486)*/
			(0x0C << 0) // Stepping
			| (0x02 << 4) // Model
			| (0x05 << 8); //Family (Pentium)
		cpu->regs.longregs[regebx] = cpu->regs.longregs[regecx] = 0;
		cpu->regs.longregs[regedx] =
			((cpu->have387 ? 1 : 0) << 0) | // FPU
			(1 << 4) | // TSC
			(1 << 8) | // CMPXCHG8B
			(1 << 15) | // CMOV
			(0 << 23) | // MMX
			(0 << 24) | // FXSR
			(0 << 25) | // SSE
			(0 << 26);  // SSE2
		break;
	default:
		cpu->regs.longregs[regeax] = cpu->regs.longregs[regebx] = cpu->regs.longregs[regecx] = cpu->regs.longregs[regedx] = 0;
	}
}

static int32_t cpu_floor_div_pow2_s32(int32_t value, uint32_t shift) {
	int64_t wide = value;
	int64_t divisor = 1LL << shift;

	if (wide >= 0) {
		return (int32_t)(wide / divisor);
	}
	return (int32_t)(-(((-wide) + divisor - 1) / divisor));
}

static uint32_t cpu_bitstring_ea_adjust(int32_t bit_offset, uint32_t operand_bits) {
	if (operand_bits == 32) {
		return (uint32_t)(cpu_floor_div_pow2_s32(bit_offset, 5) * 4);
	}
	return (uint32_t)(cpu_floor_div_pow2_s32(bit_offset, 4) * 2);
}

static uint32_t cpu_bit_access_read32(CPU_t* cpu, int32_t bit_offset, int adjust_memory) {
	if (cpu->mode == 3) {
		return readrm32(cpu, cpu->rm);
	}

	getea(cpu, cpu->rm);
	if (adjust_memory) {
		cpu->ea += cpu_bitstring_ea_adjust(bit_offset, 32);
	}
	return cpu_readl(cpu, cpu->ea);
}

static uint16_t cpu_bit_access_read16(CPU_t* cpu, int32_t bit_offset, int adjust_memory) {
	if (cpu->mode == 3) {
		return readrm16(cpu, cpu->rm);
	}

	getea(cpu, cpu->rm);
	if (adjust_memory) {
		cpu->ea += cpu_bitstring_ea_adjust(bit_offset, 16);
	}
	return cpu_readw(cpu, cpu->ea);
}

static void cpu_bit_access_write32(CPU_t* cpu, uint32_t value) {
	if (cpu->mode == 3) {
		writerm32(cpu, cpu->rm, value);
	}
	else {
		cpu_writel(cpu, cpu->ea, value);
	}
}

static void cpu_bit_access_write16(CPU_t* cpu, uint16_t value) {
	if (cpu->mode == 3) {
		writerm16(cpu, cpu->rm, value);
	}
	else {
		cpu_writew(cpu, cpu->ea, value);
	}
}

//BT reg
void op_ext_A3(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		int32_t bit_offset = (int32_t)getreg32(cpu, cpu->reg);
		uint32_t bit_index = ((uint32_t)bit_offset) & 31u;

		cpu->oper1_32 = cpu_bit_access_read32(cpu, bit_offset, cpu->mode != 3);
		cpu->cf = (cpu->oper1_32 >> bit_index) & 1;
	}
	else {
		int32_t bit_offset = (int32_t)(int16_t)getreg16(cpu, cpu->reg);
		uint32_t bit_index = ((uint32_t)bit_offset) & 15u;

		cpu->oper1 = cpu_bit_access_read16(cpu, bit_offset, cpu->mode != 3);
		cpu->cf = (cpu->oper1 >> bit_index) & 1;
	}
}

//SHLD r/m, r, imm8
//SHLD r/m, r, CL
void op_ext_A4_A5(CPU_t* cpu) {
	uint8_t count;
	modregrm(cpu);
	if (cpu->opcode == 0xA4) {
		count = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 1);
	}
	else {
		count = cpu->regs.byteregs[regcl];
	}

	if (cpu->isoper32) {
		uint32_t dest = readrm32(cpu, cpu->rm);
		uint32_t src = getreg32(cpu, cpu->reg);
		count &= 0x1F;

		if (count != 0) {
			uint64_t combined = ((uint64_t)dest << 32) | src;

			cpu->cf = (uint32_t)((combined >> (64 - count)) & 1u);
			cpu->res32 = (uint32_t)((combined << count) >> 32);
			writerm32(cpu, cpu->rm, cpu->res32);
			flag_szp32(cpu, cpu->res32);
			if (count == 1)
				cpu->of = (((cpu->res32 >> 31) & 1) ^ cpu->cf);
			else
				cpu->of = 0;
		}
	}
	else {
		uint16_t dest = readrm16(cpu, cpu->rm);
		uint16_t src = getreg16(cpu, cpu->reg);
		count &= 0x1F;

		if (count != 0) {
			uint32_t combined = ((uint32_t)dest << 16) | src;

			cpu->cf = (combined >> (32 - count)) & 1u;
			cpu->res16 = (uint16_t)((combined << count) >> 16);
			writerm16(cpu, cpu->rm, cpu->res16);
			flag_szp16(cpu, cpu->res16);
			if (count == 1)
				cpu->of = (((cpu->res16 >> 15) & 1) ^ cpu->cf);
			else
				cpu->of = 0;
		}
	}
}

//CMPXCHG
void op_ext_A6_B0(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = cpu->regs.byteregs[regal]; // readreg(cpu, cpu->reg);
	cpu->oper2b = readrm8(cpu, cpu->rm);
	op_sub8(cpu);
	if (cpu->zf) {
		writerm8(cpu, cpu->rm, getreg8(cpu, cpu->reg));
	}
	else {
		cpu->regs.byteregs[regal] = cpu->oper2b;
	}
}

//PUSH GS
void op_ext_A8(CPU_t* cpu) {
	push(cpu, getsegreg(cpu, reggs));
}

//POP GS
void op_ext_A9(CPU_t* cpu) {
	putsegreg(cpu, reggs, pop(cpu));
}

//RSM (?)
void op_ext_AA(CPU_t* cpu) {

}

//BTS rm, reg
void op_ext_AB(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		int32_t bit_offset = (int32_t)getreg32(cpu, cpu->reg);
		uint32_t bit_index = ((uint32_t)bit_offset) & 31u;
		uint32_t bit_mask = 1u << bit_index;

		cpu->oper1_32 = cpu_bit_access_read32(cpu, bit_offset, cpu->mode != 3);
		cpu_bit_access_write32(cpu, cpu->oper1_32 | bit_mask);
		cpu->cf = (cpu->oper1_32 >> bit_index) & 1;
	}
	else {
		int32_t bit_offset = (int32_t)(int16_t)getreg16(cpu, cpu->reg);
		uint32_t bit_index = ((uint32_t)bit_offset) & 15u;
		uint16_t bit_mask = (uint16_t)(1u << bit_index);

		cpu->oper1 = cpu_bit_access_read16(cpu, bit_offset, cpu->mode != 3);
		cpu_bit_access_write16(cpu, cpu->oper1 | bit_mask);
		cpu->cf = (cpu->oper1 >> bit_index) & 1;
	}
}

//SHRD
void op_ext_AC_AD(CPU_t* cpu) {
	uint32_t count, sign;
	uint64_t temp;
	modregrm(cpu);
	if (cpu->opcode == 0xAD) {
		count = cpu->regs.byteregs[regcl];
	}
	else {
		count = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 1);
	}
	count &= 0x1F;
	if (count != 0) {
		if (cpu->isoper32) {
			uint32_t dest, src;

			dest = readrm32(cpu, cpu->rm);
			src = getreg32(cpu, cpu->reg);
			sign = dest & 0x80000000;
			temp = (dest | ((uint64_t)src << 32)) >> count;
			temp &= 0xFFFFFFFF;
			cpu->cf = (uint32_t)((((uint64_t)dest | ((uint64_t)src << 32)) >> (count - 1)) & 1u);
			if (count == 1) cpu->of = ((temp & 0x80000000) != sign) ? 1 : 0;
			flag_szp32(cpu, (uint32_t)temp);
			writerm32(cpu, cpu->rm, (uint32_t)temp);
		}
		else {
			uint16_t dest, src;

			dest = readrm16(cpu, cpu->rm);
			src = getreg16(cpu, cpu->reg);
			sign = dest & 0x8000;
			temp = (dest | ((uint32_t)src << 16)) >> count;
			temp &= 0xFFFF;
			cpu->cf = (uint32_t)((((uint32_t)dest | ((uint32_t)src << 16)) >> (count - 1)) & 1u);
			if (count == 1) cpu->of = ((temp & 0x8000) != sign) ? 1 : 0;
			flag_szp16(cpu, (uint16_t)temp);
			writerm16(cpu, cpu->rm, (uint16_t)temp);
		}
	}
}

//IMUL - TODO: is this right?
void op_ext_AF(CPU_t* cpu) {
	modregrm(cpu);
	int32_t src, dst;
	int64_t result;
	src = cpu->isoper32 ? readrm32(cpu, cpu->rm) : (int32_t)(int16_t)readrm16(cpu, cpu->rm);
	dst = cpu->isoper32 ? getreg32(cpu, cpu->reg) : (int32_t)(int16_t)getreg16(cpu, cpu->reg);
	result = (int64_t)dst * (int64_t)src;
	if (cpu->isoper32) {
		putreg32(cpu, cpu->reg, (uint32_t)result);
		if ((result < (-2147483647LL - 1)) || (result > 2147483647LL))
			cpu->cf = cpu->of = 1;
		else
			cpu->cf = cpu->of = 0;
	}
	else {
		putreg16(cpu, cpu->reg, (uint16_t)result);
		if ((result < -32768) || (result > 32767))
			cpu->cf = cpu->of = 1;
		else
			cpu->cf = cpu->of = 0;
	}
}


void op_ext_B1(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = cpu->regs.longregs[regeax]; // readreg(cpu, cpu->reg);
		cpu->oper2_32 = readrm32(cpu, cpu->rm);
		op_sub32(cpu);
		if (cpu->zf) {
			writerm32(cpu, cpu->rm, getreg32(cpu, cpu->reg));
		}
		else {
			cpu->regs.longregs[regeax] = cpu->oper2_32;
		}
	}
	else {
		cpu->oper1 = cpu->regs.wordregs[regax]; // readreg(cpu, cpu->reg);
		cpu->oper2 = readrm16(cpu, cpu->rm);
		op_sub16(cpu);
		if (cpu->zf) {
			writerm16(cpu, cpu->rm, getreg16(cpu, cpu->reg));
		}
		else {
			cpu->regs.wordregs[regax] = cpu->oper2;
		}
	}
}

//BTR rm16/32, r16/32
void op_ext_B3(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		int32_t bit_offset = (int32_t)getreg32(cpu, cpu->reg);
		uint32_t bit_index = ((uint32_t)bit_offset) & 31u;
		uint32_t bit_mask = 1u << bit_index;

		cpu->oper1_32 = cpu_bit_access_read32(cpu, bit_offset, cpu->mode != 3);
		cpu_bit_access_write32(cpu, cpu->oper1_32 & ~bit_mask);
		cpu->cf = (cpu->oper1_32 >> bit_index) & 1;
	}
	else {
		int32_t bit_offset = (int32_t)(int16_t)getreg16(cpu, cpu->reg);
		uint32_t bit_index = ((uint32_t)bit_offset) & 15u;
		uint16_t bit_mask = (uint16_t)(1u << bit_index);

		cpu->oper1 = cpu_bit_access_read16(cpu, bit_offset, cpu->mode != 3);
		cpu_bit_access_write16(cpu, cpu->oper1 & ~bit_mask);
		cpu->cf = (cpu->oper1 >> bit_index) & 1;
	}
}

//LSS
//LFS
//LGS
void op_ext_B2_B4_B5(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->mode == 3) {
		cpu->ip = firstip;
		exception(cpu, 6, 0); //UD
		return;
	}
	getea(cpu, cpu->rm);
	if (cpu->isoper32) {
		putreg32(cpu, cpu->reg, cpu_readl(cpu, cpu->ea));
		switch (cpu->opcode) {
		case 0xB2:
			putsegreg(cpu, regss, cpu_readw(cpu, cpu->ea + 4));
			cpu_begin_interrupt_shadow(cpu);
			break;
		case 0xB4: putsegreg(cpu, regfs, cpu_readw(cpu, cpu->ea + 4)); break;
		case 0xB5: putsegreg(cpu, reggs, cpu_readw(cpu, cpu->ea + 4)); break;
		}
	}
	else {
		putreg16(cpu, cpu->reg, cpu_readw(cpu, cpu->ea));
		switch (cpu->opcode) {
		case 0xB2:
			putsegreg(cpu, regss, cpu_readw(cpu, cpu->ea + 2));
			cpu_begin_interrupt_shadow(cpu);
			break;
		case 0xB4: putsegreg(cpu, regfs, cpu_readw(cpu, cpu->ea + 2)); break;
		case 0xB5: putsegreg(cpu, reggs, cpu_readw(cpu, cpu->ea + 2)); break;
		}
	}
}

//MOVZX r32/16, rm8 TODO: is this right?
void op_ext_B6(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		putreg32(cpu, cpu->reg, (uint32_t)readrm8(cpu, cpu->rm));
	}
	else {
		putreg16(cpu, cpu->reg, (uint16_t)readrm8(cpu, cpu->rm));
	}
}

//MOVZX r32, rm16 TODO: is this right?
void op_ext_B7(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		putreg32(cpu, cpu->reg, (uint32_t)readrm16(cpu, cpu->rm));
	}
	else {
		putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
	}
}

//bit operations
void op_ext_BA(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper2_32 = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
		cpu->oper1_32 = cpu_bit_access_read32(cpu, (int32_t)cpu->oper2_32, 0);
	}
	else {
		cpu->oper2 = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
		cpu->oper1 = cpu_bit_access_read16(cpu, (int32_t)(uint16_t)cpu->oper2, 0);
	}
	StepIP(cpu, 1);
	switch (cpu->reg) {
	case 4: //BT rm16/32, imm8
		if (cpu->isoper32) {
			uint32_t bit_index = cpu->oper2_32 & 31u;

			cpu->cf = (cpu->oper1_32 >> bit_index) & 1;
		}
		else {
			uint32_t bit_index = cpu->oper2 & 15u;

			cpu->cf = (cpu->oper1 >> bit_index) & 1;
		}
		break;
	case 5: //BTS rm16/32, imm8
		if (cpu->isoper32) {
			uint32_t bit_index = cpu->oper2_32 & 31u;
			uint32_t bit_mask = 1u << bit_index;

			cpu->cf = (cpu->oper1_32 >> bit_index) & 1;
			cpu_bit_access_write32(cpu, cpu->oper1_32 | bit_mask);
		}
		else {
			uint32_t bit_index = cpu->oper2 & 15u;
			uint16_t bit_mask = (uint16_t)(1u << bit_index);

			cpu->cf = (cpu->oper1 >> bit_index) & 1;
			cpu_bit_access_write16(cpu, cpu->oper1 | bit_mask);
		}
		break;
	case 6: //BTR rm16/32, imm8
		if (cpu->isoper32) {
			uint32_t bit_index = cpu->oper2_32 & 31u;
			uint32_t bit_mask = 1u << bit_index;

			cpu->cf = (cpu->oper1_32 >> bit_index) & 1;
			cpu_bit_access_write32(cpu, cpu->oper1_32 & ~bit_mask);
		}
		else {
			uint32_t bit_index = cpu->oper2 & 15u;
			uint16_t bit_mask = (uint16_t)(1u << bit_index);

			cpu->cf = (cpu->oper1 >> bit_index) & 1;
			cpu_bit_access_write16(cpu, cpu->oper1 & ~bit_mask);
		}
		break;
	case 7: //BTC rm16/32, imm8
		if (cpu->isoper32) {
			uint32_t bit_index = cpu->oper2_32 & 31u;
			uint32_t bit_mask = 1u << bit_index;

			cpu->cf = (cpu->oper1_32 >> bit_index) & 1;
			cpu_bit_access_write32(cpu, cpu->oper1_32 ^ bit_mask);
		}
		else {
			uint32_t bit_index = cpu->oper2 & 15u;
			uint16_t bit_mask = (uint16_t)(1u << bit_index);

			cpu->cf = (cpu->oper1 >> bit_index) & 1;
			cpu_bit_access_write16(cpu, cpu->oper1 ^ bit_mask);
		}
		break;
	default:
		printf("0F BA reg=%u", cpu->reg);
		exit(0);
	}
}

//BTC reg
void op_ext_BB(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		int32_t bit_offset = (int32_t)getreg32(cpu, cpu->reg);
		uint32_t bit_index = ((uint32_t)bit_offset) & 31u;
		uint32_t bit_mask = 1u << bit_index;

		cpu->oper1_32 = cpu_bit_access_read32(cpu, bit_offset, cpu->mode != 3);
		cpu_bit_access_write32(cpu, cpu->oper1_32 ^ bit_mask);
		cpu->cf = (cpu->oper1_32 >> bit_index) & 1;
	}
	else {
		int32_t bit_offset = (int32_t)(int16_t)getreg16(cpu, cpu->reg);
		uint32_t bit_index = ((uint32_t)bit_offset) & 15u;
		uint16_t bit_mask = (uint16_t)(1u << bit_index);

		cpu->oper1 = cpu_bit_access_read16(cpu, bit_offset, cpu->mode != 3);
		cpu_bit_access_write16(cpu, cpu->oper1 ^ bit_mask);
		cpu->cf = (cpu->oper1 >> bit_index) & 1;
	}
}

//BSF
void op_ext_BC(CPU_t* cpu) {
	uint32_t src, i;
	modregrm(cpu);
	if (cpu->isoper32) {
		src = readrm32(cpu, cpu->rm);
		if (src == 0) {
			cpu->zf = 1;
			return;
		}
		cpu->zf = 0;
		for (i = 0; i < 32; i++) {
			if (src & (1u << i)) break;
		}
		putreg32(cpu, cpu->reg, i);
	}
	else {
		src = readrm16(cpu, cpu->rm);
		if (src == 0) {
			cpu->zf = 1;
			return;
		}
		cpu->zf = 0;
		for (i = 0; i < 16; i++) {
			if (src & (1u << i)) break;
		}
		putreg16(cpu, cpu->reg, (uint16_t)i);
	}
}

//BSR
void op_ext_BD(CPU_t* cpu) {
	uint32_t src;
	int i;
	modregrm(cpu);
	if (cpu->isoper32) {
		src = readrm32(cpu, cpu->rm);
		if (src == 0) {
			cpu->zf = 1;
			return;
		}
		cpu->zf = 0;
		for (i = 31; i >= 0; i--) {
			if (src & (1u << i)) break;
		}
		putreg32(cpu, cpu->reg, i);
	}
	else {
		src = readrm16(cpu, cpu->rm);
		if (src == 0) {
			cpu->zf = 1;
			return;
		}
		cpu->zf = 0;
		for (i = 15; i >= 0; i--) {
			if (src & (1u << i)) break;
		}
		putreg16(cpu, cpu->reg, (uint16_t)i);
	}
}

//MOVSX reg, rm8
void op_ext_BE(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = (int32_t)(int16_t)(int8_t)readrm8(cpu, cpu->rm);
		putreg32(cpu, cpu->reg, cpu->oper1_32);
	}
	else {
		cpu->oper1 = (int16_t)(int8_t)readrm8(cpu, cpu->rm);
		putreg16(cpu, cpu->reg, cpu->oper1);
	}
}

//MOVSX reg, rm16
void op_ext_BF(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = (int32_t)(int16_t)readrm16(cpu, cpu->rm);
		putreg32(cpu, cpu->reg, cpu->oper1_32);
	}
	else {
		putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
	}
}

//XADD r/m8, r8
void op_ext_C0(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = readrm8(cpu, cpu->rm);
	cpu->oper2b = getreg8(cpu, cpu->reg);
	op_add8(cpu);
	writerm8(cpu, cpu->rm, cpu->res8);
	putreg8(cpu, cpu->reg, cpu->oper1b);
}

//XADD r/m16/32, r16/32
void op_ext_C1(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = readrm32(cpu, cpu->rm);
		cpu->oper2_32 = getreg32(cpu, cpu->reg);
		op_add32(cpu);
		writerm32(cpu, cpu->rm, cpu->res32);
		putreg32(cpu, cpu->reg, cpu->oper1_32);
	}
	else {
		cpu->oper1 = readrm16(cpu, cpu->rm);
		cpu->oper2 = getreg16(cpu, cpu->reg);
		op_add16(cpu);
		writerm16(cpu, cpu->rm, cpu->res16);
		putreg16(cpu, cpu->reg, cpu->oper1);
	}
}

//0F C7
void op_ext_C7(CPU_t* cpu) {
	uint64_t val64, cmp64;

	modregrm(cpu);
	switch (cpu->reg) {
	case 1: //CMPXCHG8B
		if (cpu->mode == 3) {
			cpu->ip = firstip;
			exception(cpu, 6, 0); //UD
			return;
		}

		val64 = readrm64(cpu, cpu->rm);
		cmp64 = (((uint64_t)cpu->regs.longregs[regedx]) << 32) | cpu->regs.longregs[regeax];
		if (cmp64 == val64) {
			cpu->zf = 1;
			val64 = (((uint64_t)cpu->regs.longregs[regecx]) << 32) | cpu->regs.longregs[regebx];
			writerm64(cpu, cpu->rm, val64);
		}
		else {
			cpu->zf = 0;
			cpu->regs.longregs[regeax] = (uint32_t)val64;
			cpu->regs.longregs[regedx] = (uint32_t)(val64 >> 32);
		}
		break;
	default:
		op_ext_illegal(cpu);
	}
}

//BSWAP EAX
//BSWAP ECX
//BSWAP EDX
//BSWAP EBX
//BSWAP ESP
//BSWAP EBP
//BSWAP ESI
//BSWAP EDI
void op_ext_C8_C9_CA_CB_CC_CD_CE_CF(CPU_t* cpu) {
	uint8_t reg;
	uint32_t val;
	reg = cpu->opcode & 7;
	val = cpu->regs.longregs[reg];
	cpu->regs.longregs[reg] =
		((val & 0x000000FF) << 24) |
		((val & 0x0000FF00) << 8) |
		((val & 0x00FF0000) >> 8) |
		((val & 0xFF000000) >> 24);
}

void op_ext_illegal(CPU_t* cpu) {
	exception(cpu, 6, 0); //UD
	debug_log(DEBUG_DETAIL, "[CPU] Invalid opcode exception at %08X (op 0F %02X)\r\n", cpu->segcache[regcs] + firstip, cpu->opcode);
}

void (*opcode_ext_table[256])(CPU_t* cpu) = {
	op_ext_00, op_ext_01, op_ext_02, op_ext_03, op_ext_illegal, op_ext_illegal, op_ext_06, op_ext_illegal,
	op_ext_08_09, op_ext_08_09, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal,
	op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal,
	op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal,
	op_ext_20, op_ext_21, op_ext_22, op_ext_23, op_ext_24_26, op_ext_illegal, op_ext_24_26, op_ext_illegal,
	op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal,
	op_ext_30, op_ext_31, op_ext_32, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal,
	op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal,
	op_ext_40, op_ext_41, op_ext_42, op_ext_43, op_ext_44, op_ext_45, op_ext_46, op_ext_47,
	op_ext_48, op_ext_49, op_ext_4A, op_ext_4B, op_ext_4C, op_ext_4D, op_ext_4E, op_ext_4F,
	op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal,
	op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal,
	op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal,
	op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal,
	op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal,
	op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal,
	op_ext_80, op_ext_81, op_ext_82, op_ext_83, op_ext_84, op_ext_85, op_ext_86, op_ext_87,
	op_ext_88, op_ext_89, op_ext_8A, op_ext_8B, op_ext_8C, op_ext_8D, op_ext_8E, op_ext_8F,
	op_ext_90, op_ext_91, op_ext_92, op_ext_93, op_ext_94, op_ext_95, op_ext_96, op_ext_97,
	op_ext_98, op_ext_99, op_ext_9A, op_ext_9B, op_ext_9C, op_ext_9D, op_ext_9E, op_ext_9F,
	op_ext_A0, op_ext_A1, op_ext_A2, op_ext_A3, op_ext_A4_A5, op_ext_A4_A5, op_ext_A6_B0, op_ext_illegal,
	op_ext_A8, op_ext_A9, op_ext_AA, op_ext_AB, op_ext_AC_AD, op_ext_AC_AD, op_ext_illegal, op_ext_AF,
	op_ext_A6_B0, op_ext_B1, op_ext_B2_B4_B5, op_ext_B3, op_ext_B2_B4_B5, op_ext_B2_B4_B5, op_ext_B6, op_ext_B7,
	op_ext_illegal, op_ext_illegal, op_ext_BA, op_ext_BB, op_ext_BC, op_ext_BD, op_ext_BE, op_ext_BF,
	op_ext_C0, op_ext_C1, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_C7,
	op_ext_C8_C9_CA_CB_CC_CD_CE_CF, op_ext_C8_C9_CA_CB_CC_CD_CE_CF, op_ext_C8_C9_CA_CB_CC_CD_CE_CF, op_ext_C8_C9_CA_CB_CC_CD_CE_CF, op_ext_C8_C9_CA_CB_CC_CD_CE_CF, op_ext_C8_C9_CA_CB_CC_CD_CE_CF, op_ext_C8_C9_CA_CB_CC_CD_CE_CF, op_ext_C8_C9_CA_CB_CC_CD_CE_CF,
	op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal,
	op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal,
	op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal,
	op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal,
	op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal,
	op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal, op_ext_illegal,
};

static FUNC_FORCE_INLINE void cpu_extop(CPU_t* cpu) {
	cpu->opcode = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);

	(*opcode_ext_table[cpu->opcode])(cpu);
}

void fpu_exec(CPU_t* cpu);

/* 00 ADD Eb Gb */
void op_00(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = readrm8(cpu, cpu->rm);
	cpu->oper2b = getreg8(cpu, cpu->reg);
	op_add8(cpu);
	writerm8(cpu, cpu->rm, cpu->res8);
}

/* 01 ADD Ev Gv */
void op_01(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = readrm32(cpu, cpu->rm);
		cpu->oper2_32 = getreg32(cpu, cpu->reg);
		op_add32(cpu);
		writerm32(cpu, cpu->rm, cpu->res32);
	}
	else {
		cpu->oper1 = readrm16(cpu, cpu->rm);
		cpu->oper2 = getreg16(cpu, cpu->reg);
		op_add16(cpu);
		writerm16(cpu, cpu->rm, cpu->res16);
	}
}

/* 02 ADD Gb Eb */
void op_02(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = getreg8(cpu, cpu->reg);
	cpu->oper2b = readrm8(cpu, cpu->rm);
	op_add8(cpu);
	putreg8(cpu, cpu->reg, cpu->res8);
}

/* 03 ADD Gv Ev */
void op_03(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = getreg32(cpu, cpu->reg);
		cpu->oper2_32 = readrm32(cpu, cpu->rm);
		op_add32(cpu);
		putreg32(cpu, cpu->reg, cpu->res32);
	}
	else {
		cpu->oper1 = getreg16(cpu, cpu->reg);
		cpu->oper2 = readrm16(cpu, cpu->rm);
		op_add16(cpu);
		putreg16(cpu, cpu->reg, cpu->res16);
	}
}

/* 04 ADD cpu->regs.byteregs[regal] Ib */
void op_04(CPU_t* cpu) {
	cpu->oper1b = cpu->regs.byteregs[regal];
	cpu->oper2b = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
	op_add8(cpu);
	cpu->regs.byteregs[regal] = cpu->res8;
}

/* 05 ADD eAX Iv */
void op_05(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->oper1_32 = cpu->regs.longregs[regeax];
		cpu->oper2_32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
		op_add32(cpu);
		cpu->regs.longregs[regeax] = cpu->res32;
	}
	else {
		cpu->oper1 = cpu->regs.wordregs[regax];
		cpu->oper2 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
		op_add16(cpu);
		cpu->regs.wordregs[regax] = cpu->res16;
	}
}

/* 06 PUSH cpu->segregs[reges] */
void op_06(CPU_t* cpu) {
	push(cpu, cpu->segregs[reges]);
}

/* 07 POP cpu->segregs[reges] */
void op_07(CPU_t* cpu) {
	putsegreg(cpu, reges, pop(cpu));
}

/* 08 OR Eb Gb */
void op_08(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = readrm8(cpu, cpu->rm);
	cpu->oper2b = getreg8(cpu, cpu->reg);
	op_or8(cpu);
	writerm8(cpu, cpu->rm, cpu->res8);
}

/* 09 OR Ev Gv */
void op_09(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = readrm32(cpu, cpu->rm);
		cpu->oper2_32 = getreg32(cpu, cpu->reg);
		op_or32(cpu);
		writerm32(cpu, cpu->rm, cpu->res32);
	}
	else {
		cpu->oper1 = readrm16(cpu, cpu->rm);
		cpu->oper2 = getreg16(cpu, cpu->reg);
		op_or16(cpu);
		writerm16(cpu, cpu->rm, cpu->res16);
	}
}

/* 0A OR Gb Eb */
void op_0A(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = getreg8(cpu, cpu->reg);
	cpu->oper2b = readrm8(cpu, cpu->rm);
	op_or8(cpu);
	putreg8(cpu, cpu->reg, cpu->res8);
}

/* 0B OR Gv Ev */
void op_0B(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = getreg32(cpu, cpu->reg);
		cpu->oper2_32 = readrm32(cpu, cpu->rm);
		op_or32(cpu);
		putreg32(cpu, cpu->reg, cpu->res32);
	}
	else {
		cpu->oper1 = getreg16(cpu, cpu->reg);
		cpu->oper2 = readrm16(cpu, cpu->rm);
		op_or16(cpu);
		putreg16(cpu, cpu->reg, cpu->res16);
	}
}

/* 0C OR cpu->regs.byteregs[regal] Ib */
void op_0C(CPU_t* cpu) {
	cpu->oper1b = cpu->regs.byteregs[regal];
	cpu->oper2b = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
	op_or8(cpu);
	cpu->regs.byteregs[regal] = cpu->res8;
}

/* 0D OR eAX Iv */
void op_0D(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->oper1_32 = cpu->regs.longregs[regeax];
		cpu->oper2_32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
		op_or32(cpu);
		cpu->regs.longregs[regeax] = cpu->res32;
	}
	else {
		cpu->oper1 = cpu->regs.wordregs[regax];
		cpu->oper2 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
		op_or16(cpu);
		cpu->regs.wordregs[regax] = cpu->res16;
	}
}

/* 0E PUSH cpu->segregs[regcs] */
void op_0E(CPU_t* cpu) {
	push(cpu, cpu->segregs[regcs]);
}

/* 10 ADC Eb Gb */
void op_10(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = readrm8(cpu, cpu->rm);
	cpu->oper2b = getreg8(cpu, cpu->reg);
	op_adc8(cpu);
	writerm8(cpu, cpu->rm, cpu->res8);
}

/* 11 ADC Ev Gv */
void op_11(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = readrm32(cpu, cpu->rm);
		cpu->oper2_32 = getreg32(cpu, cpu->reg);
		op_adc32(cpu);
		writerm32(cpu, cpu->rm, cpu->res32);
	}
	else {
		cpu->oper1 = readrm16(cpu, cpu->rm);
		cpu->oper2 = getreg16(cpu, cpu->reg);
		op_adc16(cpu);
		writerm16(cpu, cpu->rm, cpu->res16);
	}
}

/* 12 ADC Gb Eb */
void op_12(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = getreg8(cpu, cpu->reg);
	cpu->oper2b = readrm8(cpu, cpu->rm);
	op_adc8(cpu);
	putreg8(cpu, cpu->reg, cpu->res8);
}

/* 13 ADC Gv Ev */
void op_13(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = getreg32(cpu, cpu->reg);
		cpu->oper2_32 = readrm32(cpu, cpu->rm);
		op_adc32(cpu);
		putreg32(cpu, cpu->reg, cpu->res32);
	}
	else {
		cpu->oper1 = getreg16(cpu, cpu->reg);
		cpu->oper2 = readrm16(cpu, cpu->rm);
		op_adc16(cpu);
		putreg16(cpu, cpu->reg, cpu->res16);
	}
}

/* 14 ADC cpu->regs.byteregs[regal] Ib */
void op_14(CPU_t* cpu) {
	cpu->oper1b = cpu->regs.byteregs[regal];
	cpu->oper2b = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
	op_adc8(cpu);
	cpu->regs.byteregs[regal] = cpu->res8;
}

/* 15 ADC eAX Iv */
void op_15(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->oper1_32 = cpu->regs.longregs[regeax];
		cpu->oper2_32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
		op_adc32(cpu);
		cpu->regs.longregs[regeax] = cpu->res32;
	}
	else {
		cpu->oper1 = cpu->regs.wordregs[regax];
		cpu->oper2 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
		op_adc16(cpu);
		cpu->regs.wordregs[regax] = cpu->res16;
	}
}

/* 16 PUSH cpu->segregs[regss] */
void op_16(CPU_t* cpu) {
	push(cpu, cpu->segregs[regss]);
}

/* 17 POP cpu->segregs[regss] */
void op_17(CPU_t* cpu) {
	putsegreg(cpu, regss, pop(cpu));
	cpu_begin_interrupt_shadow(cpu);
}

/* 18 SBB Eb Gb */
void op_18(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = readrm8(cpu, cpu->rm);
	cpu->oper2b = getreg8(cpu, cpu->reg);
	op_sbb8(cpu);
	writerm8(cpu, cpu->rm, cpu->res8);
}

/* 19 SBB Ev Gv */
void op_19(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = readrm32(cpu, cpu->rm);
		cpu->oper2_32 = getreg32(cpu, cpu->reg);
		op_sbb32(cpu);
		writerm32(cpu, cpu->rm, cpu->res32);
	}
	else {
		cpu->oper1 = readrm16(cpu, cpu->rm);
		cpu->oper2 = getreg16(cpu, cpu->reg);
		op_sbb16(cpu);
		writerm16(cpu, cpu->rm, cpu->res16);
	}
}

/* 1A SBB Gb Eb */
void op_1A(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = getreg8(cpu, cpu->reg);
	cpu->oper2b = readrm8(cpu, cpu->rm);
	op_sbb8(cpu);
	putreg8(cpu, cpu->reg, cpu->res8);
}

/* 1B SBB Gv Ev */
void op_1B(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = getreg32(cpu, cpu->reg);
		cpu->oper2_32 = readrm32(cpu, cpu->rm);
		op_sbb32(cpu);
		putreg32(cpu, cpu->reg, cpu->res32);
	}
	else {
		cpu->oper1 = getreg16(cpu, cpu->reg);
		cpu->oper2 = readrm16(cpu, cpu->rm);
		op_sbb16(cpu);
		putreg16(cpu, cpu->reg, cpu->res16);
	}
}

/* 1C SBB cpu->regs.byteregs[regal] Ib */
void op_1C(CPU_t* cpu) {
	cpu->oper1b = cpu->regs.byteregs[regal];
	cpu->oper2b = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
	op_sbb8(cpu);
	cpu->regs.byteregs[regal] = cpu->res8;
}

/* 1D SBB eAX Iv */
void op_1D(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->oper1_32 = cpu->regs.longregs[regeax];
		cpu->oper2_32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
		op_sbb32(cpu);
		cpu->regs.longregs[regeax] = cpu->res32;
	}
	else {
		cpu->oper1 = cpu->regs.wordregs[regax];
		cpu->oper2 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
		op_sbb16(cpu);
		cpu->regs.wordregs[regax] = cpu->res16;
	}
}

/* 1E PUSH cpu->segregs[regds] */
void op_1E(CPU_t* cpu) {
	push(cpu, cpu->segregs[regds]);
}

/* 1F POP cpu->segregs[regds] */
void op_1F(CPU_t* cpu) {
	putsegreg(cpu, regds, pop(cpu));
}

/* 20 AND Eb Gb */
void op_20(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = readrm8(cpu, cpu->rm);
	cpu->oper2b = getreg8(cpu, cpu->reg);
	op_and8(cpu);
	writerm8(cpu, cpu->rm, cpu->res8);
}

/* 21 AND Ev Gv */
void op_21(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = readrm32(cpu, cpu->rm);
		cpu->oper2_32 = getreg32(cpu, cpu->reg);
		op_and32(cpu);
		writerm32(cpu, cpu->rm, cpu->res32);
	}
	else {
		cpu->oper1 = readrm16(cpu, cpu->rm);
		cpu->oper2 = getreg16(cpu, cpu->reg);
		op_and16(cpu);
		writerm16(cpu, cpu->rm, cpu->res16);
	}
}

/* 22 AND Gb Eb */
void op_22(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = getreg8(cpu, cpu->reg);
	cpu->oper2b = readrm8(cpu, cpu->rm);
	op_and8(cpu);
	putreg8(cpu, cpu->reg, cpu->res8);
}

/* 23 AND Gv Ev */
void op_23(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = getreg32(cpu, cpu->reg);
		cpu->oper2_32 = readrm32(cpu, cpu->rm);
		op_and32(cpu);
		putreg32(cpu, cpu->reg, cpu->res32);
	}
	else {
		cpu->oper1 = getreg16(cpu, cpu->reg);
		cpu->oper2 = readrm16(cpu, cpu->rm);
		op_and16(cpu);
		putreg16(cpu, cpu->reg, cpu->res16);
	}
}

/* 24 AND cpu->regs.byteregs[regal] Ib */
void op_24(CPU_t* cpu) {
	cpu->oper1b = cpu->regs.byteregs[regal];
	cpu->oper2b = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
	op_and8(cpu);
	cpu->regs.byteregs[regal] = cpu->res8;
}

/* 25 AND eAX Iv */
void op_25(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->oper1_32 = cpu->regs.longregs[regeax];
		cpu->oper2_32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
		op_and32(cpu);
		cpu->regs.longregs[regeax] = cpu->res32;
	}
	else {
		cpu->oper1 = cpu->regs.wordregs[regax];
		cpu->oper2 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
		op_and16(cpu);
		cpu->regs.wordregs[regax] = cpu->res16;
	}
}

/* 27 DAA */
void op_27(CPU_t* cpu) {
	uint8_t old_al;
	uint8_t old_cf;
	uint16_t tmp;

	old_al = cpu->regs.byteregs[regal];
	old_cf = cpu->cf;
	cpu->cf = 0;
	if (((cpu->regs.byteregs[regal] & 0x0F) > 9) || cpu->af) {
		tmp = (uint16_t)cpu->regs.byteregs[regal] + 0x06;
		cpu->regs.byteregs[regal] = tmp & 0xFF;
		cpu->cf = old_cf || ((tmp & 0xFF00) != 0);
		cpu->af = 1;
	}
	else {
		cpu->af = 0;
	}

	if ((old_al > 0x99) || old_cf) {
		cpu->regs.byteregs[regal] += 0x60;
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0;
	}

	flag_szp8(cpu, cpu->regs.byteregs[regal]);
}

/* 28 SUB Eb Gb */
void op_28(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = readrm8(cpu, cpu->rm);
	cpu->oper2b = getreg8(cpu, cpu->reg);
	op_sub8(cpu);
	writerm8(cpu, cpu->rm, cpu->res8);
}

/* 29 SUB Ev Gv */
void op_29(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = readrm32(cpu, cpu->rm);
		cpu->oper2_32 = getreg32(cpu, cpu->reg);
		op_sub32(cpu);
		writerm32(cpu, cpu->rm, cpu->res32);
	}
	else {
		cpu->oper1 = readrm16(cpu, cpu->rm);
		cpu->oper2 = getreg16(cpu, cpu->reg);
		op_sub16(cpu);
		writerm16(cpu, cpu->rm, cpu->res16);
	}
}

/* 2A SUB Gb Eb */
void op_2A(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = getreg8(cpu, cpu->reg);
	cpu->oper2b = readrm8(cpu, cpu->rm);
	op_sub8(cpu);
	putreg8(cpu, cpu->reg, cpu->res8);
}

/* 2B SUB Gv Ev */
void op_2B(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = getreg32(cpu, cpu->reg);
		cpu->oper2_32 = readrm32(cpu, cpu->rm);
		op_sub32(cpu);
		putreg32(cpu, cpu->reg, cpu->res32);
	}
	else {
		cpu->oper1 = getreg16(cpu, cpu->reg);
		cpu->oper2 = readrm16(cpu, cpu->rm);
		op_sub16(cpu);
		putreg16(cpu, cpu->reg, cpu->res16);
	}
}

/* 2C SUB cpu->regs.byteregs[regal] Ib */
void op_2C(CPU_t* cpu) {
	cpu->oper1b = cpu->regs.byteregs[regal];
	cpu->oper2b = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
	op_sub8(cpu);
	cpu->regs.byteregs[regal] = cpu->res8;
}

/* 2D SUB eAX Iv */
void op_2D(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->oper1_32 = cpu->regs.longregs[regeax];
		cpu->oper2_32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
		op_sub32(cpu);
		cpu->regs.longregs[regeax] = cpu->res32;
	}
	else {
		cpu->oper1 = cpu->regs.wordregs[regax];
		cpu->oper2 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
		op_sub16(cpu);
		cpu->regs.wordregs[regax] = cpu->res16;
	}
}

/* 2F DAS */
void op_2F(CPU_t* cpu) {
	uint8_t old_al;
	uint8_t old_cf;
	uint16_t tmp;

	old_al = cpu->regs.byteregs[regal];
	old_cf = cpu->cf;
	cpu->cf = 0;
	if (((cpu->regs.byteregs[regal] & 0x0F) > 9) || cpu->af) {
		tmp = (uint16_t)cpu->regs.byteregs[regal] - 0x06;
		cpu->regs.byteregs[regal] = tmp & 0xFF;
		cpu->cf = old_cf || ((tmp & 0xFF00) != 0);
		cpu->af = 1;
	}
	else {
		cpu->af = 0;
	}

	if ((old_al > 0x99) || old_cf) {
		cpu->regs.byteregs[regal] -= 0x60;
		cpu->cf = 1;
	}

	flag_szp8(cpu, cpu->regs.byteregs[regal]);
}

/* 30 XOR Eb Gb */
void op_30(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = readrm8(cpu, cpu->rm);
	cpu->oper2b = getreg8(cpu, cpu->reg);
	op_xor8(cpu);
	writerm8(cpu, cpu->rm, cpu->res8);

}

/* 31 XOR Ev Gv */
void op_31(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = readrm32(cpu, cpu->rm);
		cpu->oper2_32 = getreg32(cpu, cpu->reg);
		op_xor32(cpu);
		writerm32(cpu, cpu->rm, cpu->res32);
	}
	else {
		cpu->oper1 = readrm16(cpu, cpu->rm);
		cpu->oper2 = getreg16(cpu, cpu->reg);
		op_xor16(cpu);
		writerm16(cpu, cpu->rm, cpu->res16);
	}
}

/* 32 XOR Gb Eb */
void op_32(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = getreg8(cpu, cpu->reg);
	cpu->oper2b = readrm8(cpu, cpu->rm);
	op_xor8(cpu);
	putreg8(cpu, cpu->reg, cpu->res8);
}

/* 33 XOR Gv Ev */
void op_33(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = getreg32(cpu, cpu->reg);
		cpu->oper2_32 = readrm32(cpu, cpu->rm);
		op_xor32(cpu);
		putreg32(cpu, cpu->reg, cpu->res32);
	}
	else {
		cpu->oper1 = getreg16(cpu, cpu->reg);
		cpu->oper2 = readrm16(cpu, cpu->rm);
		op_xor16(cpu);
		putreg16(cpu, cpu->reg, cpu->res16);
	}
}

/* 34 XOR cpu->regs.byteregs[regal] Ib */
void op_34(CPU_t* cpu) {
	cpu->oper1b = cpu->regs.byteregs[regal];
	cpu->oper2b = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
	op_xor8(cpu);
	cpu->regs.byteregs[regal] = cpu->res8;
}

/* 35 XOR eAX Iv */
void op_35(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->oper1_32 = cpu->regs.longregs[regeax];
		cpu->oper2_32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
		op_xor32(cpu);
		cpu->regs.longregs[regeax] = cpu->res32;
	}
	else {
		cpu->oper1 = cpu->regs.wordregs[regax];
		cpu->oper2 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
		op_xor16(cpu);
		cpu->regs.wordregs[regax] = cpu->res16;
	}
}

/* 37 AAA ASCII */
void op_37(CPU_t* cpu) {
	if (((cpu->regs.byteregs[regal] & 0xF) > 9) || (cpu->af == 1)) {
		cpu->regs.byteregs[regal] = (cpu->regs.byteregs[regal] + 0x06) & 0x0F;
		cpu->regs.byteregs[regah] = cpu->regs.byteregs[regah] + 1;
		cpu->af = 1;
		cpu->cf = 1;
	}
	else {
		cpu->af = 0;
		cpu->cf = 0;
		cpu->regs.byteregs[regal] = cpu->regs.byteregs[regal] & 0x0F;
	}
}

/* 38 CMP Eb Gb */
void op_38(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = readrm8(cpu, cpu->rm);
	cpu->oper2b = getreg8(cpu, cpu->reg);
	flag_sub8(cpu, cpu->oper1b, cpu->oper2b);
}

/* 39 CMP Ev Gv */
void op_39(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = readrm32(cpu, cpu->rm);
		cpu->oper2_32 = getreg32(cpu, cpu->reg);
		flag_sub32(cpu, cpu->oper1_32, cpu->oper2_32);
	}
	else {
		cpu->oper1 = readrm16(cpu, cpu->rm);
		cpu->oper2 = getreg16(cpu, cpu->reg);
		//printf("cmp %04X, %04X", cpu->oper1, cpu->oper2);
		flag_sub16(cpu, cpu->oper1, cpu->oper2);
	}
}

/* 3A CMP Gb Eb */
void op_3A(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = getreg8(cpu, cpu->reg);
	cpu->oper2b = readrm8(cpu, cpu->rm);
	flag_sub8(cpu, cpu->oper1b, cpu->oper2b);
}

/* 3B CMP Gv Ev */
void op_3B(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = getreg32(cpu, cpu->reg);
		cpu->oper2_32 = readrm32(cpu, cpu->rm);
		flag_sub32(cpu, cpu->oper1_32, cpu->oper2_32);
	}
	else {
		cpu->oper1 = getreg16(cpu, cpu->reg);
		cpu->oper2 = readrm16(cpu, cpu->rm);
		flag_sub16(cpu, cpu->oper1, cpu->oper2);
	}
}

/* 3C CMP cpu->regs.byteregs[regal] Ib */
void op_3C(CPU_t* cpu) {
	cpu->oper1b = cpu->regs.byteregs[regal];
	cpu->oper2b = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
	flag_sub8(cpu, cpu->oper1b, cpu->oper2b);
}

/* 3D CMP eAX Iv */
void op_3D(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->oper1_32 = cpu->regs.longregs[regeax];
		cpu->oper2_32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
		flag_sub32(cpu, cpu->oper1_32, cpu->oper2_32);
	}
	else {
		cpu->oper1 = cpu->regs.wordregs[regax];
		cpu->oper2 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
		flag_sub16(cpu, cpu->oper1, cpu->oper2);
	}
}

/* 3F AAS ASCII */
void op_3F(CPU_t* cpu) {
	if (((cpu->regs.byteregs[regal] & 0xF) > 9) || (cpu->af == 1)) {
		cpu->regs.byteregs[regal] = (cpu->regs.byteregs[regal] - 0x06) & 0x0F;
		cpu->regs.byteregs[regah] = cpu->regs.byteregs[regah] - 1;
		cpu->af = 1;
		cpu->cf = 1;
	}
	else {
		cpu->af = 0;
		cpu->cf = 0;
		cpu->regs.byteregs[regal] = cpu->regs.byteregs[regal] & 0x0F;
	}
}

/* INC AX */
/* INC CX */
/* INC DX */
/* INC BX */
/* INC SP */
/* INC BP */
/* INC SI */
/* INC DI */
void op_inc(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->oldcf = cpu->cf;
		cpu->oper1_32 = getreg32(cpu, cpu->opcode & 7);
		cpu->oper2_32 = 1;
		op_add32(cpu);
		cpu->cf = cpu->oldcf;
		putreg32(cpu, cpu->opcode & 7, cpu->res32);
	}
	else {
		cpu->oldcf = cpu->cf;
		cpu->oper1 = getreg16(cpu, cpu->opcode & 7);
		cpu->oper2 = 1;
		op_add16(cpu);
		cpu->cf = cpu->oldcf;
		putreg16(cpu, cpu->opcode & 7, cpu->res16);
	}
}

/* DEC AX */
/* DEC CX */
/* DEC DX */
/* DEC BX */
/* DEC SP */
/* DEC BP */
/* DEC SI */
/* DEC DI */
void op_dec(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->oldcf = cpu->cf;
		cpu->oper1_32 = getreg32(cpu, cpu->opcode & 7);
		cpu->oper2_32 = 1;
		op_sub32(cpu);
		cpu->cf = cpu->oldcf;
		putreg32(cpu, cpu->opcode & 7, cpu->res32);
	}
	else {
		cpu->oldcf = cpu->cf;
		cpu->oper1 = getreg16(cpu, cpu->opcode & 7);
		cpu->oper2 = 1;
		op_sub16(cpu);
		cpu->cf = cpu->oldcf;
		putreg16(cpu, cpu->opcode & 7, cpu->res16);
	}
}

/* 50 PUSH eAX */
/* 51 PUSH eCX */
/* 52 PUSH eDX */
/* 53 PUSH eBX */
/* 55 PUSH eBP */
/* 56 PUSH eSI */
/* 57 PUSH eDI */
void op_push(CPU_t* cpu) {
	push(cpu, cpu->isoper32 ? getreg32(cpu, cpu->opcode & 7) : getreg16(cpu, cpu->opcode & 7));
}

/* 54 PUSH eSP */
void op_54(CPU_t* cpu) {
	push(cpu, cpu->isoper32 ? getreg32(cpu, cpu->opcode & 7) : getreg16(cpu, cpu->opcode & 7));
}

/* 58 POP eAX */
/* 59 POP eCX */
/* 5A POP eDX */
/* 5B POP eBX */
/* 5C POP eSP */
/* 5D POP eBP */
/* 5E POP eSI */
/* 5F POP eDI */
void op_pop(CPU_t* cpu) {
	if (cpu->isoper32) {
		putreg32(cpu, cpu->opcode & 7, pop(cpu));
		/*if (cpu->opcode == 0x5C) { //TODO: is this stuff right?
			if (cpu->segis32[regss])
				cpu->regs.longregs[regesp] += 4;
			else
				cpu->regs.wordregs[regsp] += 4;
		}*/
	}
	else {
		putreg16(cpu, cpu->opcode & 7, pop(cpu));
		/*if (cpu->opcode == 0x5C) {
			if (cpu->segis32[regss])
				cpu->regs.longregs[regesp] += 2;
			else
				cpu->regs.wordregs[regsp] += 2;
		}*/
	}
}

/* 60 PUSHA (80186+) */
void op_60(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->oldsp = cpu->regs.longregs[regesp];
		push(cpu, cpu->regs.longregs[regeax]);
		push(cpu, cpu->regs.longregs[regecx]);
		push(cpu, cpu->regs.longregs[regedx]);
		push(cpu, cpu->regs.longregs[regebx]);
		push(cpu, cpu->oldsp);
		push(cpu, cpu->regs.longregs[regebp]);
		push(cpu, cpu->regs.longregs[regesi]);
		push(cpu, cpu->regs.longregs[regedi]);
	}
	else {
		cpu->oldsp = cpu->regs.wordregs[regsp];
		push(cpu, cpu->regs.wordregs[regax]);
		push(cpu, cpu->regs.wordregs[regcx]);
		push(cpu, cpu->regs.wordregs[regdx]);
		push(cpu, cpu->regs.wordregs[regbx]);
		push(cpu, cpu->oldsp);
		push(cpu, cpu->regs.wordregs[regbp]);
		push(cpu, cpu->regs.wordregs[regsi]);
		push(cpu, cpu->regs.wordregs[regdi]);
	}
}

/* 61 POPA (80186+) */
void op_61(CPU_t* cpu) {
	if (cpu->isoper32) {
		uint32_t dummy;
		cpu->regs.longregs[regedi] = pop(cpu);
		cpu->regs.longregs[regesi] = pop(cpu);
		cpu->regs.longregs[regebp] = pop(cpu);
		dummy = pop(cpu);
		cpu->regs.longregs[regebx] = pop(cpu);
		cpu->regs.longregs[regedx] = pop(cpu);
		cpu->regs.longregs[regecx] = pop(cpu);
		cpu->regs.longregs[regeax] = pop(cpu);
	}
	else {
		uint16_t dummy;
		cpu->regs.wordregs[regdi] = pop(cpu);
		cpu->regs.wordregs[regsi] = pop(cpu);
		cpu->regs.wordregs[regbp] = pop(cpu);
		dummy = pop(cpu);
		cpu->regs.wordregs[regbx] = pop(cpu);
		cpu->regs.wordregs[regdx] = pop(cpu);
		cpu->regs.wordregs[regcx] = pop(cpu);
		cpu->regs.wordregs[regax] = pop(cpu);
	}
}

/* 62 BOUND Gv, Ev (80186+) */
void op_62(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->mode == 3) {
		exception(cpu, 6, 0); //UD
		return;
	}
	getea(cpu, cpu->rm);
	if (cpu->isoper32) {
		int32_t index, lower_bound, upper_bound;
		index = (int32_t)getreg32(cpu, cpu->reg);
		lower_bound = (int32_t)cpu_readl(cpu, cpu->ea);
		upper_bound = (int32_t)cpu_readl(cpu, cpu->ea + 4);
		if ((index < lower_bound) || (index > upper_bound)) {
			//cpu_intcall(cpu, 5, INT_SOURCE_EXCEPTION, 0); //bounds check exception
			exception(cpu, 5, 0); //BR
		}
	}
	else {
		int16_t index, lower_bound, upper_bound;
		index = (int16_t)getreg16(cpu, cpu->reg);
		lower_bound = (int16_t)cpu_readw(cpu, cpu->ea);
		upper_bound = (int16_t)cpu_readw(cpu, cpu->ea + 2);
		if ((index < lower_bound) || (index > upper_bound)) {
			//cpu_intcall(cpu, 5, INT_SOURCE_EXCEPTION, 0); //bounds check exception
			exception(cpu, 5, 0); //BR
		}
	}
}


void op_63(CPU_t* cpu) {
	if (!cpu->protected_mode || cpu->v86f) {
		exception(cpu, 6, 0); //UD
		return;
	}
	modregrm(cpu);
	cpu->oper1 = readrm16(cpu, cpu->rm);
	if (cpu->doexception) {
		return;
	}
	cpu->oper2 = getreg16(cpu, cpu->reg) & 0x03;
	if ((cpu->oper1 & 3) < cpu->oper2) {
		cpu->res16 = (cpu->oper1 & ~0x03) | cpu->oper2;
		writerm16(cpu, cpu->rm, cpu->res16);
		cpu->zf = 1;
	}
	else {
		cpu->zf = 0;
	}
}

/* 68 PUSH Iv (80186+) */
void op_68(CPU_t* cpu) {
	if (cpu->isoper32) {
		push(cpu, getmem32(cpu, cpu->segcache[regcs], cpu->ip));
		StepIP(cpu, 4);
	}
	else {
		push(cpu, getmem16(cpu, cpu->segcache[regcs], cpu->ip));
		StepIP(cpu, 2);
	}
}

/* 69 IMUL Gv Ev Iv (80186+) */
void op_69(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		int32_t src1 = (int32_t)readrm32(cpu, cpu->rm);
		int32_t src2 = (int32_t)getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);

		int64_t result = (int64_t)src1 * (int64_t)src2;
		putreg32(cpu, cpu->reg, (uint32_t)(result & 0xFFFFFFFF));

		if ((result < (-2147483647LL - 1)) || (result > 2147483647LL)) {
			cpu->cf = 1;
			cpu->of = 1;
		}
		else {
			cpu->cf = 0;
			cpu->of = 0;
		}
		/*cpu->oper1_32 = readrm32(cpu, cpu->rm);
		cpu->oper2_32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
		if ((cpu->oper1_32 & 0x80000000) == 0x80000000) {
			cpu->temp64 = cpu->oper1_32 | 0xFFFFFFFF00000000UL;
		}

		if ((cpu->oper2_32 & 0x80000000) == 0x80000000) {
			cpu->temp64_2 = cpu->oper2_32 | 0xFFFFFFFF00000000UL;
		}

		cpu->temp64 = (uint64_t)cpu->oper1_32 * (uint64_t)cpu->oper2_32;
		putreg32(cpu, cpu->reg, cpu->temp64 & 0xFFFFFFFF);
		if (cpu->temp64 & 0xFFFFFFFF00000000UL) {
			cpu->cf = 1;
			cpu->of = 1;
		}
		else {
			cpu->cf = 0;
			cpu->of = 0;
		}*/
	}
	else {
		int32_t src1;
		int32_t src2;
		int32_t result;

		src1 = (int16_t)readrm16(cpu, cpu->rm);
		src2 = (int16_t)getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
		result = src1 * src2;
		putreg16(cpu, cpu->reg, result & 0xFFFFL);
		if ((result < -32768) || (result > 32767)) {
			cpu->cf = 1;
			cpu->of = 1;
		}
		else {
			cpu->cf = 0;
			cpu->of = 0;
		}
	}
}

/* 6A PUSH Ib (80186+) */
void op_6A(CPU_t* cpu) {
	push(cpu, signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip)));
	StepIP(cpu, 1);
}

/* 6B IMUL Gv Eb Ib (80186+) */
void op_6B(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		int64_t src1;
		int64_t src2;
		int64_t result;

		src1 = (int64_t)(int32_t)readrm32(cpu, cpu->rm);
		src2 = (int64_t)(int8_t)getmem8(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 1);

		result = src1 * src2;
		putreg32(cpu, cpu->reg, result & 0xFFFFFFFFL);
		if ((result < (-2147483647LL - 1)) || (result > 2147483647LL)) {
			cpu->cf = 1;
			cpu->of = 1;
		}
		else {
			cpu->cf = 0;
			cpu->of = 0;
		}
	}
	else {
		int32_t src1;
		int32_t src2;
		int32_t result;

		src1 = (int16_t)readrm16(cpu, cpu->rm);
		src2 = (int8_t)getmem8(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 1);
		result = src1 * src2;
		putreg16(cpu, cpu->reg, result & 0xFFFFL);
		if ((result < -32768) || (result > 32767)) {
			cpu->cf = 1;
			cpu->of = 1;
		}
		else {
			cpu->cf = 0;
			cpu->of = 0;
		}
	}
}

/* 6E INSB */
void op_6C(CPU_t* cpu) {
	if (!check_io_permission(cpu, cpu->regs.wordregs[regdx], 8)) 
		return;

	if (cpu->isaddr32) {
		if (cpu->reptype && (cpu->regs.longregs[regecx] == 0)) {
			return;
		}
	}
	else {
		if (cpu->reptype && (cpu->regs.wordregs[regcx] == 0)) {
			return;
		}
	}

	if (cpu->isaddr32) {
		putmem8(cpu, cpu->segcache[reges], cpu->regs.longregs[regedi], port_read(cpu, cpu->regs.wordregs[regdx]));
	}
	else {
		putmem8(cpu, cpu->segcache[reges], cpu->regs.wordregs[regdi], port_read(cpu, cpu->regs.wordregs[regdx]));
	}

	if (cpu->df) {
		if (cpu->isaddr32) {
			//cpu->regs.longregs[regesi]--;
			cpu->regs.longregs[regedi]--;
		}
		else {
			//cpu->regs.wordregs[regsi]--;
			cpu->regs.wordregs[regdi]--;
		}
	}
	else {
		if (cpu->isaddr32) {
			//cpu->regs.longregs[regesi]++;
			cpu->regs.longregs[regedi]++;
		}
		else {
			//cpu->regs.wordregs[regsi]++;
			cpu->regs.wordregs[regdi]++;
		}
	}

	if (cpu->reptype) {
		if (cpu->isaddr32) {
			cpu->regs.longregs[regecx]--;
		}
		else {
			cpu->regs.wordregs[regcx]--;
		}
	}

	if (!cpu->reptype) {
		return;
	}

	cpu->ip = firstip;
}

/* 6D INSW */
void op_6D(CPU_t* cpu) {
	if (!check_io_permission(cpu, cpu->regs.wordregs[regdx], cpu->isoper32 ? 32 : 16)) 
		return;

	if (cpu->reptype) {
		if (cpu->isaddr32) {
			if (cpu->regs.longregs[regecx] == 0) return;
		}
		else {
			if (cpu->regs.wordregs[regcx] == 0) return;
		}
	}

	if (cpu->isoper32) {
		if (cpu->isaddr32)
			putmem32(cpu, cpu->segcache[reges], cpu->regs.longregs[regedi], port_readl(cpu, cpu->regs.wordregs[regdx]));
		else
			putmem32(cpu, cpu->segcache[reges], cpu->regs.wordregs[regdi], port_readl(cpu, cpu->regs.wordregs[regdx]));
	}
	else {
		if (cpu->isaddr32)
			putmem16(cpu, cpu->segcache[reges], cpu->regs.longregs[regedi], port_readw(cpu, cpu->regs.wordregs[regdx]));
		else
			putmem16(cpu, cpu->segcache[reges], cpu->regs.wordregs[regdi], port_readw(cpu, cpu->regs.wordregs[regdx]));
	}
	if (cpu->df) {
		if (cpu->isaddr32) {
			cpu->regs.longregs[regedi] -= cpu->isoper32 ? 4 : 2;
			//cpu->regs.longregs[regesi] -= cpu->isoper32 ? 4 : 2;
		}
		else {
			cpu->regs.wordregs[regdi] -= cpu->isoper32 ? 4 : 2;
			//cpu->regs.wordregs[regsi] -= cpu->isoper32 ? 4 : 2;
		}
	}
	else {
		if (cpu->isaddr32) {
			cpu->regs.longregs[regedi] += cpu->isoper32 ? 4 : 2;
			//cpu->regs.longregs[regesi] += cpu->isoper32 ? 4 : 2;
		}
		else {
			cpu->regs.wordregs[regdi] += cpu->isoper32 ? 4 : 2;
			//cpu->regs.wordregs[regsi] += cpu->isoper32 ? 4 : 2;
		}
	}

	if (cpu->reptype) {
		if (cpu->isaddr32)
			cpu->regs.longregs[regecx]--;
		else
			cpu->regs.wordregs[regcx]--;
	}

	if (!cpu->reptype) {
		return;
	}

	cpu->ip = firstip;
}

/* 6E OUTSB */
void op_6E(CPU_t* cpu) {
	if (!check_io_permission(cpu, cpu->regs.wordregs[regdx], 8)) 
		return;

	if (cpu->reptype) {
		if (cpu->isaddr32) {
			if (cpu->regs.longregs[regecx] == 0) return;
		}
		else {
			if (cpu->regs.wordregs[regcx] == 0) return;
		}
	}

	if (cpu->isaddr32) {
		port_write(cpu, cpu->regs.wordregs[regdx], getmem8(cpu, cpu->useseg, cpu->regs.longregs[regesi]));
	}
	else {
		port_write(cpu, cpu->regs.wordregs[regdx], getmem8(cpu, cpu->useseg, cpu->regs.wordregs[regsi]));
	}

	if (cpu->df) {
		if (cpu->isaddr32) {
			cpu->regs.longregs[regesi]--;
		}
		else {
			cpu->regs.wordregs[regsi]--;
		}
	}
	else {
		if (cpu->isaddr32) {
			cpu->regs.longregs[regesi]++;
		}
		else {
			cpu->regs.wordregs[regsi]++;
		}
	}

	if (cpu->reptype) {
		if (cpu->isaddr32)
			cpu->regs.longregs[regecx]--;
		else
			cpu->regs.wordregs[regcx]--;
	}

	if (!cpu->reptype) {
		return;
	}

	cpu->ip = firstip;
}

/* 6F OUTSW */
void op_6F(CPU_t* cpu) {
	if (!check_io_permission(cpu, cpu->regs.wordregs[regdx], cpu->isoper32 ? 32 : 16)) 
		return;

	if (cpu->reptype) {
		if (cpu->isaddr32) {
			if (cpu->regs.longregs[regecx] == 0) return;
		}
		else {
			if (cpu->regs.wordregs[regcx] == 0) return;
		}
	}

	if (cpu->isoper32) {
		if (cpu->isaddr32)
			port_writel(cpu, cpu->regs.wordregs[regdx], getmem32(cpu, cpu->useseg, cpu->regs.longregs[regesi]));
		else
			port_writel(cpu, cpu->regs.wordregs[regdx], getmem32(cpu, cpu->useseg, cpu->regs.wordregs[regsi]));
	}
	else {
		if (cpu->isaddr32)
			port_writew(cpu, cpu->regs.wordregs[regdx], getmem16(cpu, cpu->useseg, cpu->regs.longregs[regesi]));
		else
			port_writew(cpu, cpu->regs.wordregs[regdx], getmem16(cpu, cpu->useseg, cpu->regs.wordregs[regsi]));
	}
	if (cpu->df) {
		if (cpu->isaddr32) {
			//cpu->regs.longregs[regedi] -= cpu->isoper32 ? 4 : 2;
			cpu->regs.longregs[regesi] -= cpu->isoper32 ? 4 : 2;
		}
		else {
			//cpu->regs.wordregs[regdi] -= cpu->isoper32 ? 4 : 2;
			cpu->regs.wordregs[regsi] -= cpu->isoper32 ? 4 : 2;
		}
	}
	else {
		if (cpu->isaddr32) {
			//cpu->regs.longregs[regedi] += cpu->isoper32 ? 4 : 2;
			cpu->regs.longregs[regesi] += cpu->isoper32 ? 4 : 2;
		}
		else {
			//cpu->regs.wordregs[regdi] += cpu->isoper32 ? 4 : 2;
			cpu->regs.wordregs[regsi] += cpu->isoper32 ? 4 : 2;
		}
	}

	if (cpu->reptype) {
		if (cpu->isaddr32)
			cpu->regs.longregs[regecx]--;
		else
			cpu->regs.wordregs[regcx]--;
	}

	if (!cpu->reptype) {
		return;
	}

	cpu->ip = firstip;
}

/* 70 JO Jb */
void op_70(CPU_t* cpu) {
	cpu->temp32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
	if (cpu->of) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

/* 71 JNO Jb */
void op_71(CPU_t* cpu) {
	cpu->temp32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
	if (!cpu->of) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

/* 72 JB Jb */
void op_72(CPU_t* cpu) {
	cpu->temp32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
	if (cpu->cf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

/* 73 JNB Jb */
void op_73(CPU_t* cpu) {
	cpu->temp32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
	if (!cpu->cf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

/* 74 JZ Jb */
void op_74(CPU_t* cpu) {
	cpu->temp32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
	if (cpu->zf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

/* 75 JNZ Jb */
void op_75(CPU_t* cpu) {
	cpu->temp32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
	if (!cpu->zf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

/* 76 JBE Jb */
void op_76(CPU_t* cpu) {
	cpu->temp32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
	if (cpu->cf || cpu->zf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

/* 77 JA Jb */
void op_77(CPU_t* cpu) {
	cpu->temp32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
	if (!cpu->cf && !cpu->zf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

/* 78 JS Jb */
void op_78(CPU_t* cpu) {
	cpu->temp32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
	if (cpu->sf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

/* 79 JNS Jb */
void op_79(CPU_t* cpu) {
	cpu->temp32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
	if (!cpu->sf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

/* 7A JPE Jb */
void op_7A(CPU_t* cpu) {
	cpu->temp32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
	if (cpu->pf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

/* 7B JPO Jb */
void op_7B(CPU_t* cpu) {
	cpu->temp32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
	if (!cpu->pf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

/* 7C JL Jb */
void op_7C(CPU_t* cpu) {
	cpu->temp32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
	if (cpu->sf != cpu->of) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

/* 7D JGE Jb */
void op_7D(CPU_t* cpu) {
	cpu->temp32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
	if (cpu->sf == cpu->of) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

/* 7E JLE Jb */
void op_7E(CPU_t* cpu) {
	cpu->temp32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
	if ((cpu->sf != cpu->of) || cpu->zf) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

/* 7F JG Jb */
void op_7F(CPU_t* cpu) {
	cpu->temp32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
	if (!cpu->zf && (cpu->sf == cpu->of)) {
		cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
	}
}

/* 80/82 GRP1 Eb Ib */
void op_80_82(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = readrm8(cpu, cpu->rm);
	cpu->oper2b = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
	switch (cpu->reg) {
	case 0:
		op_add8(cpu);
		break;
	case 1:
		op_or8(cpu);
		break;
	case 2:
		op_adc8(cpu);
		break;
	case 3:
		op_sbb8(cpu);
		break;
	case 4:
		op_and8(cpu);
		break;
	case 5:
		op_sub8(cpu);
		break;
	case 6:
		op_xor8(cpu);
		break;
	case 7:
		flag_sub8(cpu, cpu->oper1b, cpu->oper2b);
		break;
	default:
		break;	/* to avoid compiler warnings */
	}

	if (cpu->reg < 7) {
		writerm8(cpu, cpu->rm, cpu->res8);
	}
}

/* 81 GRP1 Ev Iv */
/* 83 GRP1 Ev Ib */
void op_81_83(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = readrm32(cpu, cpu->rm);
		if (cpu->opcode == 0x81) {
			cpu->oper2_32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
			StepIP(cpu, 4);
		}
		else {
			cpu->oper2_32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
			StepIP(cpu, 1);
		}
	}
	else {
		cpu->oper1 = readrm16(cpu, cpu->rm);
		if (cpu->opcode == 0x81) {
			cpu->oper2 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
			StepIP(cpu, 2);
		}
		else {
			cpu->oper2 = signext(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
			StepIP(cpu, 1);
		}
	}

	switch (cpu->reg) {
	case 0:
		if (cpu->isoper32) op_add32(cpu); else op_add16(cpu);
		break;
	case 1:
		if (cpu->isoper32) op_or32(cpu); else op_or16(cpu);
		break;
	case 2:
		if (cpu->isoper32) op_adc32(cpu); else op_adc16(cpu);
		break;
	case 3:
		if (cpu->isoper32) op_sbb32(cpu); else op_sbb16(cpu);
		break;
	case 4:
		if (cpu->isoper32) op_and32(cpu); else op_and16(cpu);
		break;
	case 5:
		if (cpu->isoper32) op_sub32(cpu); else op_sub16(cpu);
		break;
	case 6:
		if (cpu->isoper32) op_xor32(cpu); else op_xor16(cpu);
		break;
	case 7:
		if (cpu->isoper32) flag_sub32(cpu, cpu->oper1_32, cpu->oper2_32); else flag_sub16(cpu, cpu->oper1, cpu->oper2);
		break;
	default:
		break;	/* to avoid compiler warnings */
	}

	if (cpu->reg < 7) {
		if (cpu->isoper32) {
			writerm32(cpu, cpu->rm, cpu->res32);
		}
		else {
			writerm16(cpu, cpu->rm, cpu->res16);
		}
	}
}

/* 84 TEST Gb Eb */
void op_84(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = getreg8(cpu, cpu->reg);
	cpu->oper2b = readrm8(cpu, cpu->rm);
	flag_log8(cpu, cpu->oper1b & cpu->oper2b);
}

/* 85 TEST Gv Ev */
void op_85(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = getreg32(cpu, cpu->reg);
		cpu->oper2_32 = readrm32(cpu, cpu->rm);
		flag_log32(cpu, cpu->oper1_32 & cpu->oper2_32);
	}
	else {
		cpu->oper1 = getreg16(cpu, cpu->reg);
		cpu->oper2 = readrm16(cpu, cpu->rm);
		flag_log16(cpu, cpu->oper1 & cpu->oper2);
	}
}

/* 86 XCHG Gb Eb */
void op_86(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = getreg8(cpu, cpu->reg);
	putreg8(cpu, cpu->reg, readrm8(cpu, cpu->rm));
	writerm8(cpu, cpu->rm, cpu->oper1b);
}

/* 87 XCHG Gv Ev */
void op_87(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = getreg32(cpu, cpu->reg);
		putreg32(cpu, cpu->reg, readrm32(cpu, cpu->rm));
		writerm32(cpu, cpu->rm, cpu->oper1_32);
	}
	else {
		cpu->oper1 = getreg16(cpu, cpu->reg);
		putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
		writerm16(cpu, cpu->rm, cpu->oper1);
	}
}

/* 88 MOV Eb Gb */
void op_88(CPU_t* cpu) {
	modregrm(cpu);
	writerm8(cpu, cpu->rm, getreg8(cpu, cpu->reg));
}

/* 89 MOV Ev Gv */
void op_89(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		writerm32(cpu, cpu->rm, getreg32(cpu, cpu->reg));
	}
	else {
		writerm16(cpu, cpu->rm, getreg16(cpu, cpu->reg));
	}
}

/* 8A MOV Gb Eb */
void op_8A(CPU_t* cpu) {
	modregrm(cpu);
	putreg8(cpu, cpu->reg, readrm8(cpu, cpu->rm));
}

/* 8B MOV Gv Ev */
void op_8B(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		putreg32(cpu, cpu->reg, readrm32(cpu, cpu->rm));
	}
	else {
		putreg16(cpu, cpu->reg, readrm16(cpu, cpu->rm));
	}
}

/* 8C MOV Ew Sw */
void op_8C(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->reg > reggs) {
		cpu->ip = firstip;
		exception(cpu, 6, 0); //UD
		return;
	}
	writerm16(cpu, cpu->rm, getsegreg(cpu, cpu->reg));
}

/* 8D LEA Gv M */
void op_8D(CPU_t* cpu) {
	uint32_t offset;
	modregrm(cpu);
	if (cpu->mode == 3) {
		cpu->ip = firstip;
		exception(cpu, 6, 0); //UD
		return;
	}
	offset = cpu_effective_offset(cpu, cpu->rm);
	if (cpu->isoper32) {
		putreg32(cpu, cpu->reg, offset);
	}
	else {
		putreg16(cpu, cpu->reg, (uint16_t)offset);
	}
}

/* 8E MOV Sw Ew */
void op_8E(CPU_t* cpu) {
	uint16_t value;

	//if (cpu->isoper32 || cpu->isaddr32) { printf("32-bit op attempt on %02X", cpu->opcode); exit(0); }
	modregrm(cpu);
	if ((cpu->reg == regcs) || (cpu->reg > reggs)) {
		//cpu_intcall(cpu, 6, INT_SOURCE_EXCEPTION, 0); //UD
		cpu->ip = firstip;
		exception(cpu, 6, 0); //UD
		return;
	}
	value = readrm16(cpu, cpu->rm);
	if (cpu->doexception) {
		return;
	}
	putsegreg(cpu, cpu->reg, value);
	if (cpu->reg == regss) {
		cpu_begin_interrupt_shadow(cpu);
	}
}

/* 8F POP Ev */
void op_8F(CPU_t* cpu) {
	uint8_t modrm = getmem8(cpu, cpu->segcache[regcs], cpu->ip);

	if (((modrm >> 3) & 7) != 0) {
		cpu->ip = firstip;
		exception(cpu, 6, 0); //UD
		return;
	}

	if (cpu->isaddr32) {
		cpu->shadow_esp += cpu->isoper32 ? 4 : 2;
	}

	if (cpu->isoper32) {
		modregrm(cpu);
		writerm32(cpu, cpu->rm, pop(cpu));
		//debug_log(DEBUG_DETAIL, "EA was %08X", cpu->ea - cpu->useseg);
	}
	else {
		modregrm(cpu);
		writerm16(cpu, cpu->rm, pop(cpu));
	}
}

/* 90 NOP (technically XCHG eAX eAX) */
void op_90(CPU_t* cpu) {

}

/* 91 XCHG eCX eAX */
/* 92 XCHG eDX eAX */
/* 93 XCHG eBX eAX */
/* 94 XCHG eSP eAX */
/* 95 XCHG eBP eAX */
/* 96 XCHG eSI eAX */
/* 97 XCHG eDI eAX */
void op_xchg(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->oper1_32 = getreg32(cpu, cpu->opcode & 7);
		putreg32(cpu, cpu->opcode & 7, cpu->regs.longregs[regeax]);
		cpu->regs.longregs[regeax] = cpu->oper1_32;
	}
	else {
		cpu->oper1 = getreg16(cpu, cpu->opcode & 7);
		putreg16(cpu, cpu->opcode & 7, cpu->regs.wordregs[regax]);
		cpu->regs.wordregs[regax] = cpu->oper1;
	}
}

/* 98 CBW */
void op_98(CPU_t* cpu) {
	if (cpu->isoper32) {
		if ((cpu->regs.wordregs[regax] & 0x8000) == 0x8000) {
			cpu->regs.longregs[regeax] |= 0xFFFF0000;
		}
		else {
			cpu->regs.longregs[regeax] &= 0x0000FFFF;
		}
	}
	else {
		if ((cpu->regs.byteregs[regal] & 0x80) == 0x80) {
			cpu->regs.byteregs[regah] = 0xFF;
		}
		else {
			cpu->regs.byteregs[regah] = 0;
		}
	}
}

/* 99 CWD */
void op_99(CPU_t* cpu) {
	if (cpu->isoper32) {
		if (cpu->regs.longregs[regeax] & 0x80000000) {
			cpu->regs.longregs[regedx] = 0xFFFFFFFF;
		}
		else {
			cpu->regs.longregs[regedx] = 0;
		}
	}
	else {
		if ((cpu->regs.byteregs[regah] & 0x80) == 0x80) {
			cpu->regs.wordregs[regdx] = 0xFFFF;
		}
		else {
			cpu->regs.wordregs[regdx] = 0;
		}
	}
}

/* 9A CALL Ap */
void op_9A(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->oper1_32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
		cpu->oper2 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
		//pushl(cpu, cpu->segregs[regcs]);
		//pushl(cpu, cpu->ip);
	}
	else {
		cpu->oper1_32 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
		cpu->oper2 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
		//pushw(cpu, cpu->segregs[regcs]);
		//pushw(cpu, cpu->ip);
	}
	cpu_callf(cpu, cpu->oper2, cpu->oper1_32);
	//cpu->ip = cpu->oper1_32;
	//putsegreg(cpu, regcs, cpu->oper2);
}

/* 9B WAIT */
static int cpu_fpu_check_nm(CPU_t* cpu) {
	if (cpu->cr[0] & (CR0_EM | CR0_TS)) {
		cpu->ip = firstip;
		exception(cpu, 7, 0); //NM
		return 1;
	}
	return 0;
}

void op_9B(CPU_t* cpu) {
	if (cpu_fpu_check_nm(cpu)) {
		return;
	}
}

/* 9C PUSHF */
void op_9C(CPU_t* cpu) {
	uint32_t flags;

	if (cpu->v86f && (cpu->iopl < 3)) {
		exception(cpu, 13, 0); //GP(0)
		return;
	}

	flags = makeflagsword(cpu) & ~(EFLAGS_VM | EFLAGS_RF);
	push(cpu, flags);
}

/* 9D POPF */
void op_9D(CPU_t* cpu) {
	uint32_t old_flags;
	uint32_t raw_flags;
	uint32_t new_flags;
	uint32_t modifiable_mask;

	if (cpu->v86f && (cpu->iopl < 3)) {
		exception(cpu, 13, 0); //GP(0)
		return;
	}

	old_flags = makeflagsword(cpu);
	if (cpu->isoper32) {
		raw_flags = pop(cpu);
	}
	else {
		raw_flags = (old_flags & 0xFFFF0000u) | (pop(cpu) & 0xFFFFu);
	}

	modifiable_mask = EFLAGS_CF | EFLAGS_PF | EFLAGS_AF | EFLAGS_ZF | EFLAGS_SF | EFLAGS_TF | EFLAGS_DF | EFLAGS_OF | EFLAGS_NT;
	if (cpu->isoper32) {
		modifiable_mask |= (1u << 18) | (1u << 21);
	}

	new_flags = (old_flags & ~modifiable_mask) | (raw_flags & modifiable_mask);
	if (!cpu->protected_mode || (cpu->cpl <= cpu->iopl)) {
		new_flags = (new_flags & ~EFLAGS_IF) | (raw_flags & EFLAGS_IF);
	}
	else {
		new_flags = (new_flags & ~EFLAGS_IF) | (old_flags & EFLAGS_IF);
	}
	if (!cpu->protected_mode || (cpu->cpl == 0)) {
		new_flags = (new_flags & ~EFLAGS_IOPL) | (raw_flags & EFLAGS_IOPL);
	}
	else {
		new_flags = (new_flags & ~EFLAGS_IOPL) | (old_flags & EFLAGS_IOPL);
	}

	cpu->cf = (new_flags & EFLAGS_CF) ? 1 : 0;
	cpu->pf = (new_flags & EFLAGS_PF) ? 1 : 0;
	cpu->af = (new_flags & EFLAGS_AF) ? 1 : 0;
	cpu->zf = (new_flags & EFLAGS_ZF) ? 1 : 0;
	cpu->sf = (new_flags & EFLAGS_SF) ? 1 : 0;
	cpu->tf = (new_flags & EFLAGS_TF) ? 1 : 0;
	cpu->ifl = (new_flags & EFLAGS_IF) ? 1 : 0;
	cpu->df = (new_flags & EFLAGS_DF) ? 1 : 0;
	cpu->of = (new_flags & EFLAGS_OF) ? 1 : 0;
	cpu->iopl = (uint8_t)((new_flags & EFLAGS_IOPL) >> 12);
	cpu->nt = (new_flags & EFLAGS_NT) ? 1 : 0;
	cpu->rf = (old_flags & EFLAGS_RF) ? 1 : 0;
	cpu->v86f = (old_flags & EFLAGS_VM) ? 1 : 0;
	cpu->acf = (new_flags & (1u << 18)) ? 1 : 0;
	cpu->idf = (new_flags & (1u << 21)) ? 1 : 0;
}

/* 9E SAHF */
void op_9E(CPU_t* cpu) {
	decodeflagsword(cpu, (makeflagsword(cpu) & 0xFFFFFF00) | cpu->regs.byteregs[regah]);
}

/* 9F LAHF */
void op_9F(CPU_t* cpu) {
	cpu->regs.byteregs[regah] = makeflagsword(cpu) & 0xFF;
}

/* A0 MOV cpu->regs.byteregs[regal] Ob */
void op_A0(CPU_t* cpu) {
	if (cpu->isaddr32) {
		cpu->regs.byteregs[regal] = getmem8(cpu, cpu->useseg, getmem32(cpu, cpu->segcache[regcs], cpu->ip));
		StepIP(cpu, 4);
	}
	else {
		cpu->regs.byteregs[regal] = getmem8(cpu, cpu->useseg, getmem16(cpu, cpu->segcache[regcs], cpu->ip));
		StepIP(cpu, 2);
	}
}

/* A1 MOV eAX Ov */
void op_A1(CPU_t* cpu) {
	if (cpu->isaddr32) {
		cpu->tempaddr32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
	}
	else {
		cpu->tempaddr32 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
	}
	if (cpu->isoper32) {
		cpu->oper1_32 = getmem32(cpu, cpu->useseg, cpu->tempaddr32);
		cpu->regs.longregs[regeax] = cpu->oper1_32;
	}
	else {
		cpu->oper1 = getmem16(cpu, cpu->useseg, cpu->tempaddr32);
		cpu->regs.wordregs[regax] = cpu->oper1;
	}
}

/* A2 MOV Ob cpu->regs.byteregs[regal] */
void op_A2(CPU_t* cpu) {
	if (cpu->isaddr32) {
		putmem8(cpu, cpu->useseg, getmem32(cpu, cpu->segcache[regcs], cpu->ip), cpu->regs.byteregs[regal]);
		StepIP(cpu, 4);
	}
	else {
		putmem8(cpu, cpu->useseg, getmem16(cpu, cpu->segcache[regcs], cpu->ip), cpu->regs.byteregs[regal]);
		StepIP(cpu, 2);
	}
}

/* A3 MOV Ov eAX */
void op_A3(CPU_t* cpu) {
	if (cpu->isaddr32) {
		cpu->tempaddr32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
	}
	else {
		cpu->tempaddr32 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
	}
	if (cpu->isoper32) {
		putmem32(cpu, cpu->useseg, cpu->tempaddr32, cpu->regs.longregs[regeax]);
	}
	else {
		putmem16(cpu, cpu->useseg, cpu->tempaddr32, cpu->regs.wordregs[regax]);
	}
}

/* A4 MOVSB */
void op_A4(CPU_t* cpu) {
	if (cpu->isaddr32) {
		if (cpu->reptype && (cpu->regs.longregs[regecx] == 0)) {
			return;
		}
	}
	else {
		if (cpu->reptype && (cpu->regs.wordregs[regcx] == 0)) {
			return;
		}
	}

	if (cpu->isaddr32)
		putmem8(cpu, cpu->segcache[reges], cpu->regs.longregs[regedi], getmem8(cpu, cpu->useseg, cpu->regs.longregs[regesi]));
	else
		putmem8(cpu, cpu->segcache[reges], cpu->regs.wordregs[regdi], getmem8(cpu, cpu->useseg, cpu->regs.wordregs[regsi]));

	if (cpu->df) {
		if (cpu->isaddr32) {
			cpu->regs.longregs[regesi]--;
			cpu->regs.longregs[regedi]--;
		}
		else {
			cpu->regs.wordregs[regsi]--;
			cpu->regs.wordregs[regdi]--;
		}
	}
	else {
		if (cpu->isaddr32) {
			cpu->regs.longregs[regesi]++;
			cpu->regs.longregs[regedi]++;
		}
		else {
			cpu->regs.wordregs[regsi]++;
			cpu->regs.wordregs[regdi]++;
		}
	}

	if (cpu->reptype) {
		if (cpu->isaddr32)
			cpu->regs.longregs[regecx]--;
		else
			cpu->regs.wordregs[regcx]--;
	}

	if (!cpu->reptype) {
		return;
	}

	cpu->ip = firstip;
}

/* A5 MOVSW */
void op_A5(CPU_t* cpu) {
	if (cpu->reptype) {
		if (cpu->isaddr32) {
			if (cpu->regs.longregs[regecx] == 0) return;
		}
		else {
			if (cpu->regs.wordregs[regcx] == 0) return;
		}
	}

	if (cpu->isoper32) {
		if (cpu->isaddr32)
			putmem32(cpu, cpu->segcache[reges], cpu->regs.longregs[regedi], getmem32(cpu, cpu->useseg, cpu->regs.longregs[regesi]));
		else
			putmem32(cpu, cpu->segcache[reges], cpu->regs.wordregs[regdi], getmem32(cpu, cpu->useseg, cpu->regs.wordregs[regsi]));
	}
	else {
		if (cpu->isaddr32)
			putmem16(cpu, cpu->segcache[reges], cpu->regs.longregs[regedi], getmem16(cpu, cpu->useseg, cpu->regs.longregs[regesi]));
		else
			putmem16(cpu, cpu->segcache[reges], cpu->regs.wordregs[regdi], getmem16(cpu, cpu->useseg, cpu->regs.wordregs[regsi]));
	}

	if (cpu->df) {
		if (cpu->isaddr32) {
			cpu->regs.longregs[regedi] -= cpu->isoper32 ? 4 : 2;
			cpu->regs.longregs[regesi] -= cpu->isoper32 ? 4 : 2;
		}
		else {
			cpu->regs.wordregs[regdi] -= cpu->isoper32 ? 4 : 2;
			cpu->regs.wordregs[regsi] -= cpu->isoper32 ? 4 : 2;
		}
	}
	else {
		if (cpu->isaddr32) {
			cpu->regs.longregs[regedi] += cpu->isoper32 ? 4 : 2;
			cpu->regs.longregs[regesi] += cpu->isoper32 ? 4 : 2;
		}
		else {
			cpu->regs.wordregs[regdi] += cpu->isoper32 ? 4 : 2;
			cpu->regs.wordregs[regsi] += cpu->isoper32 ? 4 : 2;
		}
	}

	if (cpu->reptype) {
		if (cpu->isaddr32)
			cpu->regs.longregs[regecx]--;
		else
			cpu->regs.wordregs[regcx]--;
	}

	if (!cpu->reptype) {
		return;
	}

	cpu->ip = firstip;
}

/* A6 CMPSB */
void op_A6(CPU_t* cpu) {
	if (cpu->isaddr32) {
		if (cpu->reptype && (cpu->regs.longregs[regecx] == 0)) {
			return;
		}
	}
	else {
		if (cpu->reptype && (cpu->regs.wordregs[regcx] == 0)) {
			return;
		}
	}

	if (cpu->isaddr32) {
		cpu->oper1b = getmem8(cpu, cpu->useseg, cpu->regs.longregs[regesi]);
		cpu->oper2b = getmem8(cpu, cpu->segcache[reges], cpu->regs.longregs[regedi]);
	}
	else {
		cpu->oper1b = getmem8(cpu, cpu->useseg, cpu->regs.wordregs[regsi]);
		cpu->oper2b = getmem8(cpu, cpu->segcache[reges], cpu->regs.wordregs[regdi]);
	}
	flag_sub8(cpu, cpu->oper1b, cpu->oper2b);

	if (cpu->df) {
		if (cpu->isaddr32) {
			cpu->regs.longregs[regesi]--;
			cpu->regs.longregs[regedi]--;
		}
		else {
			cpu->regs.wordregs[regsi]--;
			cpu->regs.wordregs[regdi]--;
		}
	}
	else {
		if (cpu->isaddr32) {
			cpu->regs.longregs[regesi]++;
			cpu->regs.longregs[regedi]++;
		}
		else {
			cpu->regs.wordregs[regsi]++;
			cpu->regs.wordregs[regdi]++;
		}
	}

	if (cpu->reptype) {
		if (cpu->isaddr32)
			cpu->regs.longregs[regecx]--;
		else
			cpu->regs.wordregs[regcx]--;
	}

	if ((cpu->reptype == 1) && !cpu->zf) {
		return;
	}
	else if ((cpu->reptype == 2) && cpu->zf) {
		return;
	}

	if (!cpu->reptype) {
		return;
	}

	cpu->ip = firstip;
}

/* A7 CMPSW */
void op_A7(CPU_t* cpu) {
	if (cpu->reptype) {
		if (cpu->isaddr32) {
			if (cpu->regs.longregs[regecx] == 0) return;
		}
		else {
			if (cpu->regs.wordregs[regcx] == 0) return;
		}
	}

	if (cpu->isoper32) {
		if (cpu->isaddr32) {
			cpu->oper1_32 = getmem32(cpu, cpu->useseg, cpu->regs.longregs[regesi]);
			cpu->oper2_32 = getmem32(cpu, cpu->segcache[reges], cpu->regs.longregs[regedi]);
		}
		else {
			cpu->oper1_32 = getmem32(cpu, cpu->useseg, cpu->regs.wordregs[regsi]);
			cpu->oper2_32 = getmem32(cpu, cpu->segcache[reges], cpu->regs.wordregs[regdi]);
		}
		flag_sub32(cpu, cpu->oper1_32, cpu->oper2_32);
	}
	else {
		if (cpu->isaddr32) {
			cpu->oper1 = getmem16(cpu, cpu->useseg, cpu->regs.longregs[regesi]);
			cpu->oper2 = getmem16(cpu, cpu->segcache[reges], cpu->regs.longregs[regedi]);
		}
		else {
			cpu->oper1 = getmem16(cpu, cpu->useseg, cpu->regs.wordregs[regsi]);
			cpu->oper2 = getmem16(cpu, cpu->segcache[reges], cpu->regs.wordregs[regdi]);
		}
		flag_sub16(cpu, cpu->oper1, cpu->oper2);
	}

	if (cpu->df) {
		if (cpu->isaddr32) {
			cpu->regs.longregs[regedi] -= cpu->isoper32 ? 4 : 2;
			cpu->regs.longregs[regesi] -= cpu->isoper32 ? 4 : 2;
		}
		else {
			cpu->regs.wordregs[regdi] -= cpu->isoper32 ? 4 : 2;
			cpu->regs.wordregs[regsi] -= cpu->isoper32 ? 4 : 2;
		}
	}
	else {
		if (cpu->isaddr32) {
			cpu->regs.longregs[regedi] += cpu->isoper32 ? 4 : 2;
			cpu->regs.longregs[regesi] += cpu->isoper32 ? 4 : 2;
		}
		else {
			cpu->regs.wordregs[regdi] += cpu->isoper32 ? 4 : 2;
			cpu->regs.wordregs[regsi] += cpu->isoper32 ? 4 : 2;
		}
	}

	if (cpu->reptype) {
		if (cpu->isaddr32)
			cpu->regs.longregs[regecx]--;
		else
			cpu->regs.wordregs[regcx]--;
	}

	if ((cpu->reptype == 1) && !cpu->zf) {
		return;
	}

	if ((cpu->reptype == 2) && cpu->zf) {
		return;
	}

	if (!cpu->reptype) {
		return;
	}

	cpu->ip = firstip;
}

/* A8 TEST cpu->regs.byteregs[regal] Ib */
void op_A8(CPU_t* cpu) {
	cpu->oper1b = cpu->regs.byteregs[regal];
	cpu->oper2b = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
	flag_log8(cpu, cpu->oper1b & cpu->oper2b);
}

/* A9 TEST eAX Iv */
void op_A9(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->oper1_32 = cpu->regs.longregs[regeax];
		cpu->oper2_32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
		flag_log32(cpu, cpu->oper1_32 & cpu->oper2_32);
	}
	else {
		cpu->oper1 = cpu->regs.wordregs[regax];
		cpu->oper2 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
		flag_log16(cpu, cpu->oper1 & cpu->oper2);
	}
}

/* AA STOSB */
void op_AA(CPU_t* cpu) {
	if (cpu->isaddr32) {
		if (cpu->reptype && (cpu->regs.longregs[regecx] == 0)) {
			return;
		}
	}
	else {
		if (cpu->reptype && (cpu->regs.wordregs[regcx] == 0)) {
			return;
		}
	}

	if (cpu->isaddr32)
		putmem8(cpu, cpu->segcache[reges], cpu->regs.longregs[regedi], cpu->regs.byteregs[regal]);
	else
		putmem8(cpu, cpu->segcache[reges], cpu->regs.wordregs[regdi], cpu->regs.byteregs[regal]);

	if (cpu->df) {
		if (cpu->isaddr32)
			cpu->regs.longregs[regedi]--;
		else
			cpu->regs.wordregs[regdi]--;
	}
	else {
		if (cpu->isaddr32)
			cpu->regs.longregs[regedi]++;
		else
			cpu->regs.wordregs[regdi]++;
	}

	if (cpu->reptype) {
		if (cpu->isaddr32)
			cpu->regs.longregs[regecx]--;
		else
			cpu->regs.wordregs[regcx]--;
	}

	if (!cpu->reptype) {
		return;
	}

	cpu->ip = firstip;
}

/* AB STOSW */
void op_AB(CPU_t* cpu) {
	//if (cpu->isoper32 || cpu->isaddr32) { printf("32-bit op attempt on %02X", cpu->opcode); exit(0); }
	if (cpu->reptype) {
		if (cpu->isaddr32) {
			if (cpu->regs.longregs[regecx] == 0) return;
		}
		else {
			if (cpu->regs.wordregs[regcx] == 0) return;
		}
	}

	if (cpu->isoper32) {
		if (cpu->isaddr32)
			putmem32(cpu, cpu->segcache[reges], cpu->regs.longregs[regedi], cpu->regs.longregs[regeax]);
		else
			putmem32(cpu, cpu->segcache[reges], cpu->regs.wordregs[regdi], cpu->regs.longregs[regeax]);
	}
	else {
		if (cpu->isaddr32)
			putmem16(cpu, cpu->segcache[reges], cpu->regs.longregs[regedi], cpu->regs.wordregs[regax]);
		else
			putmem16(cpu, cpu->segcache[reges], cpu->regs.wordregs[regdi], cpu->regs.wordregs[regax]);
	}
	if (cpu->df) {
		if (cpu->isaddr32)
			cpu->regs.longregs[regedi] -= cpu->isoper32 ? 4 : 2;
		else
			cpu->regs.wordregs[regdi] -= cpu->isoper32 ? 4 : 2;
	}
	else {
		if (cpu->isaddr32)
			cpu->regs.longregs[regedi] += cpu->isoper32 ? 4 : 2;
		else
			cpu->regs.wordregs[regdi] += cpu->isoper32 ? 4 : 2;
	}

	if (cpu->reptype) {
		if (cpu->isaddr32)
			cpu->regs.longregs[regecx]--;
		else
			cpu->regs.wordregs[regcx]--;
	}

	if (!cpu->reptype) {
		return;
	}

	cpu->ip = firstip;
}

/* AC LODSB */
void op_AC(CPU_t* cpu) {
	if (cpu->isaddr32) {
		if (cpu->reptype && (cpu->regs.longregs[regecx] == 0)) {
			return;
		}
	}
	else {
		if (cpu->reptype && (cpu->regs.wordregs[regcx] == 0)) {
			return;
		}
	}

	if (cpu->isaddr32)
		cpu->regs.byteregs[regal] = getmem8(cpu, cpu->useseg, cpu->regs.longregs[regesi]);
	else
		cpu->regs.byteregs[regal] = getmem8(cpu, cpu->useseg, cpu->regs.wordregs[regsi]);

	if (cpu->df) {
		if (cpu->isaddr32)
			cpu->regs.longregs[regesi]--;
		else
			cpu->regs.wordregs[regsi]--;
	}
	else {
		if (cpu->isaddr32)
			cpu->regs.longregs[regesi]++;
		else
			cpu->regs.wordregs[regsi]++;
	}

	if (cpu->reptype) {
		if (cpu->isaddr32)
			cpu->regs.longregs[regecx]--;
		else
			cpu->regs.wordregs[regcx]--;
	}

	if (!cpu->reptype) {
		return;
	}

	cpu->ip = firstip;
}

/* AD LODSW */
void op_AD(CPU_t* cpu) {
	if (cpu->reptype) {
		if (cpu->isaddr32) {
			if (cpu->regs.longregs[regecx] == 0) return;
		}
		else {
			if (cpu->regs.wordregs[regcx] == 0) return;
		}
	}

	if (cpu->isoper32) {
		if (cpu->isaddr32)
			cpu->regs.longregs[regeax] = getmem32(cpu, cpu->useseg, cpu->regs.longregs[regesi]);
		else
			cpu->regs.longregs[regeax] = getmem32(cpu, cpu->useseg, cpu->regs.wordregs[regsi]);
	}
	else {
		if (cpu->isaddr32)
			cpu->regs.wordregs[regax] = getmem16(cpu, cpu->useseg, cpu->regs.longregs[regesi]);
		else
			cpu->regs.wordregs[regax] = getmem16(cpu, cpu->useseg, cpu->regs.wordregs[regsi]);
	}
	if (cpu->df) {
		if (cpu->isaddr32)
			cpu->regs.longregs[regesi] -= cpu->isoper32 ? 4 : 2;
		else
			cpu->regs.wordregs[regsi] -= cpu->isoper32 ? 4 : 2;
	}
	else {
		if (cpu->isaddr32)
			cpu->regs.longregs[regesi] += cpu->isoper32 ? 4 : 2;
		else
			cpu->regs.wordregs[regsi] += cpu->isoper32 ? 4 : 2;
	}

	if (cpu->reptype) {
		if (cpu->isaddr32)
			cpu->regs.longregs[regecx]--;
		else
			cpu->regs.wordregs[regcx]--;
	}

	if (!cpu->reptype) {
		return;
	}

	cpu->ip = firstip;
}

/* AE SCASB */
void op_AE(CPU_t* cpu) {
	if (cpu->isaddr32) {
		if (cpu->reptype && (cpu->regs.longregs[regecx] == 0)) {
			return;
		}
	}
	else {
		if (cpu->reptype && (cpu->regs.wordregs[regcx] == 0)) {
			return;
		}
	}

	if (cpu->isaddr32) {
		cpu->oper1b = cpu->regs.byteregs[regal];
		cpu->oper2b = getmem8(cpu, cpu->segcache[reges], cpu->regs.longregs[regedi]);
	}
	else {
		cpu->oper1b = cpu->regs.byteregs[regal];
		cpu->oper2b = getmem8(cpu, cpu->segcache[reges], cpu->regs.wordregs[regdi]);
	}
	flag_sub8(cpu, cpu->oper1b, cpu->oper2b);

	if (cpu->df) {
		if (cpu->isaddr32)
			cpu->regs.longregs[regedi]--;
		else
			cpu->regs.wordregs[regdi]--;
	}
	else {
		if (cpu->isaddr32)
			cpu->regs.longregs[regedi]++;
		else
			cpu->regs.wordregs[regdi]++;
	}

	if (cpu->reptype) {
		if (cpu->isaddr32)
			cpu->regs.longregs[regecx]--;
		else
			cpu->regs.wordregs[regcx]--;
	}

	if ((cpu->reptype == 1) && !cpu->zf) {
		return;
	}
	else if ((cpu->reptype == 2) && cpu->zf) {
		return;
	}

	if (!cpu->reptype) {
		return;
	}

	cpu->ip = firstip;
}

/* AF SCASW */
void op_AF(CPU_t* cpu) {
	if (cpu->isaddr32) {
		if (cpu->reptype && (cpu->regs.longregs[regecx] == 0)) {
			return;
		}
	}
	else {
		if (cpu->reptype && (cpu->regs.wordregs[regcx] == 0)) {
			return;
		}
	}

	if (cpu->isoper32) {
		if (cpu->isaddr32) {
			cpu->oper1_32 = cpu->regs.longregs[regeax];
			cpu->oper2_32 = getmem32(cpu, cpu->segcache[reges], cpu->regs.longregs[regedi]);
			flag_sub32(cpu, cpu->oper1_32, cpu->oper2_32);
		}
		else {
			cpu->oper1_32 = cpu->regs.longregs[regeax];
			cpu->oper2_32 = getmem32(cpu, cpu->segcache[reges], cpu->regs.wordregs[regdi]);
			flag_sub32(cpu, cpu->oper1_32, cpu->oper2_32);
		}
	}
	else {
		if (cpu->isaddr32) {
			cpu->oper1 = cpu->regs.wordregs[regax];
			cpu->oper2 = getmem16(cpu, cpu->segcache[reges], cpu->regs.longregs[regedi]);
			flag_sub16(cpu, cpu->oper1, cpu->oper2);
		}
		else {
			cpu->oper1 = cpu->regs.wordregs[regax];
			cpu->oper2 = getmem16(cpu, cpu->segcache[reges], cpu->regs.wordregs[regdi]);
			flag_sub16(cpu, cpu->oper1, cpu->oper2);
		}
	}
	if (cpu->df) {
		if (cpu->isaddr32)
			cpu->regs.longregs[regedi] -= cpu->isoper32 ? 4 : 2;
		else
			cpu->regs.wordregs[regdi] -= cpu->isoper32 ? 4 : 2;
	}
	else {
		if (cpu->isaddr32)
			cpu->regs.longregs[regedi] += cpu->isoper32 ? 4 : 2;
		else
			cpu->regs.wordregs[regdi] += cpu->isoper32 ? 4 : 2;
	}

	if (cpu->reptype) {
		if (cpu->isaddr32)
			cpu->regs.longregs[regecx]--;
		else
			cpu->regs.wordregs[regcx]--;
	}

	if ((cpu->reptype == 1) && !cpu->zf) {
		return;
	}
	else if ((cpu->reptype == 2) && cpu->zf) { //did i fix a typo bug? this used to be & instead of &&
		return;
	}

	if (!cpu->reptype) {
		return;
	}

	cpu->ip = firstip;
}

/* B0 MOV cpu->regs.byteregs[regal] Ib */
void op_B0(CPU_t* cpu) {
	cpu->regs.byteregs[regal] = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
}

/* B1 MOV cpu->regs.byteregs[regcl] Ib */
void op_B1(CPU_t* cpu) {
	cpu->regs.byteregs[regcl] = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
}

/* B2 MOV cpu->regs.byteregs[regdl] Ib */
void op_B2(CPU_t* cpu) {
	cpu->regs.byteregs[regdl] = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
}

/* B3 MOV cpu->regs.byteregs[regbl] Ib */
void op_B3(CPU_t* cpu) {
	cpu->regs.byteregs[regbl] = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
}

/* B4 MOV cpu->regs.byteregs[regah] Ib */
void op_B4(CPU_t* cpu) {
	cpu->regs.byteregs[regah] = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
}

/* B5 MOV cpu->regs.byteregs[regch] Ib */
void op_B5(CPU_t* cpu) {
	cpu->regs.byteregs[regch] = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
}

/* B6 MOV cpu->regs.byteregs[regdh] Ib */
void op_B6(CPU_t* cpu) {
	cpu->regs.byteregs[regdh] = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
}

/* B7 MOV cpu->regs.byteregs[regbh] Ib */
void op_B7(CPU_t* cpu) {
	cpu->regs.byteregs[regbh] = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
}

/* MOV eAX Iv */
/* MOV eCX Iv */
/* MOV eDX Iv */
/* MOV eBX Iv */
/* MOV eSP Iv */
/* MOV eBP Iv */
/* MOV eSI Iv */
/* MOV eDI Iv */
void op_mov(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->oper1_32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
		putreg32(cpu, cpu->opcode & 7, cpu->oper1_32);
	}
	else {
		cpu->oper1 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
		putreg16(cpu, cpu->opcode & 7, cpu->oper1);
	}
}

/* C0 GRP2 byte imm8 (80186+) */
void op_C0(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = readrm8(cpu, cpu->rm);
	cpu->oper2b = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
	writerm8(cpu, cpu->rm, op_grp2_8(cpu, cpu->oper2b));
}

/* C1 GRP2 word imm8 (80186+) */
void op_C1(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = readrm32(cpu, cpu->rm);
		cpu->oper2_32 = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
		writerm32(cpu, cpu->rm, op_grp2_32(cpu, (uint8_t)cpu->oper2_32));
	}
	else {
		cpu->oper1 = readrm16(cpu, cpu->rm);
		cpu->oper2 = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
		writerm16(cpu, cpu->rm, op_grp2_16(cpu, (uint8_t)cpu->oper2));
	}
	StepIP(cpu, 1);
}

/* C2 RET Iw */
void op_C2(CPU_t* cpu) {
	cpu->oper1 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
	cpu->ip = pop(cpu);
	if (cpu->segis32[regss]) {
		cpu->regs.longregs[regesp] += (uint32_t)cpu->oper1;
	}
	else {
		cpu->regs.wordregs[regsp] += cpu->oper1;
	}
}

/* C3 RET */
void op_C3(CPU_t* cpu) {
	cpu->ip = pop(cpu);
}

/* C4 LES Gv Mp */
void op_C4(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->mode == 3) {
		cpu->ip = firstip;
		exception(cpu, 6, 0); //UD
		return;
	}
	getea(cpu, cpu->rm);
	if (cpu->isoper32) {
		putreg32(cpu, cpu->reg, cpu_readl(cpu, cpu->ea) | ((uint32_t)cpu_read(cpu, cpu->ea + 1) << 8) | ((uint32_t)cpu_read(cpu, cpu->ea + 2) << 16) | ((uint32_t)cpu_read(cpu, cpu->ea + 3) << 24));
		putsegreg(cpu, reges, cpu_read(cpu, cpu->ea + 4) | ((uint16_t)cpu_read(cpu, cpu->ea + 5) << 8));
	}
	else {
		putreg16(cpu, cpu->reg, cpu_read(cpu, cpu->ea) + cpu_read(cpu, cpu->ea + 1) * 256);
		putsegreg(cpu, reges, cpu_read(cpu, cpu->ea + 2) + cpu_read(cpu, cpu->ea + 3) * 256);
	}
}

/* C5 LDS Gv Mp */
void op_C5(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->mode == 3) {
		cpu->ip = firstip;
		exception(cpu, 6, 0); //UD
		return;
	}
	getea(cpu, cpu->rm);
	if (cpu->isoper32) {
		putreg32(cpu, cpu->reg, cpu_read(cpu, cpu->ea) | ((uint32_t)cpu_read(cpu, cpu->ea + 1) << 8) | ((uint32_t)cpu_read(cpu, cpu->ea + 2) << 16) | ((uint32_t)cpu_read(cpu, cpu->ea + 3) << 24));
		putsegreg(cpu, regds, cpu_read(cpu, cpu->ea + 4) | ((uint16_t)cpu_read(cpu, cpu->ea + 5) << 8));
	}
	else {
		putreg16(cpu, cpu->reg, cpu_read(cpu, cpu->ea) | ((uint16_t)cpu_read(cpu, cpu->ea + 1) << 8));
		putsegreg(cpu, regds, cpu_read(cpu, cpu->ea + 2) | ((uint16_t)cpu_read(cpu, cpu->ea + 3) << 8));
	}
}

/* C6 MOV Eb Ib */
void op_C6(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->reg != 0) {
		cpu->ip = firstip;
		exception(cpu, 6, 0); //UD
		return;
	}
	writerm8(cpu, cpu->rm, getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
}

/* C7 MOV Ev Iv */
void op_C7(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->reg != 0) {
		cpu->ip = firstip;
		exception(cpu, 6, 0); //UD
		return;
	}
	if (cpu->isoper32) {
		writerm32(cpu, cpu->rm, getmem32(cpu, cpu->segcache[regcs], cpu->ip));
		StepIP(cpu, 4);
	}
	else {
		writerm16(cpu, cpu->rm, getmem16(cpu, cpu->segcache[regcs], cpu->ip));
		StepIP(cpu, 2);
	}
}

/* C8 ENTER (80186+) */
void op_C8(CPU_t* cpu) {
	//if (cpu->isoper32 || cpu->isaddr32) { printf("32-bit op attempt on %02X", cpu->opcode); exit(0); }
	cpu->stacksize = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 2);
	cpu->nestlev = getmem8(cpu, cpu->segcache[regcs], cpu->ip) & 0x1F;
	StepIP(cpu, 1);
	if (cpu->isoper32) {
		push(cpu, cpu->regs.longregs[regebp]);
		cpu->frametemp32 = cpu->regs.longregs[regesp];
		if (cpu->nestlev) {
			for (cpu->temp16 = 1; cpu->temp16 < cpu->nestlev; ++cpu->temp16) {
				cpu->regs.longregs[regebp] -= 4;
				push(cpu, cpu->regs.longregs[regebp]);
			}

			push(cpu, cpu->frametemp32); //cpu->regs.wordregs[regsp]);
		}

		cpu->regs.longregs[regebp] = cpu->frametemp32;
		cpu->regs.longregs[regesp] = cpu->regs.longregs[regebp] - (uint32_t)cpu->stacksize;
	}
	else {
		push(cpu, cpu->regs.wordregs[regbp]);
		cpu->frametemp = cpu->regs.wordregs[regsp];
		if (cpu->nestlev) {
			for (cpu->temp16 = 1; cpu->temp16 < cpu->nestlev; ++cpu->temp16) {
				cpu->regs.wordregs[regbp] -= 2;
				push(cpu, cpu->regs.wordregs[regbp]);
			}

			push(cpu, cpu->frametemp); //cpu->regs.wordregs[regsp]);
		}

		cpu->regs.wordregs[regbp] = cpu->frametemp;
		cpu->regs.wordregs[regsp] = cpu->regs.wordregs[regbp] - cpu->stacksize;
	}
}

/* C9 LEAVE (80186+) */
void op_C9(CPU_t* cpu) {
	if (cpu->segis32[regss]) {
		cpu->regs.longregs[regesp] = cpu->regs.longregs[regebp];
	}
	else {
		cpu->regs.wordregs[regsp] = cpu->regs.wordregs[regbp];
	}
	if (cpu->isoper32) {
		cpu->regs.longregs[regebp] = pop(cpu);
	}
	else {
		cpu->regs.wordregs[regbp] = pop(cpu);
	}
}

/* CA RETF Iw */
void op_CA(CPU_t* cpu) {
	cpu->oper1 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
	cpu_retf(cpu, cpu->oper1);
	//cpu->ip = pop(cpu);
	//putsegreg(cpu, regcs, pop(cpu));
	/*if (cpu->isaddr32) {
		cpu->regs.longregs[regesp] += (uint32_t)cpu->oper1;
	}
	else {
		cpu->regs.wordregs[regsp] += cpu->oper1;
	}*/
}

/* CB RETF */
void op_CB(CPU_t* cpu) {
	cpu_retf(cpu, 0);
	//cpu->ip = pop(cpu);
	//putsegreg(cpu, regcs, pop(cpu));
}

/* CC INT 3 */
void op_CC(CPU_t* cpu) {
	cpu_intcall(cpu, 3, INT_SOURCE_INT3, 0);
}

/* CD INT Ib */
void op_CD(CPU_t* cpu) {
	cpu->oper1b = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
	cpu_intcall(cpu, cpu->oper1b, INT_SOURCE_SOFTWARE, 0);
}

/* CE INTO */
void op_CE(CPU_t* cpu) {
	if (cpu->of) {
		cpu_intcall(cpu, 4, INT_SOURCE_INTO, 0);
	}
}

/* CF IRET */
void op_CF(CPU_t* cpu) {
	cpu_iret(cpu);
}

/* D0 GRP2 Eb 1 */
void op_D0(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = readrm8(cpu, cpu->rm);
	writerm8(cpu, cpu->rm, op_grp2_8(cpu, 1));
}

/* D1 GRP2 Ev 1 */
void op_D1(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = readrm32(cpu, cpu->rm);
		writerm32(cpu, cpu->rm, op_grp2_32(cpu, 1));
	}
	else {
		cpu->oper1 = readrm16(cpu, cpu->rm);
		writerm16(cpu, cpu->rm, op_grp2_16(cpu, 1));
	}
}

/* D2 GRP2 Eb cpu->regs.byteregs[regcl] */
void op_D2(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = readrm8(cpu, cpu->rm);
	writerm8(cpu, cpu->rm, op_grp2_8(cpu, cpu->regs.byteregs[regcl]));
}

/* D3 GRP2 Ev cpu->regs.byteregs[regcl] */
void op_D3(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = readrm32(cpu, cpu->rm);
		writerm32(cpu, cpu->rm, op_grp2_32(cpu, cpu->regs.byteregs[regcl]));
	}
	else {
		cpu->oper1 = readrm16(cpu, cpu->rm);
		writerm16(cpu, cpu->rm, op_grp2_16(cpu, cpu->regs.byteregs[regcl]));
	}
}

/* D4 AAM I0 */
void op_D4(CPU_t* cpu) {
	uint8_t old_al;

	cpu->oper1 = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
	if (!cpu->oper1) {
		//cpu_intcall(cpu, 0, INT_SOURCE_EXCEPTION, 0);
		exception(cpu, 0, 0); //DE
		return;
	}	/* division by zero */

	old_al = cpu->regs.byteregs[regal];
	cpu->regs.byteregs[regah] = (old_al / cpu->oper1) & 255;
	cpu->regs.byteregs[regal] = (old_al % cpu->oper1) & 255;
	flag_szp8(cpu, cpu->regs.byteregs[regal]);
}

/* D5 AAD I0 */
void op_D5(CPU_t* cpu) {
	uint8_t old_al;
	uint8_t old_ah;

	cpu->oper1 = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
	old_al = cpu->regs.byteregs[regal];
	old_ah = cpu->regs.byteregs[regah];
	cpu->regs.byteregs[regal] = (old_ah * cpu->oper1 + old_al) & 255;
	cpu->regs.byteregs[regah] = 0;
	flag_szp8(cpu, cpu->regs.byteregs[regal]);
}

/* D6 XLAT on 80186+, SALC on 8086/8088 */
/* D7 XLAT */
void op_D6_D7(CPU_t* cpu) {
	if (cpu->isaddr32) {
		cpu->regs.byteregs[regal] = cpu_read(cpu, cpu->useseg + (uint32_t)cpu->regs.longregs[regebx] + (uint32_t)cpu->regs.byteregs[regal]);
	}
	else {
		cpu->regs.byteregs[regal] = cpu_read(cpu, cpu->useseg + (uint32_t)cpu->regs.wordregs[regbx] + (uint32_t)cpu->regs.byteregs[regal]);
	}
}

/* escape to FPU */
void op_fpu(CPU_t* cpu) {
	unsigned int op;
	bool ok;

	if (cpu_fpu_check_nm(cpu)) {
		return;
	}

	if (!cpu->fpu) {
		cpu->ip = firstip;
		exception(cpu, 6, 0); //UD
		return;
	}

	modregrm(cpu);
	if (cpu->doexception) {
		return;
	}

	op = cpu->opcode & 7;
	if (cpu->mode == 3) {
		ok = fpu_exec1(cpu->fpu, cpu, op, cpu->reg, cpu->rm);
	}
	else {
		getea(cpu, cpu->rm);
		if (cpu->doexception) {
			return;
		}
		ok = fpu_exec2(cpu->fpu, cpu, !cpu->isoper32, op, cpu->reg, cpu->currentseg, cpu->ea);
	}

	if (!ok) {
		return;
	}
}

/* E0 LOOPNZ Jb */
void op_E0(CPU_t* cpu) {
	cpu->temp32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
	if (cpu->isaddr32) {
		cpu->regs.longregs[regecx]--;
		if ((cpu->regs.longregs[regecx]) && !cpu->zf) {
			cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
		}
	}
	else {
		cpu->regs.wordregs[regcx]--;
		if ((cpu->regs.wordregs[regcx]) && !cpu->zf) {
			cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
		}
	}
}

/* E1 LOOPZ Jb */
void op_E1(CPU_t* cpu) {
	cpu->temp32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
	if (cpu->isaddr32) {
		cpu->regs.longregs[regecx]--;
		if (cpu->regs.longregs[regecx] && (cpu->zf == 1)) {
			cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
		}
	}
	else {
		cpu->regs.wordregs[regcx]--;
		if (cpu->regs.wordregs[regcx] && (cpu->zf == 1)) {
			cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
		}
	}
}

/* E2 LOOP Jb */
void op_E2(CPU_t* cpu) {
	cpu->temp32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
	if (cpu->isaddr32) {
		cpu->regs.longregs[regecx]--;
		if (cpu->regs.longregs[regecx]) {
			cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
		}
	}
	else {
		cpu->regs.wordregs[regcx]--;
		if (cpu->regs.wordregs[regcx]) {
			cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
		}
	}
}

/* E3 JCXZ Jb */
void op_E3(CPU_t* cpu) {
	cpu->temp32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
	if (cpu->isaddr32) {
		if (!cpu->regs.longregs[regecx]) {
			cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
		}
	}
	else {
		if (!cpu->regs.wordregs[regcx]) {
			cpu_apply_relative_branch(cpu, (int32_t)cpu->temp32);
		}
	}
}

/* E4 IN cpu->regs.byteregs[regal] Ib */
void op_E4(CPU_t* cpu) {
	cpu->oper1b = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);

	if (!check_io_permission(cpu, cpu->oper1b, 8)) 
		return;

	cpu->regs.byteregs[regal] = (uint8_t)port_read(cpu, cpu->oper1b);
}

/* E5 IN eAX Ib */
void op_E5(CPU_t* cpu) {
	cpu->oper1b = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);

	if (!check_io_permission(cpu, cpu->oper1b, cpu->isoper32 ? 32 : 16)) 
		return;

	if (cpu->isoper32) {
		cpu->regs.longregs[regeax] = port_readl(cpu, cpu->oper1b);
	}
	else {
		cpu->regs.wordregs[regax] = port_readw(cpu, cpu->oper1b);
	}
}

/* E6 OUT Ib cpu->regs.byteregs[regal] */
void op_E6(CPU_t* cpu) {
	cpu->oper1b = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);

	if (!check_io_permission(cpu, cpu->oper1b, 8)) 
		return;

	port_write(cpu, cpu->oper1b, cpu->regs.byteregs[regal]);
}

/* E7 OUT Ib eAX */
void op_E7(CPU_t* cpu) {
	cpu->oper1b = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);

	if (!check_io_permission(cpu, cpu->oper1b, cpu->isoper32 ? 32 : 16)) 
		return;

	if (cpu->isoper32) {
		port_writel(cpu, cpu->oper1b, cpu->regs.longregs[regeax]);
	}
	else {
		port_writew(cpu, cpu->oper1b, cpu->regs.wordregs[regax]);
	}
}

/* E8 CALL Jv */
void op_E8(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->oper1_32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
		push(cpu, cpu->ip);
	}
	else {
		cpu->oper1_32 = (int32_t)(int16_t)getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
		push(cpu, cpu->ip);
	}
	cpu->ip = cpu->ip + cpu->oper1_32;
	if (!cpu->isoper32) {
		cpu->ip &= 0xFFFF;
	}
}

/* E9 JMP Jv */
void op_E9(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->oper1_32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
	}
	else {
		cpu->oper1_32 = (int32_t)(int16_t)getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
	}
	cpu->ip = cpu->ip + cpu->oper1_32;
	if (!cpu->isoper32) {
		cpu->ip &= 0xFFFF;
	}
}

/* EA JMP Ap */
void op_EA(CPU_t* cpu) {
	if (cpu->isoper32) {
		cpu->oper1_32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 4);
		cpu->oper2 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
		cpu_jmpf(cpu, cpu->oper2, cpu->oper1_32);
		//debug_log(DEBUG_DETAIL, "32-bit far jump CS = %04X, IP = %08X", cpu->oper2, cpu->oper1_32);
	}
	else {
		cpu->oper1 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
		cpu->oper2 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
		StepIP(cpu, 2);
		cpu_jmpf(cpu, cpu->oper2, cpu->oper1);
		//debug_log(DEBUG_DETAIL, "16-bit far jump CS = %04X, IP = %04X", cpu->oper2, cpu->oper1);
	}
}

/* EB JMP Jb */
void op_EB(CPU_t* cpu) {
	cpu->oper1_32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
	StepIP(cpu, 1);
	cpu->ip = cpu->ip + cpu->oper1_32;
	if (!cpu->isoper32) {
		cpu->ip &= 0xFFFF;
	}
}

/* EC IN cpu->regs.byteregs[regal] regdx */
void op_EC(CPU_t* cpu) {
	cpu->oper1 = cpu->regs.wordregs[regdx];

	if (!check_io_permission(cpu, cpu->oper1, 8)) 
		return;

	cpu->regs.byteregs[regal] = (uint8_t)port_read(cpu, cpu->oper1);
}

/* ED IN eAX regdx */
void op_ED(CPU_t* cpu) {
	cpu->oper1 = cpu->regs.wordregs[regdx];

	if (!check_io_permission(cpu, cpu->oper1, cpu->isoper32 ? 32 : 16)) 
		return;

	if (cpu->isoper32) {
		cpu->regs.longregs[regeax] = port_readl(cpu, cpu->oper1);
	}
	else {
		cpu->regs.wordregs[regax] = port_readw(cpu, cpu->oper1);
	}
}

/* EE OUT regdx cpu->regs.byteregs[regal] */
void op_EE(CPU_t* cpu) {
	cpu->oper1 = cpu->regs.wordregs[regdx];

	if (!check_io_permission(cpu, cpu->oper1, 8)) 
		return;

	port_write(cpu, cpu->oper1, cpu->regs.byteregs[regal]);
}

/* EF OUT regdx eAX */
void op_EF(CPU_t* cpu) {
	cpu->oper1 = cpu->regs.wordregs[regdx];

	if (!check_io_permission(cpu, cpu->oper1, cpu->isoper32 ? 32 : 16)) 
		return;

	if (cpu->isoper32) {
		port_writel(cpu, cpu->oper1, cpu->regs.longregs[regeax]);
	}
	else {
		port_writew(cpu, cpu->oper1, cpu->regs.wordregs[regax]);
	}
}

/* CC INT 1 */
void op_F1(CPU_t* cpu) {
	cpu_intcall(cpu, 1, INT_SOURCE_SOFTWARE, 0);
}

/* F4 HLT */
void op_F4(CPU_t* cpu) {
	if (cpu->v86f || (cpu->protected_mode && (cpu->cpl != 0))) {
		exception(cpu, 13, 0); //GP(0)
		return;
	}
	cpu->hltstate = 1;
}

/* F5 CMC */
void op_F5(CPU_t* cpu) {
	if (!cpu->cf) {
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0;
	}
}

/* F6 GRP3a Eb */
void op_F6(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = readrm8(cpu, cpu->rm);
	op_grp3_8(cpu);
	if ((cpu->reg > 1) && (cpu->reg < 4)) {
		writerm8(cpu, cpu->rm, cpu->res8);
	}
}

/* F7 GRP3b Ev */
void op_F7(CPU_t* cpu) {
	modregrm(cpu);
	if (cpu->isoper32) {
		cpu->oper1_32 = readrm32(cpu, cpu->rm);
		op_grp3_32(cpu);
		if ((cpu->reg > 1) && (cpu->reg < 4)) {
			writerm32(cpu, cpu->rm, cpu->res32);
		}
	}
	else {
		cpu->oper1 = readrm16(cpu, cpu->rm);
		op_grp3_16(cpu);
		if ((cpu->reg > 1) && (cpu->reg < 4)) {
			writerm16(cpu, cpu->rm, cpu->res16);
		}
	}
}

/* F8 CLC */
void op_F8(CPU_t* cpu) {
	cpu->cf = 0;
}

/* F9 STC */
void op_F9(CPU_t* cpu) {
	cpu->cf = 1;
}

/* FA CLI */
void op_FA(CPU_t* cpu) {
	if (cpu->protected_mode) {
		if (cpu->iopl >= cpu->cpl) {
			cpu->ifl = 0;
		}
		else {
			exception(cpu, 13, 0); //GP(0)
		}
	}
	else {
		cpu->ifl = 0;
	}
}

/* FB STI */
void op_FB(CPU_t* cpu) {
	if (cpu->protected_mode) {
		if (cpu->iopl >= cpu->cpl) {
			cpu->ifl = 1;
			cpu_begin_interrupt_shadow(cpu);
		}
		else {
			exception(cpu, 13, 0); //GP(0)
		}
	}
	else {
		cpu->ifl = 1;
		cpu_begin_interrupt_shadow(cpu);
	}
}

/* FC CLD */
void op_FC(CPU_t* cpu) {
	cpu->df = 0;
}

/* FD STD */
void op_FD(CPU_t* cpu) {
	cpu->df = 1;
}

/* FE GRP4 Eb */
void op_FE(CPU_t* cpu) {
	modregrm(cpu);
	cpu->oper1b = readrm8(cpu, cpu->rm);
	cpu->oper2b = 1;
	if (!cpu->reg) {
		cpu->tempcf = cpu->cf;
		cpu->res8 = cpu->oper1b + cpu->oper2b;
		flag_add8(cpu, cpu->oper1b, cpu->oper2b);
		cpu->cf = cpu->tempcf;
		writerm8(cpu, cpu->rm, cpu->res8);
	}
	else {
		cpu->tempcf = cpu->cf;
		cpu->res8 = cpu->oper1b - cpu->oper2b;
		flag_sub8(cpu, cpu->oper1b, cpu->oper2b);
		cpu->cf = cpu->tempcf;
		writerm8(cpu, cpu->rm, cpu->res8);
	}
}

/* FF GRP5 Ev */
void op_FF(CPU_t* cpu) {
	if (cpu->isoper32) {
		//if (((getmem8(cpu, cpu->segcache[regcs], cpu->ip) >> 3) & 7) == 6) {
			//special case, if it's a PUSH op, then shadow ESP has to be pre-decremented in case EA is calculated with an ESP base
			//cpu->shadow_esp -= 4;
		//}
		modregrm(cpu);
		cpu->oper1_32 = readrm32(cpu, cpu->rm);
		op_grp5_32(cpu);
		//debug_log(DEBUG_DETAIL, "EA was %08X", cpu->ea - cpu->useseg);
	}
	else {
		modregrm(cpu);
		cpu->oper1 = readrm16(cpu, cpu->rm);
		op_grp5(cpu);
	}
}

void op_illegal(CPU_t* cpu) {
	cpu->ip = firstip;
	exception(cpu, 6, 0); //UD(0)
	debug_log(DEBUG_DETAIL, "[CPU] Invalid opcode exception at %04X:%04X (%02X)\r\n", cpu->segregs[regcs], firstip, cpu->opcode);
}

void (*opcode_table[256])(CPU_t* cpu) = {
	op_00, op_01, op_02, op_03, op_04, op_05, op_06, op_07,
	op_08, op_09, op_0A, op_0B, op_0C, op_0D, op_0E, cpu_extop,
	op_10, op_11, op_12, op_13, op_14, op_15, op_16, op_17,
	op_18, op_19, op_1A, op_1B, op_1C, op_1D, op_1E, op_1F,
	op_20, op_21, op_22, op_23, op_24, op_25, op_illegal, op_27,
	op_28, op_29, op_2A, op_2B, op_2C, op_2D, op_illegal, op_2F,
	op_30, op_31, op_32, op_33, op_34, op_35, op_illegal, op_37,
	op_38, op_39, op_3A, op_3B, op_3C, op_3D, op_illegal, op_3F,
	op_inc, op_inc, op_inc, op_inc, op_inc, op_inc, op_inc, op_inc,
	op_dec, op_dec, op_dec, op_dec, op_dec, op_dec, op_dec, op_dec,
	op_push, op_push, op_push, op_push, op_54, op_push, op_push, op_push,
	op_pop, op_pop, op_pop, op_pop, op_pop, op_pop, op_pop, op_pop,
	op_60, op_61, op_62, op_63, op_illegal, op_illegal, op_illegal, op_illegal,
	op_68, op_69, op_6A, op_6B, op_6C, op_6D, op_6E, op_6F,
	op_70, op_71, op_72, op_73, op_74, op_75, op_76, op_77,
	op_78, op_79, op_7A, op_7B, op_7C, op_7D, op_7E, op_7F,
	op_80_82, op_81_83, op_80_82, op_81_83, op_84, op_85, op_86, op_87,
	op_88, op_89, op_8A, op_8B, op_8C, op_8D, op_8E, op_8F,
	op_90, op_xchg, op_xchg, op_xchg, op_xchg, op_xchg, op_xchg, op_xchg,
	op_98, op_99, op_9A, op_9B, op_9C, op_9D, op_9E, op_9F,
	op_A0, op_A1, op_A2, op_A3, op_A4, op_A5, op_A6, op_A7,
	op_A8, op_A9, op_AA, op_AB, op_AC, op_AD, op_AE, op_AF,
	op_B0, op_B1, op_B2, op_B3, op_B4, op_B5, op_B6, op_B7,
	op_mov, op_mov, op_mov, op_mov, op_mov, op_mov, op_mov, op_mov,
	op_C0, op_C1, op_C2, op_C3, op_C4, op_C5, op_C6, op_C7,
	op_C8, op_C9, op_CA, op_CB, op_CC, op_CD, op_CE, op_CF,
	op_D0, op_D1, op_D2, op_D3, op_D4, op_D5, op_D6_D7, op_D6_D7,
	op_fpu, op_fpu, op_fpu, op_fpu, op_fpu, op_fpu, op_fpu, op_fpu,
	op_E0, op_E1, op_E2, op_E3, op_E4, op_E5, op_E6, op_E7,
	op_E8, op_E9, op_EA, op_EB, op_EC, op_ED, op_EE, op_EF,
	op_illegal, op_F1, op_illegal, op_illegal, op_F4, op_F5, op_F6, op_F7,
	op_F8, op_F9, op_FA, op_FB, op_FC, op_FD, op_FE, op_FF,
};

void cpu_exec(CPU_t* cpu, I8259_t* i8259, I8259_t* i8259_slave, uint32_t execloops) {

	uint32_t loopcount;
	uint8_t docontinue;
	uint8_t bufop[12];
	CPU_EXEC_SNAPSHOT_t exec_snapshot;

	for (loopcount = 0; loopcount < execloops; loopcount++) {
		//EXTREME HACK ALERT (saving CPU state in case we need to restore later on an exception)
		cpu_snapshot_exec_state(cpu, &exec_snapshot);
		//END EXTREME HACK ALERT
		cpu->shadow_esp = cpu->regs.longregs[regesp];

		cpu->doexception = 0;
		cpu->exceptionerr = 0;
		cpu->nowrite = 0;
		cpu->exceptionip = cpu->ip;
		cpu->startcpl = cpu->cpl;

		if (cpu->trap_toggle) {
			//cpu_intcall(cpu, 1, INT_SOURCE_EXCEPTION, 0);
			exception(cpu, 1, 0); //GB
		}

		if (cpu->tf) {
			cpu->trap_toggle = 1;
		}
		else {
			cpu->trap_toggle = 0;
		}

		if (cpu->interrupt_inhibit) {
			cpu->interrupt_inhibit--;
		}
		else if (!cpu->doexception) {
			int doInt = 0;
			if (cpu_interruptCheck(cpu, i8259, 0)) {
				doInt = 1;
			}
			if (doInt) {
				// A hardware IRQ can switch privilege levels before the next instruction begins.
				// Refresh the "current instruction" snapshot so the first handler instruction
				// sees the correct CPL, ESP base, and rollback state.
				cpu_snapshot_exec_state(cpu, &exec_snapshot);
				cpu->shadow_esp = cpu->regs.longregs[regesp];
				cpu->exceptionip = cpu->ip;
				cpu->startcpl = cpu->cpl;
			}
		}

		if (cpu->hltstate) goto skipexecution;

		cpu->reptype = 0;
		cpu->segoverride = 0;
		cpu->useseg = cpu->segcache[regds];
		cpu->currentseg = regds;
		if (cpu->segis32[regcs] && !cpu->v86f) {
			cpu->isaddr32 = 1;
			cpu->isoper32 = 1;
		}
		else {
			cpu->isaddr32 = 0;
			cpu->isoper32 = 0;
		}
		docontinue = 0;
		firstip = cpu->segis32[regcs] ? cpu->ip : cpu->ip & 0xFFFF;
		if (showops) for (uint32_t i = 0; i < 12; i++) {
			bufop[i] = cpu_read(cpu, cpu->segcache[regcs] + cpu->ip + i);
		}

		while (!docontinue) {
			if (!cpu->segis32[regcs] || cpu->v86f) cpu->ip = cpu->ip & 0xFFFF;
			cpu->savecs = cpu->segregs[regcs];
			cpu->saveip = cpu->ip;
			cpu->opcode = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
			StepIP(cpu, 1);

			switch (cpu->opcode) {
				/* segment prefix check */
			case 0x2E:	/* segment cpu->segregs[regcs] */
				cpu->useseg = cpu->segcache[regcs];
				cpu->segoverride = 1;
				cpu->currentseg = regcs;
				break;

			case 0x3E:	/* segment cpu->segregs[regds] */
				cpu->useseg = cpu->segcache[regds];
				cpu->segoverride = 1;
				cpu->currentseg = regds;
				break;

			case 0x26:	/* segment cpu->segregs[reges] */
				cpu->useseg = cpu->segcache[reges];
				cpu->segoverride = 1;
				cpu->currentseg = reges;
				break;

			case 0x36:	/* segment cpu->segregs[regss] */
				cpu->useseg = cpu->segcache[regss];
				cpu->segoverride = 1;
				cpu->currentseg = regss;
				break;

			case 0x64: /* segment cpu->segregs[regfs] */
				cpu->useseg = cpu->segcache[regfs];
				cpu->segoverride = 1;
				cpu->currentseg = regfs;
				break;

			case 0x65: /* segment cpu->segregs[reggs] */
				cpu->useseg = cpu->segcache[reggs];
				cpu->segoverride = 1;
				cpu->currentseg = reggs;
				break;

			case 0x66: /* operand size override */
				cpu->isoper32 ^= 1;
				break;

			case 0x67: /* address size override */
				cpu->isaddr32 ^= 1;
				break;

			case 0xF0:	/* F0 LOCK */
				break;

				/* repetition prefix check */
			case 0xF3:	/* REP/REPE/REPZ */
				cpu->reptype = 1;
				break;

			case 0xF2:	/* REPNE/REPNZ */
				cpu->reptype = 2;
				break;

			default:
				docontinue = 1;
				break;
			}
		}

		cpu->totalexec++;

		(*opcode_table[cpu->opcode])(cpu);

		//if (cpu->startcpl == 3) showops = 1; else showops = 0;

		if (showops) { // || cpu->doexception) {
			uint32_t i;
			//if (cpu->isoper32) printf("32-bit operand %02X\n", cpu->opcode);
			//if (cpu->isaddr32) printf("32-bit addressing %02X\n", cpu->opcode);
			if (cpu->doexception) debug_log(DEBUG_DETAIL, "Exception %u, CPU state:\n", cpu->exceptionval);
			if (!cpu->reptype || (cpu->reptype && !cpu->regs.wordregs[regcx])) {
				if (cpu->protected_mode && !cpu->v86f)
					debug_log(DEBUG_DETAIL, "%08X: ", cpu->segcache[regcs] + firstip);
				else
					debug_log(DEBUG_DETAIL, "%04X:%04X ", cpu->savecs, firstip);
				for (i = 0; i < 12; i++) {
					debug_log(DEBUG_DETAIL, "%02X ", bufop[i]);
				}
				debug_log(DEBUG_DETAIL, "\n");
				/*if (cpu->isoper32)
					debug_log(DEBUG_DETAIL, "oper1_32 = %08X, oper2_32 = %08X\n", cpu->oper1_32, cpu->oper2_32);
				else
					debug_log(DEBUG_DETAIL, "oper1 = %04X, oper2 = %04X\n", cpu->oper1, cpu->oper2);*/
				debug_log(DEBUG_DETAIL, "\taddr32: %u\toper32: %u\t TF: %u\tIF: %u\tV86: %u\n", cpu->isaddr32, cpu->isoper32, cpu->tf, cpu->ifl, cpu->v86f);
				debug_log(DEBUG_DETAIL, "\tEAX: %08X\tECX: %08X\tEDX: %08X\tEBX: %08X\n",
					cpu->regs.longregs[regeax], cpu->regs.longregs[regecx], cpu->regs.longregs[regedx], cpu->regs.longregs[regebx]);
				debug_log(DEBUG_DETAIL, "\tESI: %08X\tEDI: %08X\tESP: %08X\tEBP: %08X\n",
					cpu->regs.longregs[regesi], cpu->regs.longregs[regedi], cpu->regs.longregs[regesp], cpu->regs.longregs[regebp]);
				debug_log(DEBUG_DETAIL, "\tCS: %04X\tDS: %04X\tES: %04X\tSS: %04X\tFS: %04X\tGS: %04X\n\n",
					cpu->segregs[regcs], cpu->segregs[regds], cpu->segregs[reges], cpu->segregs[regss], cpu->segregs[regfs], cpu->segregs[reggs]);
				//cpu_debug_state(cpu);
				//getch();
			}
		}

		if (cpu->doexception) {
			// EXTREME HACK ALERT
			cpu_restore_exec_state(cpu, &exec_snapshot);
			// END EXTREME HACK ALERT
			wrcache_init(); //there was an exception, so flush memory write cache -- the faulting instruction should not modify anything
			cpu_intcall(cpu, cpu->exceptionval, INT_SOURCE_EXCEPTION, cpu->exceptionerr);
		}

	skipexecution:
		;

		wrcache_flush(); //flush/commit memory write cache
	}

}
