#ifndef _CPUHELPER_H_
#define _CPUHELPER_H_

enum {
	TASK_SWITCH_REASON_CALL = 0,
	TASK_SWITCH_REASON_GATE = 1,
	TASK_SWITCH_REASON_IRET = 2,
	TASK_SWITCH_REASON_JMP = 3,
};

typedef struct {
	uint32_t addr;
	uint8_t access;
	uint8_t flags;
	uint8_t dpl;
} SEGDESC_t;

typedef struct {
	uint16_t selector;
	uint8_t target_cpl;
	uint8_t outer;
} RETURNCS_t;

typedef struct {
	uint32_t addr;
	uint16_t target_selector;
	uint32_t offset;
	uint8_t access;
	uint8_t flags;
	uint8_t type;
	uint8_t dpl;
	uint8_t present;
	uint8_t param_count;
} GATEDESC_t;

typedef struct {
	uint16_t selector;
	uint8_t target_cpl;
	uint8_t outer;
	uint8_t conforming;
} CODETARGET_t;

typedef struct {
	union _bytewordregs_ regs;
	uint16_t segregs[6];
	uint32_t segcache[6];
	uint8_t segis32[6];
	uint32_t seglimit[6];
	uint32_t flags;
	uint8_t cpl;
	uint8_t isCS32;
	uint8_t protected_mode;
	uint8_t paging;
	uint8_t usegdt;
	uint32_t gdtr, gdtl;
	uint32_t idtr, idtl;
	uint32_t ldtr, ldtl;
	uint32_t trbase, trlimit;
	uint16_t ldt_selector, tr_selector;
	uint8_t trtype;
} CPU_EXEC_SNAPSHOT_t;

static FUNC_INLINE void cpu_callf(CPU_t* cpu, uint16_t selector, uint32_t ip);
static FUNC_INLINE void cpu_jmpf(CPU_t* cpu, uint16_t selector, uint32_t ip);
static FUNC_INLINE void cpu_retf(CPU_t* cpu, uint32_t adjust);
static FUNC_INLINE void pushw(CPU_t* cpu, uint16_t pushval);
static FUNC_INLINE void pushl(CPU_t* cpu, uint32_t pushval);

static FUNC_FORCE_INLINE uint16_t cpu_selector_offset(uint16_t selector) {
	return selector & 0xFFF8;
}

static FUNC_FORCE_INLINE uint16_t cpu_selector_error_code(uint16_t selector) {
	return selector & 0xFFFC;
}

static FUNC_FORCE_INLINE uint16_t cpu_normalize_selector(uint16_t selector, uint8_t rpl) {
	return (selector & 0xFFFC) | (rpl & 3);
}

static FUNC_FORCE_INLINE uint32_t cpu_idt_error_code(uint8_t intnum, uint8_t source) {
	return ((uint32_t)intnum << 3) | 2 |
		(((source == INT_SOURCE_SOFTWARE) || (source == INT_SOURCE_INT3) || (source == INT_SOURCE_INTO)) ? 0 : 1);
}

static FUNC_FORCE_INLINE void cpu_apply_relative_branch(CPU_t* cpu, int32_t displacement) {
	cpu->ip += (uint32_t)displacement;
	if (!cpu->isoper32) {
		cpu->ip &= 0xFFFF;
	}
}

static FUNC_FORCE_INLINE uint32_t cpu_selector_table_base(CPU_t* cpu, uint16_t selector) {
	return (selector & 4) ? cpu->ldtr : cpu->gdtr;
}

static FUNC_FORCE_INLINE uint32_t cpu_selector_table_limit(CPU_t* cpu, uint16_t selector) {
	return (selector & 4) ? cpu->ldtl : cpu->gdtl;
}

static FUNC_FORCE_INLINE int cpu_read_segdesc(CPU_t* cpu, uint16_t selector, SEGDESC_t* desc) {
	uint32_t offset = cpu_selector_offset(selector);

	if ((offset + 7) > cpu_selector_table_limit(cpu, selector)) {
		return 0;
	}

	desc->addr = cpu_selector_table_base(cpu, selector) + offset;
	desc->access = cpu_read_sys(cpu, desc->addr + 5);
	desc->flags = cpu_read_sys(cpu, desc->addr + 6);
	desc->dpl = (desc->access >> 5) & 3;
	return 1;
}

static FUNC_FORCE_INLINE int cpu_desc_is_present(const SEGDESC_t* desc) {
	return (desc->access & 0x80) != 0;
}

static FUNC_FORCE_INLINE int cpu_desc_is_system(const SEGDESC_t* desc) {
	return (desc->access & 0x10) == 0;
}

static FUNC_FORCE_INLINE int cpu_desc_is_code(const SEGDESC_t* desc) {
	return (desc->access & 0x18) == 0x18;
}

static FUNC_FORCE_INLINE int cpu_desc_is_conforming_code(const SEGDESC_t* desc) {
	return cpu_desc_is_code(desc) && (desc->access & 0x04);
}

static FUNC_FORCE_INLINE int cpu_desc_is_readable_code(const SEGDESC_t* desc) {
	return cpu_desc_is_code(desc) && (desc->access & 0x02);
}

static FUNC_FORCE_INLINE int cpu_desc_is_writable_data(const SEGDESC_t* desc) {
	return ((desc->access & 0x18) == 0x10) && (desc->access & 0x02);
}

static FUNC_FORCE_INLINE int cpu_desc_type(const SEGDESC_t* desc) {
	return desc->access & 0x0F;
}

static FUNC_FORCE_INLINE int cpu_desc_is_call_gate(const SEGDESC_t* desc) {
	return cpu_desc_is_system(desc) && ((cpu_desc_type(desc) == 0x4) || (cpu_desc_type(desc) == 0xC));
}

static FUNC_FORCE_INLINE int cpu_desc_is_task_gate(const SEGDESC_t* desc) {
	return cpu_desc_is_system(desc) && (cpu_desc_type(desc) == 0x5);
}

static FUNC_FORCE_INLINE int cpu_desc_is_tss(const SEGDESC_t* desc) {
	return cpu_desc_is_system(desc) && ((cpu_desc_type(desc) == 0x9) || (cpu_desc_type(desc) == 0xB));
}

static FUNC_FORCE_INLINE int cpu_desc_is_busy_tss(const SEGDESC_t* desc) {
	return cpu_desc_is_system(desc) && (cpu_desc_type(desc) == 0xB);
}

static FUNC_FORCE_INLINE int cpu_gate_is_interrupt_or_trap(const GATEDESC_t* gate) {
	return (gate->type == 0x6) || (gate->type == 0x7) || (gate->type == 0xE) || (gate->type == 0xF);
}

static FUNC_FORCE_INLINE int cpu_gate_is_interrupt(const GATEDESC_t* gate) {
	return (gate->type == 0x6) || (gate->type == 0xE);
}

static FUNC_FORCE_INLINE int cpu_gate_is_32bit(const GATEDESC_t* gate) {
	return (gate->type == 0xC) || (gate->type == 0xE) || (gate->type == 0xF);
}

static FUNC_FORCE_INLINE int cpu_exception_has_error_code(uint8_t intnum) {
	switch (intnum) {
	case 8:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 17:
		return 1;
	default:
		return 0;
	}
}

static FUNC_FORCE_INLINE int cpu_interrupt_frame_size16(int include_stack, int has_err) {
	return (include_stack ? 4 : 0) + 6 + (has_err ? 2 : 0);
}

static FUNC_FORCE_INLINE int cpu_interrupt_frame_size32(int include_stack, int include_vm86, int has_err) {
	return (include_vm86 ? 16 : 0) + (include_stack ? 8 : 0) + 12 + (has_err ? 4 : 0);
}

static FUNC_FORCE_INLINE void cpu_push_far_return16(CPU_t* cpu, uint16_t old_cs, uint32_t old_ip) {
	pushw(cpu, old_cs);
	pushw(cpu, (uint16_t)old_ip);
}

static FUNC_FORCE_INLINE void cpu_push_far_return32(CPU_t* cpu, uint16_t old_cs, uint32_t old_ip) {
	pushl(cpu, old_cs);
	pushl(cpu, old_ip);
}

static FUNC_FORCE_INLINE void cpu_push_far_return16_sys(CPU_t* cpu, uint16_t old_cs, uint32_t old_ip) {
	cpu_writew_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 2) : (cpu->regs.wordregs[regsp] -= 2))), old_cs);
	cpu_writew_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 2) : (cpu->regs.wordregs[regsp] -= 2))), (uint16_t)old_ip);
}

static FUNC_FORCE_INLINE void cpu_push_far_return32_sys(CPU_t* cpu, uint16_t old_cs, uint32_t old_ip) {
	cpu_writel_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 4) : (cpu->regs.wordregs[regsp] -= 4))), old_cs);
	cpu_writel_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 4) : (cpu->regs.wordregs[regsp] -= 4))), old_ip);
}

static FUNC_FORCE_INLINE void cpu_push_interrupt_frame16(CPU_t* cpu, int include_stack, uint16_t old_ss, uint32_t old_esp, uint32_t old_flags, uint16_t old_cs, uint32_t old_ip, int has_err, uint32_t err) {
	if (include_stack) {
		pushw(cpu, old_ss);
		pushw(cpu, (uint16_t)old_esp);
	}
	pushw(cpu, (uint16_t)old_flags);
	pushw(cpu, old_cs);
	pushw(cpu, (uint16_t)old_ip);
	if (has_err) {
		pushw(cpu, (uint16_t)err);
	}
}

static FUNC_FORCE_INLINE void cpu_push_interrupt_frame32(CPU_t* cpu, int include_stack, int include_vm86, uint16_t old_ss, uint32_t old_esp, uint32_t old_flags, uint16_t old_cs, uint32_t old_ip, int has_err, uint32_t err) {
	if (include_vm86) {
		pushl(cpu, cpu->segregs[reggs]);
		pushl(cpu, cpu->segregs[regfs]);
		pushl(cpu, cpu->segregs[regds]);
		pushl(cpu, cpu->segregs[reges]);
	}
	if (include_stack) {
		pushl(cpu, old_ss);
		pushl(cpu, old_esp);
	}
	pushl(cpu, old_flags);
	pushl(cpu, old_cs);
	pushl(cpu, old_ip);
	if (has_err) {
		pushl(cpu, err);
	}
}

static FUNC_FORCE_INLINE void cpu_push_interrupt_frame16_sys(CPU_t* cpu, int include_stack, uint16_t old_ss, uint32_t old_esp, uint32_t old_flags, uint16_t old_cs, uint32_t old_ip, int has_err, uint32_t err) {
	if (include_stack) {
		cpu_push_far_return16_sys(cpu, old_ss, old_esp);
	}
	cpu_push_far_return16_sys(cpu, (uint16_t)old_flags, old_cs);
	cpu_writew_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 2) : (cpu->regs.wordregs[regsp] -= 2))), (uint16_t)old_ip);
	if (has_err) {
		cpu_writew_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 2) : (cpu->regs.wordregs[regsp] -= 2))), (uint16_t)err);
	}
}

static FUNC_FORCE_INLINE void cpu_push_interrupt_frame32_sys(CPU_t* cpu, int include_stack, int include_vm86, uint16_t old_ss, uint32_t old_esp, uint32_t old_flags, uint16_t old_cs, uint32_t old_ip, int has_err, uint32_t err) {
	if (include_vm86) {
		cpu_writel_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 4) : (cpu->regs.wordregs[regsp] -= 4))), cpu->segregs[reggs]);
		cpu_writel_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 4) : (cpu->regs.wordregs[regsp] -= 4))), cpu->segregs[regfs]);
		cpu_writel_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 4) : (cpu->regs.wordregs[regsp] -= 4))), cpu->segregs[regds]);
		cpu_writel_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 4) : (cpu->regs.wordregs[regsp] -= 4))), cpu->segregs[reges]);
	}
	if (include_stack) {
		cpu_writel_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 4) : (cpu->regs.wordregs[regsp] -= 4))), old_ss);
		cpu_writel_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 4) : (cpu->regs.wordregs[regsp] -= 4))), old_esp);
	}
	// Preserve the full 32-bit EFLAGS image here. VM86 returns rely on VM/RF
	// bits living above bit 15; truncating to 16 bits turns a vm86 return frame
	// into a normal protected-mode outer return frame.
	cpu_writel_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 4) : (cpu->regs.wordregs[regsp] -= 4))), old_flags);
	cpu_writel_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 4) : (cpu->regs.wordregs[regsp] -= 4))), old_cs);
	cpu_writel_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 4) : (cpu->regs.wordregs[regsp] -= 4))), old_ip);
	if (has_err) {
		cpu_writel_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 4) : (cpu->regs.wordregs[regsp] -= 4))), err);
	}
}

static FUNC_FORCE_INLINE void cpu_copy_call_gate_params(CPU_t* cpu, uint32_t old_ss_base, uint32_t old_esp, int old_stack_is32, uint8_t param_count, int gate32) {
	uint32_t params[31];
	uint32_t param_size = gate32 ? 4 : 2;
	int i;

	for (i = 0; i < param_count; i++) {
		uint32_t offset = old_stack_is32 ? (old_esp + ((uint32_t)i * param_size)) : ((old_esp + ((uint32_t)i * param_size)) & 0xFFFF);
		params[i] = gate32 ? cpu_readl(cpu, old_ss_base + offset) : cpu_readw(cpu, old_ss_base + offset);
	}

	for (i = param_count - 1; i >= 0; i--) {
		if (gate32) {
			pushl(cpu, params[i]);
		}
		else {
			pushw(cpu, (uint16_t)params[i]);
		}
	}
}

static FUNC_FORCE_INLINE void cpu_copy_call_gate_params_sys(CPU_t* cpu, uint32_t old_ss_base, uint32_t old_esp, int old_stack_is32, uint8_t param_count, int gate32) {
	uint32_t params[31];
	uint32_t param_size = gate32 ? 4 : 2;
	int i;

	for (i = 0; i < param_count; i++) {
		uint32_t offset = old_stack_is32 ? (old_esp + ((uint32_t)i * param_size)) : ((old_esp + ((uint32_t)i * param_size)) & 0xFFFF);
		params[i] = gate32 ? cpu_readl_sys(cpu, old_ss_base + offset) : cpu_readw_sys(cpu, old_ss_base + offset);
	}

	for (i = param_count - 1; i >= 0; i--) {
		if (gate32) {
			cpu_writel_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 4) : (cpu->regs.wordregs[regsp] -= 4))), params[i]);
		}
		else {
			cpu_writew_sys(cpu, cpu->segcache[regss] + ((uint32_t)(cpu->segis32[regss] ? (cpu->regs.longregs[regesp] -= 2) : (cpu->regs.wordregs[regsp] -= 2))), (uint16_t)params[i]);
		}
	}
}

static FUNC_FORCE_INLINE void cpu_load_gate_desc(CPU_t* cpu, uint32_t addr, uint8_t access, uint8_t flags, GATEDESC_t* gate) {
	gate->addr = addr;
	gate->access = access;
	gate->flags = flags;
	gate->type = access & 0x0F;
	gate->dpl = (access >> 5) & 3;
	gate->present = access >> 7;
	gate->param_count = cpu_read_sys(cpu, addr + 4) & 0x1F;
	gate->target_selector = cpu_readw_sys(cpu, addr + 2);
	gate->offset = (uint32_t)cpu_readw_sys(cpu, addr);
	if (cpu_gate_is_32bit(gate)) {
		gate->offset |= (uint32_t)cpu_readw_sys(cpu, addr + 6) << 16;
	}
}

static FUNC_FORCE_INLINE int cpu_read_idt_gate(CPU_t* cpu, uint8_t intnum, GATEDESC_t* gate) {
	uint32_t offset = (uint32_t)intnum << 3;

	if ((offset + 7) > cpu->idtl) {
		return 0;
	}

	cpu_load_gate_desc(cpu, cpu->idtr + offset, cpu_read_sys(cpu, cpu->idtr + offset + 5), cpu_read_sys(cpu, cpu->idtr + offset + 6), gate);
	return 1;
}

static FUNC_FORCE_INLINE int cpu_validate_stack_selector(CPU_t* cpu, uint16_t selector, uint8_t target_cpl, uint8_t invalid_vector, uint8_t not_present_vector) {
	SEGDESC_t desc;

	if (cpu_selector_offset(selector) == 0) {
		exception(cpu, invalid_vector, 0);
		return 0;
	}

	if (!cpu_read_segdesc(cpu, selector, &desc)) {
		exception(cpu, invalid_vector, cpu_selector_error_code(selector));
		return 0;
	}

	if (!cpu_desc_is_writable_data(&desc)) {
		exception(cpu, invalid_vector, cpu_selector_error_code(selector));
		return 0;
	}

	if (!cpu_desc_is_present(&desc)) {
		exception(cpu, not_present_vector, cpu_selector_error_code(selector));
		return 0;
	}

	if (((selector & 3) != target_cpl) || (desc.dpl != target_cpl)) {
		exception(cpu, invalid_vector, cpu_selector_error_code(selector));
		return 0;
	}

	return 1;
}

static FUNC_FORCE_INLINE int cpu_read_code_desc(CPU_t* cpu, uint16_t selector, uint8_t not_present_vector, SEGDESC_t* desc) {
	if (cpu_selector_offset(selector) == 0) {
		exception(cpu, 13, 0);
		return 0;
	}

	if (!cpu_read_segdesc(cpu, selector, desc)) {
		exception(cpu, 13, cpu_selector_error_code(selector));
		return 0;
	}

	if (!cpu_desc_is_code(desc)) {
		exception(cpu, 13, cpu_selector_error_code(selector));
		return 0;
	}

	if (!cpu_desc_is_present(desc)) {
		exception(cpu, not_present_vector, cpu_selector_error_code(selector));
		return 0;
	}

	return 1;
}

static FUNC_FORCE_INLINE int cpu_validate_gate_target_code(CPU_t* cpu, uint16_t selector, CODETARGET_t* target) {
	SEGDESC_t desc;

	if (!cpu_read_code_desc(cpu, selector, 11, &desc)) {
		return 0;
	}

	target->conforming = cpu_desc_is_conforming_code(&desc) ? 1 : 0;
	if (target->conforming) {
		if (desc.dpl > cpu->cpl) {
			exception(cpu, 13, cpu_selector_error_code(selector));
			return 0;
		}
		target->target_cpl = cpu->cpl;
		target->outer = 0;
	}
	else {
		if (desc.dpl > cpu->cpl) {
			exception(cpu, 13, cpu_selector_error_code(selector));
			return 0;
		}
		target->target_cpl = desc.dpl;
		target->outer = (desc.dpl < cpu->cpl);
	}

	target->selector = cpu_normalize_selector(selector, target->target_cpl);
	return 1;
}

static FUNC_FORCE_INLINE int cpu_validate_direct_call_target(CPU_t* cpu, uint16_t selector, CODETARGET_t* target) {
	SEGDESC_t desc;
	uint8_t rpl = selector & 3;
	uint8_t epl = (rpl > cpu->cpl) ? rpl : cpu->cpl;

	if (!cpu_read_code_desc(cpu, selector, 11, &desc)) {
		return 0;
	}

	target->conforming = cpu_desc_is_conforming_code(&desc) ? 1 : 0;
	if (target->conforming) {
		if (desc.dpl > epl) {
			exception(cpu, 13, cpu_selector_error_code(selector));
			return 0;
		}
	}
	else {
		if ((desc.dpl != cpu->cpl) || (rpl > cpu->cpl)) {
			exception(cpu, 13, cpu_selector_error_code(selector));
			return 0;
		}
	}

	target->selector = cpu_normalize_selector(selector, cpu->cpl);
	target->target_cpl = cpu->cpl;
	target->outer = 0;
	return 1;
}

static FUNC_FORCE_INLINE int cpu_resolve_task_switch_target(CPU_t* cpu, uint16_t selector, const SEGDESC_t* first_desc, uint16_t* task_selector) {
	SEGDESC_t desc = *first_desc;
	GATEDESC_t gate;
	uint8_t epl = ((selector & 3) > cpu->cpl) ? (selector & 3) : cpu->cpl;
	uint16_t target_selector = selector;

	if (cpu_desc_is_task_gate(&desc)) {
		cpu_load_gate_desc(cpu, desc.addr, desc.access, desc.flags, &gate);
		if (epl > gate.dpl) {
			exception(cpu, 13, cpu_selector_error_code(selector));
			return 0;
		}
		if (!gate.present) {
			exception(cpu, 11, cpu_selector_error_code(selector));
			return 0;
		}

		target_selector = gate.target_selector;
		if (cpu_selector_offset(target_selector) == 0) {
			exception(cpu, 13, 0);
			return 0;
		}
		if (target_selector & 4) {
			exception(cpu, 13, cpu_selector_error_code(target_selector));
			return 0;
		}
		if (!cpu_read_segdesc(cpu, target_selector, &desc)) {
			exception(cpu, 13, cpu_selector_error_code(target_selector));
			return 0;
		}
	}
	else {
		if (target_selector & 4) {
			exception(cpu, 13, cpu_selector_error_code(target_selector));
			return 0;
		}
		if (epl > desc.dpl) {
			exception(cpu, 13, cpu_selector_error_code(selector));
			return 0;
		}
	}

	if (!cpu_desc_is_tss(&desc)) {
		exception(cpu, 13, cpu_selector_error_code(target_selector));
		return 0;
	}
	if (!cpu_desc_is_present(&desc)) {
		exception(cpu, 11, cpu_selector_error_code(target_selector));
		return 0;
	}
	if (cpu_desc_is_busy_tss(&desc)) {
		exception(cpu, 13, cpu_selector_error_code(target_selector));
		return 0;
	}

	*task_selector = target_selector & 0xFFFC;
	return 1;
}

static FUNC_FORCE_INLINE int cpu_fetch_tss_stack(CPU_t* cpu, uint8_t target_cpl, uint16_t* new_ss, uint32_t* new_esp) {
	uint32_t addval;

	if (target_cpl > 2) {
		exception(cpu, 10, cpu->tr_selector);
		return 0;
	}

	addval = (uint32_t)target_cpl << 3;
	if (cpu->trlimit < (10 + addval)) {
		exception(cpu, 10, cpu->tr_selector);
		return 0;
	}

	*new_esp = cpu_readl_sys(cpu, cpu->trbase + 4 + addval);
	*new_ss = cpu_readw_sys(cpu, cpu->trbase + 8 + addval);
	return cpu_validate_stack_selector(cpu, *new_ss, target_cpl, 10, 12);
}

static FUNC_FORCE_INLINE int cpu_validate_return_cs(CPU_t* cpu, uint16_t selector, RETURNCS_t* retinfo) {
	SEGDESC_t desc;
	uint8_t rpl;

	if (cpu_selector_offset(selector) == 0) {
		exception(cpu, 13, 0);
		return 0;
	}

	if (!cpu_read_segdesc(cpu, selector, &desc)) {
		exception(cpu, 13, cpu_selector_error_code(selector));
		return 0;
	}

	if (!cpu_desc_is_code(&desc)) {
		exception(cpu, 13, cpu_selector_error_code(selector));
		return 0;
	}

	if (!cpu_desc_is_present(&desc)) {
		exception(cpu, 11, cpu_selector_error_code(selector));
		return 0;
	}

	rpl = selector & 3;
	if (cpu_desc_is_conforming_code(&desc)) {
		if ((rpl != cpu->cpl) || (desc.dpl > cpu->cpl)) {
			exception(cpu, 13, cpu_selector_error_code(selector));
			return 0;
		}
		retinfo->target_cpl = cpu->cpl;
		retinfo->outer = 0;
	}
	else {
		if (rpl < cpu->cpl) {
			exception(cpu, 13, cpu_selector_error_code(selector));
			return 0;
		}
		if (desc.dpl != rpl) {
			exception(cpu, 13, cpu_selector_error_code(selector));
			return 0;
		}
		retinfo->target_cpl = rpl;
		retinfo->outer = (rpl > cpu->cpl);
	}

	retinfo->selector = selector;
	return 1;
}

static FUNC_FORCE_INLINE int cpu_validate_return_ss(CPU_t* cpu, uint16_t selector, uint8_t target_cpl) {
	return cpu_validate_stack_selector(cpu, selector, target_cpl, 13, 12);
}

static FUNC_FORCE_INLINE void cpu_restore_iret_flags(CPU_t* cpu, uint32_t new_eflags, uint8_t current_cpl, uint8_t target_cpl) {
	uint32_t old_eflags = makeflagsword(cpu);
	uint32_t merged_flags = old_eflags;
	uint32_t modifiable_mask = EFLAGS_CF | EFLAGS_PF | EFLAGS_AF | EFLAGS_ZF | EFLAGS_SF | EFLAGS_TF | EFLAGS_DF | EFLAGS_OF | EFLAGS_NT;

	if (cpu->isoper32) {
		modifiable_mask |= (1u << 18) | (1u << 21);
	}
	else {
		new_eflags = (new_eflags & 0xFFFFu) | (old_eflags & 0xFFFF0000u);
	}

	merged_flags = (merged_flags & ~modifiable_mask) | (new_eflags & modifiable_mask);

	if (!cpu->protected_mode || (current_cpl <= cpu->iopl)) {
		merged_flags = (merged_flags & ~EFLAGS_IF) | (new_eflags & EFLAGS_IF);
	}
	if (!cpu->protected_mode || (current_cpl == 0)) {
		merged_flags = (merged_flags & ~EFLAGS_IOPL) | (new_eflags & EFLAGS_IOPL);
	}

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
	cpu->rf = (merged_flags & EFLAGS_RF) ? 1 : 0;
	cpu->v86f = (merged_flags & EFLAGS_VM) ? 1 : 0;
	cpu->acf = (merged_flags & (1u << 18)) ? 1 : 0;
	cpu->idf = (merged_flags & (1u << 21)) ? 1 : 0;
	cpu->cpl = cpu->v86f ? 3 : target_cpl;
}

static FUNC_FORCE_INLINE void cpu_null_seg(CPU_t* cpu, uint8_t reg) {
	cpu->segregs[reg] = 0;
	cpu->segcache[reg] = 0;
	cpu->segis32[reg] = 0;
	cpu->seglimit[reg] = 0;
}

static FUNC_FORCE_INLINE int cpu_segment_usable_at_cpl(CPU_t* cpu, uint16_t selector, uint8_t target_cpl) {
	SEGDESC_t desc;
	uint8_t rpl;

	if (cpu_selector_offset(selector) == 0) {
		return 1;
	}

	if (!cpu_read_segdesc(cpu, selector, &desc)) {
		return 0;
	}

	if (!cpu_desc_is_present(&desc) || ((desc.access & 0x10) == 0)) {
		return 0;
	}

	rpl = selector & 3;
	if (cpu_desc_is_code(&desc)) {
		if (!cpu_desc_is_readable_code(&desc)) {
			return 0;
		}
		if (!cpu_desc_is_conforming_code(&desc) && ((desc.dpl < target_cpl) || (desc.dpl < rpl))) {
			return 0;
		}
		return 1;
	}

	return (desc.dpl >= target_cpl) && (desc.dpl >= rpl);
}

static FUNC_FORCE_INLINE void cpu_cleanup_outer_return_segments(CPU_t* cpu, uint8_t target_cpl) {
	if (!cpu_segment_usable_at_cpl(cpu, cpu->segregs[reges], target_cpl)) cpu_null_seg(cpu, reges);
	if (!cpu_segment_usable_at_cpl(cpu, cpu->segregs[regds], target_cpl)) cpu_null_seg(cpu, regds);
	if (!cpu_segment_usable_at_cpl(cpu, cpu->segregs[regfs], target_cpl)) cpu_null_seg(cpu, regfs);
	if (!cpu_segment_usable_at_cpl(cpu, cpu->segregs[reggs], target_cpl)) cpu_null_seg(cpu, reggs);
}

static FUNC_FORCE_INLINE void cpu_commit_segdesc(CPU_t* cpu, uint8_t reg, uint16_t selector, const SEGDESC_t* desc) {
	uint32_t base;

	base = cpu_read_sys(cpu, desc->addr + 2) | ((uint32_t)cpu_read_sys(cpu, desc->addr + 3) << 8);
	base |= ((uint32_t)cpu_read_sys(cpu, desc->addr + 4) << 16);
	base |= ((uint32_t)cpu_read_sys(cpu, desc->addr + 7) << 24);

	cpu->segregs[reg] = selector;
	cpu->segcache[reg] = base;
	cpu->segis32[reg] = (desc->flags >> 6) & 1;
	cpu->seglimit[reg] = (uint32_t)cpu_readw_sys(cpu, desc->addr) | ((uint32_t)(desc->flags & 0x0F) << 16);
	if (showops) debug_log(DEBUG_DETAIL, "Segment %04X is %s\n", selector >> 3, cpu->segis32[reg] ? "32-bit" : "16-bit");
	if (desc->flags & 0x80) {
		cpu->seglimit[reg] <<= 12;
		cpu->seglimit[reg] |= 0xFFF;
	}

	if ((reg == regcs) && cpu->protected_mode) {
		if (!cpu->v86f) {
			cpu->cpl = selector & 0x03;
		}
		else {
			cpu->cpl = 3;
		}
	}
}

static FUNC_FORCE_INLINE void cpu_commit_real_segment(CPU_t* cpu, uint8_t reg, uint16_t selector) {
	cpu->segregs[reg] = selector;
	cpu->segcache[reg] = (uint32_t)selector << 4;
	cpu->segis32[reg] = 0;
}

static FUNC_FORCE_INLINE int cpu_commit_validated_segment(CPU_t* cpu, uint8_t reg, uint16_t selector) {
	SEGDESC_t desc;

	if (!cpu->protected_mode || cpu->v86f) {
		cpu_commit_real_segment(cpu, reg, selector);
		return 1;
	}

	if ((cpu_selector_offset(selector) == 0) && (reg != regcs) && (reg != regss)) {
		cpu_null_seg(cpu, reg);
		return 1;
	}

	if (!cpu_read_segdesc(cpu, selector, &desc)) {
		return 0;
	}

	cpu_commit_segdesc(cpu, reg, selector, &desc);
	return 1;
}

//translate protected mode segments/selectors into linear addresses from descriptor table
static FUNC_FORCE_INLINE uint32_t segtolinear(CPU_t* cpu, uint16_t seg) {
	uint32_t addr, gdtidx;

	if (!cpu->protected_mode || cpu->v86f) return (uint32_t)seg << 4;

	gdtidx = ((seg & 4) ? cpu->ldtr : cpu->gdtr) + ((uint32_t)seg & ~7);
	addr = cpu_read_sys(cpu, gdtidx + 2) | ((uint32_t)cpu_read_sys(cpu, gdtidx + 3) << 8);  // Base Low (16-bit)
	addr |= ((uint32_t)cpu_read_sys(cpu, gdtidx + 4) << 16);  // Base Mid (8-bit)
	addr |= ((uint32_t)cpu_read_sys(cpu, gdtidx + 7) << 24);  // Base High (8-bit)
	//if (cpu->isoper32) printf("Entered 32-bit segment\n");
	//printf("segtolinear %04X = %08X\n", seg, addr);
	return addr;
}

static void task_switch(CPU_t* cpu, uint16_t new_tss_selector, int reason) {
	int is_task_gate = (reason == TASK_SWITCH_REASON_GATE);
	int is_task_return = (reason == TASK_SWITCH_REASON_IRET);
	int is_task_jump = (reason == TASK_SWITCH_REASON_JMP);
	int is_nested_switch = (reason == TASK_SWITCH_REASON_CALL) || is_task_gate;
	uint32_t new_desc_addr = cpu->gdtr + (new_tss_selector & ~7);
	uint8_t access = cpu_read_sys(cpu, new_desc_addr + 5);
	uint8_t type = access & 0x0F;
	uint8_t present = access >> 7;

	if (!present || (type != 0x9 && type != 0xB)) {
		exception(cpu, 10, new_tss_selector); // Invalid TSS
		return;
	}

	// Get base and limit of new TSS
	uint32_t base = cpu_read_sys(cpu, new_desc_addr + 2) |
		(cpu_read_sys(cpu, new_desc_addr + 3) << 8) |
		(cpu_read_sys(cpu, new_desc_addr + 4) << 16) |
		(cpu_read_sys(cpu, new_desc_addr + 7) << 24);
	uint32_t limit = cpu_read_sys(cpu, new_desc_addr) |
		(cpu_read_sys(cpu, new_desc_addr + 1) << 8) |
		((cpu_read_sys(cpu, new_desc_addr + 6) & 0x0F) << 16);

	if (!is_task_return) {
		// Save current task into its TSS (only if this is a task gate or CALL/JMP, not IRET)
		uint32_t old_tss_base = cpu->trbase;
		cpu_writel_sys(cpu, old_tss_base + 0x20, cpu->ip);
		cpu_writel_sys(cpu, old_tss_base + 0x24, makeflagsword(cpu));
		cpu_writel_sys(cpu, old_tss_base + 0x28, cpu->regs.longregs[regeax]);
		cpu_writel_sys(cpu, old_tss_base + 0x2C, cpu->regs.longregs[regecx]);
		cpu_writel_sys(cpu, old_tss_base + 0x30, cpu->regs.longregs[regedx]);
		cpu_writel_sys(cpu, old_tss_base + 0x34, cpu->regs.longregs[regebx]);
		cpu_writel_sys(cpu, old_tss_base + 0x38, cpu->regs.longregs[regesp]);
		cpu_writel_sys(cpu, old_tss_base + 0x3C, cpu->regs.longregs[regebp]);
		cpu_writel_sys(cpu, old_tss_base + 0x40, cpu->regs.longregs[regesi]);
		cpu_writel_sys(cpu, old_tss_base + 0x44, cpu->regs.longregs[regedi]);
		cpu_writew_sys(cpu, old_tss_base + 0x00, cpu->segregs[reges]);
		cpu_writew_sys(cpu, old_tss_base + 0x02, cpu->segregs[regcs]);
		cpu_writew_sys(cpu, old_tss_base + 0x04, cpu->segregs[regss]);
		cpu_writew_sys(cpu, old_tss_base + 0x06, cpu->segregs[regds]);
		cpu_writew_sys(cpu, old_tss_base + 0x08, cpu->segregs[regfs]);
		cpu_writew_sys(cpu, old_tss_base + 0x0A, cpu->segregs[reggs]);
		cpu_writew_sys(cpu, old_tss_base + 0x5C, cpu->ldtr); // LDT
		cpu_writel_sys(cpu, old_tss_base + 0x1C, cpu->cr[3]);

		uint8_t old_access = cpu_read_sys(cpu, cpu->gdtr + (cpu->tr_selector & ~7) + 5);
		if (is_nested_switch && ((old_access & 0x0F) == 0x9)) {
			cpu_write_sys(cpu, cpu->gdtr + (cpu->tr_selector & ~7) + 5, old_access | 0x2);
		}
		else if (is_task_jump && ((old_access & 0x0F) == 0xB)) {
			cpu_write_sys(cpu, cpu->gdtr + (cpu->tr_selector & ~7) + 5, old_access & (uint8_t)~0x2);
		}
	}
	else {
		uint8_t old_access = cpu_read_sys(cpu, cpu->gdtr + (cpu->tr_selector & ~7) + 5);
		if ((old_access & 0x0F) == 0xB) {
			cpu_write_sys(cpu, cpu->gdtr + (cpu->tr_selector & ~7) + 5, old_access & (uint8_t)~0x2);
		}
	}

	if (!is_task_return && type == 0x9) {
		//Set busy on new TSS
		cpu_write_sys(cpu, new_desc_addr + 5, access | 0x2);
	}

	if (is_nested_switch) {
		cpu_writew_sys(cpu, base + 0x00, cpu->tr_selector);
	}

	// Load new task
	cpu->ip = cpu_readl_sys(cpu, base + 0x20);
	uint32_t new_eflags = cpu_readl_sys(cpu, base + 0x24);
	cpu->regs.longregs[regeax] = cpu_readl_sys(cpu, base + 0x28);
	cpu->regs.longregs[regecx] = cpu_readl_sys(cpu, base + 0x2C);
	cpu->regs.longregs[regedx] = cpu_readl_sys(cpu, base + 0x30);
	cpu->regs.longregs[regebx] = cpu_readl_sys(cpu, base + 0x34);
	cpu->regs.longregs[regesp] = cpu_readl_sys(cpu, base + 0x38);
	cpu->regs.longregs[regebp] = cpu_readl_sys(cpu, base + 0x3C);
	cpu->regs.longregs[regesi] = cpu_readl_sys(cpu, base + 0x40);
	cpu->regs.longregs[regedi] = cpu_readl_sys(cpu, base + 0x44);
	putsegreg(cpu, regcs, cpu_readw_sys(cpu, base + 0x02));
	if (cpu->doexception) return;
	putsegreg(cpu, regss, cpu_readw_sys(cpu, base + 0x04));
	if (cpu->doexception) return;
	putsegreg(cpu, regds, cpu_readw_sys(cpu, base + 0x06));
	if (cpu->doexception) return;
	putsegreg(cpu, reges, cpu_readw_sys(cpu, base + 0x00));
	if (cpu->doexception) return;
	putsegreg(cpu, regfs, cpu_readw_sys(cpu, base + 0x08));
	if (cpu->doexception) return;
	putsegreg(cpu, reggs, cpu_readw_sys(cpu, base + 0x0A));
	if (cpu->doexception) return;
	cpu->ldtr = cpu_readw_sys(cpu, base + 0x5C);
	cpu->cr[3] = cpu_readl_sys(cpu, base + 0x1C);
	memory_tlb_flush(cpu);

	// Save TSS selector and base
	cpu->tr_selector = new_tss_selector;
	cpu->trbase = base;

	// Update EFLAGS
	decodeflagsword(cpu, new_eflags);

	// Set NT flag
	if (is_nested_switch) {
		cpu->nt = 1;
	}
	else if (is_task_return || is_task_jump) {
		cpu->nt = 0;
	}
}

// IO read/writes check permission in pmode as well as V86
static FUNC_FORCE_INLINE int check_io_permission(CPU_t* cpu, uint16_t port, int size) {
	if (cpu->v86f || (cpu->protected_mode && cpu->cpl > cpu->iopl)) {
		uint16_t io_map_base = cpu_readw_sys(cpu, cpu->trbase + 102);
		for (int i = 0; i < (size / 8); i++) {
			uint16_t cur_port = port + i;
			if ((io_map_base + (cur_port >> 3)) >= cpu->trlimit) {
				exception(cpu, 13, 0);
				return 0;
			}
			if (cpu_read_sys(cpu, cpu->trbase + io_map_base + (cur_port >> 3)) & (1 << (cur_port & 7))) {
				exception(cpu, 13, 0);
				return 0;
			}
		}
	}
	return 1;
}

static void modregrm(CPU_t* cpu) {
	cpu->addrbyte = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
	StepIP(cpu, 1);
	cpu->mode = cpu->addrbyte >> 6;
	cpu->reg = (cpu->addrbyte >> 3) & 7;
	cpu->rm = cpu->addrbyte & 7;

	if (!cpu->isaddr32) {
		switch (cpu->mode) {
		case 0:
			if (cpu->rm == 6) {
				cpu->disp16 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
				StepIP(cpu, 2);
			}
			if (((cpu->rm == 2) || (cpu->rm == 3)) && !cpu->segoverride) {
				cpu->useseg = cpu->segcache[regss];
			}
			break;

		case 1:
			cpu->disp16 = signext(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
			StepIP(cpu, 1);
			if (((cpu->rm == 2) || (cpu->rm == 3) || (cpu->rm == 6)) && !cpu->segoverride) {
				cpu->useseg = cpu->segcache[regss];
			}
			break;

		case 2:
			cpu->disp16 = getmem16(cpu, cpu->segcache[regcs], cpu->ip);
			StepIP(cpu, 2);
			if (((cpu->rm == 2) || (cpu->rm == 3) || (cpu->rm == 6)) && !cpu->segoverride) {
				cpu->useseg = cpu->segcache[regss];
			}
			break;

		default:
			cpu->disp8 = 0;
			cpu->disp16 = 0;
		}
	}
	else { //32-bit addressing
		//printf("modr/m 32-bit\n");
		cpu->sib_val = 0;
		if ((cpu->mode < 3) && (cpu->rm == 4)) { // SIB byte present
			cpu->sib = getmem8(cpu, cpu->segcache[regcs], cpu->ip);
			StepIP(cpu, 1);
			cpu->sib_scale = (cpu->sib >> 6) & 3;
			cpu->sib_index = (cpu->sib >> 3) & 7;
			cpu->sib_base = cpu->sib & 7;
		}

		if (!cpu->segoverride && cpu->mode < 3) {
			if (cpu->rm == 4) { // SIB
				if (cpu->sib_base == regesp || (cpu->sib_base == regebp && cpu->mode > 0)) {
					cpu->useseg = cpu->segcache[regss];
				}
			}
			else if (cpu->rm == 5 && cpu->mode > 0) {
				cpu->useseg = cpu->segcache[regss];
			}
		}


		switch (cpu->mode) {
		case 0:
			if ((cpu->rm == 5) || ((cpu->rm == 4) && (cpu->sib_base == 5))) {
				cpu->disp32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
				StepIP(cpu, 4);
			}
			else {
				cpu->disp32 = 0;
			}
			break;

		case 1:
			cpu->disp32 = signext8to32(getmem8(cpu, cpu->segcache[regcs], cpu->ip));
			StepIP(cpu, 1);
			break;

		case 2:
			cpu->disp32 = getmem32(cpu, cpu->segcache[regcs], cpu->ip);
			StepIP(cpu, 4);
			break;

		default:
			cpu->disp32 = 0;
		}

		if ((cpu->mode < 3) && (cpu->rm == 4)) { // SIB byte present
			uint32_t index, base;
			if (cpu->sib_index == regesp) { //ESP index actually means NO index
				index = 0;
			}
			else {
				index = cpu->regs.longregs[cpu->sib_index] << (uint32_t)cpu->sib_scale;
			}
			if (cpu->mode == 0 && cpu->sib_base == regebp) { //if base is EBP and mode is 0, there is actually NO base register
				base = 0;
			}
			else {
				base = (cpu->sib_base == regesp) ? cpu->shadow_esp : cpu->regs.longregs[cpu->sib_base];
				if (!cpu->segoverride && cpu->sib_base == 4) cpu->useseg = cpu->segcache[regss];
			}
			cpu->sib_val = base + index;
		}

	}
}

static FUNC_FORCE_INLINE uint16_t getreg16(CPU_t* cpu, uint8_t reg) {
	switch (reg) {
	case 0: return cpu->regs.wordregs[regax];
	case 1: return cpu->regs.wordregs[regcx];
	case 2: return cpu->regs.wordregs[regdx];
	case 3: return cpu->regs.wordregs[regbx];
	case 4: return cpu->regs.wordregs[regsp];
	case 5: return cpu->regs.wordregs[regbp];
	case 6: return cpu->regs.wordregs[regsi];
	case 7: return cpu->regs.wordregs[regdi];
	}
}

static FUNC_FORCE_INLINE uint32_t getreg32(CPU_t* cpu, uint8_t reg) {
	switch (reg) {
	case 0: return cpu->regs.longregs[regeax];
	case 1: return cpu->regs.longregs[regecx];
	case 2: return cpu->regs.longregs[regedx];
	case 3: return cpu->regs.longregs[regebx];
	case 4: return cpu->regs.longregs[regesp];
	case 5: return cpu->regs.longregs[regebp];
	case 6: return cpu->regs.longregs[regesi];
	case 7: return cpu->regs.longregs[regedi];
	}
}

static FUNC_FORCE_INLINE void putreg16(CPU_t* cpu, uint8_t reg, uint16_t writeval) {
	switch (reg) {
	case 0: cpu->regs.wordregs[regax] = writeval; break;
	case 1: cpu->regs.wordregs[regcx] = writeval; break;
	case 2: cpu->regs.wordregs[regdx] = writeval; break;
	case 3: cpu->regs.wordregs[regbx] = writeval; break;
	case 4: cpu->regs.wordregs[regsp] = writeval; break;
	case 5: cpu->regs.wordregs[regbp] = writeval; break;
	case 6: cpu->regs.wordregs[regsi] = writeval; break;
	case 7: cpu->regs.wordregs[regdi] = writeval; break;
	}
}

static FUNC_FORCE_INLINE void putreg32(CPU_t* cpu, uint8_t reg, uint32_t writeval) {
	switch (reg) {
	case 0: cpu->regs.longregs[regeax] = writeval; break;
	case 1: cpu->regs.longregs[regecx] = writeval; break;
	case 2: cpu->regs.longregs[regedx] = writeval; break;
	case 3: cpu->regs.longregs[regebx] = writeval; break;
	case 4: cpu->regs.longregs[regesp] = writeval; break;
	case 5: cpu->regs.longregs[regebp] = writeval; break;
	case 6: cpu->regs.longregs[regesi] = writeval; break;
	case 7: cpu->regs.longregs[regedi] = writeval; break;
	}
}

static FUNC_FORCE_INLINE uint32_t getsegreg(CPU_t* cpu, uint8_t reg) {
	switch (reg) {
	case 0: return cpu->segregs[reges];
	case 1: return cpu->segregs[regcs];
	case 2: return cpu->segregs[regss];
	case 3: return cpu->segregs[regds];
	case 4: return cpu->segregs[regfs];
	case 5: return cpu->segregs[reggs];
	}
}

static FUNC_FORCE_INLINE void putsegreg(CPU_t* cpu, uint8_t reg, uint32_t writeval) {
	SEGDESC_t desc;
	uint16_t selector = (uint16_t)writeval;

	if (!cpu->protected_mode || cpu->v86f) {
		cpu_commit_real_segment(cpu, reg, selector);
		if ((reg == regcs) && cpu->protected_mode) {
			cpu->cpl = cpu->v86f ? 3 : (selector & 0x03);
		}
		return;
	}

	if (cpu_selector_offset(selector) == 0) {
		if ((reg == regcs) || (reg == regss)) {
			exception(cpu, 13, 0);
			return;
		}
		cpu_null_seg(cpu, reg);
		return;
	}

	if (!cpu_read_segdesc(cpu, selector, &desc)) {
		exception(cpu, 13, cpu_selector_error_code(selector));
		return;
	}

	if (!cpu_desc_is_present(&desc)) {
		exception(cpu, (reg == regss) ? 12 : 11, cpu_selector_error_code(selector));
		return;
	}

	switch (reg) {
	case regcs:
		if (!cpu_desc_is_code(&desc)) {
			exception(cpu, 13, cpu_selector_error_code(selector));
			return;
		}
		break;

	case regss:
		if (!cpu_desc_is_writable_data(&desc) || ((selector & 3) != cpu->cpl) || (desc.dpl != cpu->cpl)) {
			exception(cpu, 13, cpu_selector_error_code(selector));
			return;
		}
		break;

	default:
		if (!cpu_segment_usable_at_cpl(cpu, selector, cpu->cpl)) {
			exception(cpu, 13, cpu_selector_error_code(selector));
			return;
		}
		break;
	}

	cpu_commit_segdesc(cpu, reg, selector, &desc);
}

static FUNC_FORCE_INLINE void flag_szp8(CPU_t* cpu, uint8_t value) {
	if (!value) {
		cpu->zf = 1;
	}
	else {
		cpu->zf = 0;	/* set or clear zero flag */
	}

	if (value & 0x80) {
		cpu->sf = 1;
	}
	else {
		cpu->sf = 0;	/* set or clear sign flag */
	}

	cpu->pf = parity[value]; /* retrieve parity state from lookup table */
}

static FUNC_FORCE_INLINE void flag_szp16(CPU_t* cpu, uint16_t value) {
	if (!value) {
		cpu->zf = 1;
	}
	else {
		cpu->zf = 0;	/* set or clear zero flag */
	}

	if (value & 0x8000) {
		cpu->sf = 1;
	}
	else {
		cpu->sf = 0;	/* set or clear sign flag */
	}

	cpu->pf = parity[value & 255];	/* retrieve parity state from lookup table */
}

static FUNC_FORCE_INLINE void flag_szp32(CPU_t* cpu, uint32_t value) {
	if (!value) {
		cpu->zf = 1;
	}
	else {
		cpu->zf = 0;	/* set or clear zero flag */
	}

	if (value & 0x80000000) {
		cpu->sf = 1;
	}
	else {
		cpu->sf = 0;	/* set or clear sign flag */
	}

	cpu->pf = parity[value & 255];	/* retrieve parity state from lookup table */
}

static FUNC_FORCE_INLINE void flag_log8(CPU_t* cpu, uint8_t value) {
	flag_szp8(cpu, value);
	cpu->cf = 0;
	cpu->of = 0; /* bitwise logic ops always clear carry and overflow */
}

static FUNC_FORCE_INLINE void flag_log16(CPU_t* cpu, uint16_t value) {
	flag_szp16(cpu, value);
	cpu->cf = 0;
	cpu->of = 0; /* bitwise logic ops always clear carry and overflow */
}

static FUNC_FORCE_INLINE void flag_log32(CPU_t* cpu, uint32_t value) {
	flag_szp32(cpu, value);
	cpu->cf = 0;
	cpu->of = 0; /* bitwise logic ops always clear carry and overflow */
}

static FUNC_FORCE_INLINE void flag_adc8(CPU_t* cpu, uint8_t v1, uint8_t v2, uint8_t v3) {

	/* v1 = destination operand, v2 = source operand, v3 = carry flag */
	uint16_t	dst;

	dst = (uint16_t)v1 + (uint16_t)v2 + (uint16_t)v3;
	flag_szp8(cpu, (uint8_t)dst);
	if (((dst ^ v1) & (dst ^ v2) & 0x80) == 0x80) {
		cpu->of = 1;
	}
	else {
		cpu->of = 0; /* set or clear overflow flag */
	}

	if (dst & 0xFF00) {
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0; /* set or clear carry flag */
	}

	if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
		cpu->af = 1;
	}
	else {
		cpu->af = 0; /* set or clear auxilliary flag */
	}
}

static FUNC_FORCE_INLINE void flag_adc16(CPU_t* cpu, uint16_t v1, uint16_t v2, uint16_t v3) {

	uint32_t	dst;

	dst = (uint32_t)v1 + (uint32_t)v2 + (uint32_t)v3;
	flag_szp16(cpu, (uint16_t)dst);
	if ((((dst ^ v1) & (dst ^ v2)) & 0x8000) == 0x8000) {
		cpu->of = 1;
	}
	else {
		cpu->of = 0;
	}

	if (dst & 0xFFFF0000) {
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0;
	}

	if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
		cpu->af = 1;
	}
	else {
		cpu->af = 0;
	}
}

static FUNC_FORCE_INLINE void flag_adc32(CPU_t* cpu, uint32_t v1, uint32_t v2, uint32_t v3) {

	uint64_t	dst;

	dst = (uint64_t)v1 + (uint64_t)v2 + (uint64_t)v3;
	flag_szp32(cpu, (uint32_t)dst);
	if ((((dst ^ v1) & (dst ^ v2)) & 0x80000000) == 0x80000000) {
		cpu->of = 1;
	}
	else {
		cpu->of = 0;
	}

	if (dst & 0xFFFFFFFF00000000) {
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0;
	}

	if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
		cpu->af = 1;
	}
	else {
		cpu->af = 0;
	}
}

static FUNC_FORCE_INLINE void flag_add8(CPU_t* cpu, uint8_t v1, uint8_t v2) {
	/* v1 = destination operand, v2 = source operand */
	uint16_t	dst;

	dst = (uint16_t)v1 + (uint16_t)v2;
	flag_szp8(cpu, (uint8_t)dst);
	if (dst & 0xFF00) {
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0;
	}

	if (((dst ^ v1) & (dst ^ v2) & 0x80) == 0x80) {
		cpu->of = 1;
	}
	else {
		cpu->of = 0;
	}

	if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
		cpu->af = 1;
	}
	else {
		cpu->af = 0;
	}
}

static FUNC_FORCE_INLINE void flag_add16(CPU_t* cpu, uint16_t v1, uint16_t v2) {
	/* v1 = destination operand, v2 = source operand */
	uint32_t	dst;

	dst = (uint32_t)v1 + (uint32_t)v2;
	flag_szp16(cpu, (uint16_t)dst);
	if (dst & 0xFFFF0000) {
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0;
	}

	if (((dst ^ v1) & (dst ^ v2) & 0x8000) == 0x8000) {
		cpu->of = 1;
	}
	else {
		cpu->of = 0;
	}

	if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
		cpu->af = 1;
	}
	else {
		cpu->af = 0;
	}
}

static FUNC_FORCE_INLINE void flag_add32(CPU_t* cpu, uint32_t v1, uint32_t v2) {
	/* v1 = destination operand, v2 = source operand */
	uint64_t	dst;

	dst = (uint64_t)v1 + (uint64_t)v2;
	flag_szp32(cpu, (uint32_t)dst);
	if (dst & 0xFFFFFFFF00000000) {
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0;
	}

	if (((dst ^ v1) & (dst ^ v2) & 0x80000000) == 0x80000000) {
		cpu->of = 1;
	}
	else {
		cpu->of = 0;
	}

	if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
		cpu->af = 1;
	}
	else {
		cpu->af = 0;
	}
}

static FUNC_FORCE_INLINE void flag_sbb8(CPU_t* cpu, uint8_t v1, uint8_t v2, uint8_t v3) {

	/* v1 = destination operand, v2 = source operand, v3 = carry flag */
	uint16_t	src_with_cf;
	uint16_t	dst;

	src_with_cf = (uint16_t)v2 + (uint16_t)v3;
	dst = (uint16_t)v1 - src_with_cf;
	flag_szp8(cpu, (uint8_t)dst);
	if (dst & 0xFF00) {
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0;
	}

	if ((dst ^ v1) & (v1 ^ v2) & 0x80) {
		cpu->of = 1;
	}
	else {
		cpu->of = 0;
	}

	if ((v1 ^ v2 ^ dst) & 0x10) {
		cpu->af = 1;
	}
	else {
		cpu->af = 0;
	}
}

static FUNC_FORCE_INLINE void flag_sbb16(CPU_t* cpu, uint16_t v1, uint16_t v2, uint16_t v3) {

	/* v1 = destination operand, v2 = source operand, v3 = carry flag */
	uint32_t	src_with_cf;
	uint32_t	dst;

	src_with_cf = (uint32_t)v2 + (uint32_t)v3;
	dst = (uint32_t)v1 - src_with_cf;
	flag_szp16(cpu, (uint16_t)dst);
	if (dst & 0xFFFF0000) {
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0;
	}

	if ((dst ^ v1) & (v1 ^ v2) & 0x8000) {
		cpu->of = 1;
	}
	else {
		cpu->of = 0;
	}

	if ((v1 ^ v2 ^ dst) & 0x10) {
		cpu->af = 1;
	}
	else {
		cpu->af = 0;
	}
}

static FUNC_FORCE_INLINE void flag_sbb32(CPU_t* cpu, uint32_t v1, uint32_t v2, uint32_t v3) {

	/* v1 = destination operand, v2 = source operand, v3 = carry flag */
	uint64_t	src_with_cf;
	uint64_t	dst;

	src_with_cf = (uint64_t)v2 + (uint64_t)v3;
	dst = (uint64_t)v1 - src_with_cf;
	flag_szp32(cpu, (uint32_t)dst);
	if (dst & 0xFFFFFFFF00000000) {
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0;
	}

	if ((dst ^ v1) & (v1 ^ v2) & 0x80000000) {
		cpu->of = 1;
	}
	else {
		cpu->of = 0;
	}

	if ((v1 ^ v2 ^ dst) & 0x10) {
		cpu->af = 1;
	}
	else {
		cpu->af = 0;
	}
}

static FUNC_FORCE_INLINE void flag_sub8(CPU_t* cpu, uint8_t v1, uint8_t v2) {

	/* v1 = destination operand, v2 = source operand */
	uint16_t	dst;

	dst = (uint16_t)v1 - (uint16_t)v2;
	flag_szp8(cpu, (uint8_t)dst);
	if (dst & 0xFF00) {
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0;
	}

	if ((dst ^ v1) & (v1 ^ v2) & 0x80) {
		cpu->of = 1;
	}
	else {
		cpu->of = 0;
	}

	if ((v1 ^ v2 ^ dst) & 0x10) {
		cpu->af = 1;
	}
	else {
		cpu->af = 0;
	}
}

static FUNC_FORCE_INLINE void flag_sub16(CPU_t* cpu, uint16_t v1, uint16_t v2) {

	/* v1 = destination operand, v2 = source operand */
	uint32_t	dst;

	dst = (uint32_t)v1 - (uint32_t)v2;
	flag_szp16(cpu, (uint16_t)dst);
	if (dst & 0xFFFF0000) {
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0;
	}

	if ((dst ^ v1) & (v1 ^ v2) & 0x8000) {
		cpu->of = 1;
	}
	else {
		cpu->of = 0;
	}

	if ((v1 ^ v2 ^ dst) & 0x10) {
		cpu->af = 1;
	}
	else {
		cpu->af = 0;
	}
}

static FUNC_FORCE_INLINE void flag_sub32(CPU_t* cpu, uint32_t v1, uint32_t v2) {

	/* v1 = destination operand, v2 = source operand */
	uint64_t	dst;

	dst = (uint64_t)v1 - (uint64_t)v2;
	flag_szp32(cpu, (uint32_t)dst);
	if (dst & 0xFFFFFFFF00000000) {
		cpu->cf = 1;
	}
	else {
		cpu->cf = 0;
	}

	if ((dst ^ v1) & (v1 ^ v2) & 0x80000000) {
		cpu->of = 1;
	}
	else {
		cpu->of = 0;
	}

	if ((v1 ^ v2 ^ dst) & 0x10) {
		cpu->af = 1;
	}
	else {
		cpu->af = 0;
	}
}

static FUNC_FORCE_INLINE uint32_t cpu_effective_offset(CPU_t* cpu, uint8_t rmval) {
	uint32_t tempea = 0;

	if (!cpu->isaddr32) {
		switch (cpu->mode) {
		case 0:
			switch (rmval) {
			case 0:
				tempea = cpu->regs.wordregs[regbx] + cpu->regs.wordregs[regsi];
				break;
			case 1:
				tempea = cpu->regs.wordregs[regbx] + cpu->regs.wordregs[regdi];
				break;
			case 2:
				tempea = cpu->regs.wordregs[regbp] + cpu->regs.wordregs[regsi];
				break;
			case 3:
				tempea = cpu->regs.wordregs[regbp] + cpu->regs.wordregs[regdi];
				break;
			case 4:
				tempea = cpu->regs.wordregs[regsi];
				break;
			case 5:
				tempea = cpu->regs.wordregs[regdi];
				break;
			case 6:
				tempea = cpu->disp16; // (int32_t)(int16_t)cpu->disp16;
				break;
			case 7:
				tempea = cpu->regs.wordregs[regbx];
				break;
			}
			break;

		case 1:
		case 2:
			switch (rmval) {
			case 0:
				tempea = cpu->regs.wordregs[regbx] + cpu->regs.wordregs[regsi] + cpu->disp16;
				break;
			case 1:
				tempea = cpu->regs.wordregs[regbx] + cpu->regs.wordregs[regdi] + cpu->disp16;
				break;
			case 2:
				tempea = cpu->regs.wordregs[regbp] + cpu->regs.wordregs[regsi] + cpu->disp16;
				break;
			case 3:
				tempea = cpu->regs.wordregs[regbp] + cpu->regs.wordregs[regdi] + cpu->disp16;
				break;
			case 4:
				tempea = cpu->regs.wordregs[regsi] + cpu->disp16;
				break;
			case 5:
				tempea = cpu->regs.wordregs[regdi] + cpu->disp16;
				break;
			case 6:
				tempea = cpu->regs.wordregs[regbp] + cpu->disp16;
				break;
			case 7:
				tempea = cpu->regs.wordregs[regbx] + cpu->disp16;
				break;
			}
			break;
		}
	}
	else { //32-bit addressing
		switch (cpu->mode) {
		case 0:
			switch (rmval) {
			case 0:
				tempea = cpu->regs.longregs[regeax];
				break;
			case 1:
				tempea = cpu->regs.longregs[regecx];
				break;
			case 2:
				tempea = cpu->regs.longregs[regedx];
				break;
			case 3:
				tempea = cpu->regs.longregs[regebx];
				break;
			case 4:
				tempea = cpu->sib_val;
				if (cpu->sib_base == 5) tempea += cpu->disp32;
				break;
			case 5:
				tempea = cpu->disp32;
				break;
			case 6:
				tempea = cpu->regs.longregs[regesi];
				break;
			case 7:
				tempea = cpu->regs.longregs[regedi];
				break;
			}
			break;

		case 1:
		case 2:
			switch (rmval) {
			case 0:
				tempea = cpu->regs.longregs[regeax] + cpu->disp32;
				break;
			case 1:
				tempea = cpu->regs.longregs[regecx] + cpu->disp32;
				break;
			case 2:
				tempea = cpu->regs.longregs[regedx] + cpu->disp32;
				break;
			case 3:
				tempea = cpu->regs.longregs[regebx] + cpu->disp32;
				break;
			case 4:
				tempea = cpu->sib_val + cpu->disp32;
				break;
			case 5:
				tempea = cpu->regs.longregs[regebp] + cpu->disp32;
				break;
			case 6:
				tempea = cpu->regs.longregs[regesi] + cpu->disp32;
				break;
			case 7:
				tempea = cpu->regs.longregs[regedi] + cpu->disp32;
				break;
			}
			break;
		}
	}

	return cpu->isaddr32 ? tempea : (tempea & 0xFFFF);
}

static FUNC_FORCE_INLINE void getea(CPU_t* cpu, uint8_t rmval) {
	uint32_t tempea = cpu_effective_offset(cpu, rmval);

	if (cpu->isaddr32) {
		cpu->ea = tempea + cpu->useseg;
	}
	else {
		cpu->ea = (tempea & 0xFFFF) + cpu->useseg;
	}
}

static FUNC_FORCE_INLINE void pushw(CPU_t* cpu, uint16_t pushval) {
	if (showops) debug_log(DEBUG_DETAIL, "PUSH %04X\n", pushval);

	if (cpu->segis32[regss]) {
		cpu->regs.longregs[regesp] -= 2;
		putmem16(cpu, cpu->segcache[regss], cpu->regs.longregs[regesp], pushval);
	}
	else {
		cpu->regs.wordregs[regsp] -= 2;
		putmem16(cpu, cpu->segcache[regss], cpu->regs.wordregs[regsp], pushval);
	}
}

static FUNC_FORCE_INLINE void pushl(CPU_t* cpu, uint32_t pushval) {
	if (showops) debug_log(DEBUG_DETAIL, "PUSH %08X\n", pushval);

	if (cpu->segis32[regss]) {
		cpu->regs.longregs[regesp] -= 4;
		putmem32(cpu, cpu->segcache[regss], cpu->regs.longregs[regesp], pushval);
	}
	else {
		cpu->regs.wordregs[regsp] -= 4;
		putmem32(cpu, cpu->segcache[regss], cpu->regs.wordregs[regsp], pushval);
	}
}

static FUNC_FORCE_INLINE void push(CPU_t* cpu, uint32_t pushval) {
	if (showops) debug_log(DEBUG_DETAIL, "PUSH %08X\n", pushval);

	if (cpu->segis32[regss]) {
		if (cpu->isoper32) {
			cpu->regs.longregs[regesp] -= 4;
			putmem32(cpu, cpu->segcache[regss], cpu->regs.longregs[regesp], pushval);
		}
		else {
			cpu->regs.longregs[regesp] -= 2;
			putmem16(cpu, cpu->segcache[regss], cpu->regs.longregs[regesp], pushval & 0xFFFF);
		}
	}
	else {
		if (cpu->isoper32) {
			cpu->regs.wordregs[regsp] -= 4;
			putmem32(cpu, cpu->segcache[regss], cpu->regs.wordregs[regsp], pushval);
		}
		else {
			cpu->regs.wordregs[regsp] -= 2;
			putmem16(cpu, cpu->segcache[regss], cpu->regs.wordregs[regsp], pushval & 0xFFFF);
		}
	}
}

static FUNC_FORCE_INLINE uint16_t popw(CPU_t* cpu) {
	uint16_t tempval;

	if (cpu->segis32[regss]) {
		tempval = getmem16(cpu, cpu->segcache[regss], cpu->regs.longregs[regesp]);
		cpu->regs.longregs[regesp] += 2;
	}
	else {
		tempval = getmem16(cpu, cpu->segcache[regss], cpu->regs.wordregs[regsp]);
		cpu->regs.wordregs[regsp] += 2;
	}

	if (showops) debug_log(DEBUG_DETAIL, "POP %04X\n", tempval);
	return tempval;
}

static FUNC_FORCE_INLINE uint32_t popl(CPU_t* cpu) {
	uint32_t tempval;

	if (cpu->segis32[regss]) {
		tempval = getmem32(cpu, cpu->segcache[regss], cpu->regs.longregs[regesp]);
		cpu->regs.longregs[regesp] += 4;
	}
	else {
		tempval = getmem32(cpu, cpu->segcache[regss], cpu->regs.wordregs[regsp]);
		cpu->regs.wordregs[regsp] += 4;
	}

	if (showops) debug_log(DEBUG_DETAIL, "POP %08X\n", tempval);
	return tempval;
}

static FUNC_FORCE_INLINE uint32_t pop(CPU_t* cpu) {
	uint32_t tempval;

	if (cpu->segis32[regss]) {
		if (cpu->isoper32) {
			tempval = getmem32(cpu, cpu->segcache[regss], cpu->regs.longregs[regesp]);
			cpu->regs.longregs[regesp] += 4;
		}
		else {
			tempval = getmem16(cpu, cpu->segcache[regss], cpu->regs.longregs[regesp]);
			cpu->regs.longregs[regesp] += 2;
		}
	}
	else {
		if (cpu->isoper32) {
			tempval = getmem32(cpu, cpu->segcache[regss], cpu->regs.wordregs[regsp]);
			cpu->regs.wordregs[regsp] += 4;
		}
		else {
			tempval = getmem16(cpu, cpu->segcache[regss], cpu->regs.wordregs[regsp]);
			cpu->regs.wordregs[regsp] += 2;
		}
	}

	if (showops) debug_log(DEBUG_DETAIL, "POP %08X\n", tempval);
	return tempval;
}

static FUNC_FORCE_INLINE uint16_t readrm16(CPU_t* cpu, uint8_t rmval) {
	if (cpu->mode < 3) {
		getea(cpu, rmval);
		return cpu_read(cpu, cpu->ea) | ((uint16_t)cpu_read(cpu, cpu->ea + 1) << 8);
	}
	else {
		return getreg16(cpu, rmval);
	}
}

static FUNC_FORCE_INLINE uint32_t readrm32(CPU_t* cpu, uint8_t rmval) {
	if (cpu->mode < 3) {
		getea(cpu, rmval);
		return cpu_read(cpu, cpu->ea) | ((uint32_t)cpu_read(cpu, cpu->ea + 1) << 8) | ((uint32_t)cpu_read(cpu, cpu->ea + 2) << 16) | ((uint32_t)cpu_read(cpu, cpu->ea + 3) << 24);
	}
	else {
		return getreg32(cpu, rmval);
	}
}

static FUNC_FORCE_INLINE uint64_t readrm64(CPU_t* cpu, uint8_t rmval) {
	if (cpu->mode < 3) {
		getea(cpu, rmval);
		return cpu_read(cpu, cpu->ea) | ((uint64_t)cpu_read(cpu, cpu->ea + 1) << 8) | ((uint64_t)cpu_read(cpu, cpu->ea + 2) << 16) | ((uint64_t)cpu_read(cpu, cpu->ea + 3) << 24) |
			((uint64_t)cpu_read(cpu, cpu->ea + 4) << 32) | ((uint64_t)cpu_read(cpu, cpu->ea + 5) << 40) | ((uint64_t)cpu_read(cpu, cpu->ea + 6) << 48) | ((uint64_t)cpu_read(cpu, cpu->ea + 7) << 56);
	}
	else {
		return getreg32(cpu, rmval);
	}
}

static FUNC_FORCE_INLINE uint8_t readrm8(CPU_t* cpu, uint8_t rmval) {
	if (cpu->mode < 3) {
		getea(cpu, rmval);
		return cpu_read(cpu, cpu->ea);
	}
	else {
		return getreg8(cpu, rmval);
	}
}

static FUNC_FORCE_INLINE void writerm32(CPU_t* cpu, uint8_t rmval, uint32_t value) {
	if (cpu->mode < 3) {
		getea(cpu, rmval);
		cpu_write(cpu, cpu->ea, value & 0xFF);
		cpu_write(cpu, cpu->ea + 1, value >> 8);
		cpu_write(cpu, cpu->ea + 2, value >> 16);
		cpu_write(cpu, cpu->ea + 3, value >> 24);
	}
	else {
		putreg32(cpu, rmval, value);
	}
}

static FUNC_FORCE_INLINE void writerm64(CPU_t* cpu, uint8_t rmval, uint64_t value) {
	if (cpu->mode < 3) {
		getea(cpu, rmval);
		cpu_write(cpu, cpu->ea, value & 0xFF);
		cpu_write(cpu, cpu->ea + 1, value >> 8);
		cpu_write(cpu, cpu->ea + 2, value >> 16);
		cpu_write(cpu, cpu->ea + 3, value >> 24);
		cpu_write(cpu, cpu->ea + 4, value >> 32);
		cpu_write(cpu, cpu->ea + 5, value >> 40);
		cpu_write(cpu, cpu->ea + 6, value >> 48);
		cpu_write(cpu, cpu->ea + 7, value >> 56);
	}
	else {
		putreg32(cpu, rmval, value);
	}
}

static FUNC_FORCE_INLINE void writerm16(CPU_t* cpu, uint8_t rmval, uint16_t value) {
	if (cpu->mode < 3) {
		getea(cpu, rmval);
		cpu_write(cpu, cpu->ea, value & 0xFF);
		cpu_write(cpu, cpu->ea + 1, value >> 8);
	}
	else {
		putreg16(cpu, rmval, value);
	}
}

static FUNC_FORCE_INLINE void writerm8(CPU_t* cpu, uint8_t rmval, uint8_t value) {
	if (cpu->mode < 3) {
		getea(cpu, rmval);
		cpu_write(cpu, cpu->ea, value);
	}
	else {
		putreg8(cpu, rmval, value);
	}
}

static FUNC_FORCE_INLINE void cpu_snapshot_exec_state(CPU_t* cpu, CPU_EXEC_SNAPSHOT_t* snapshot) {
	memcpy(&snapshot->regs, &cpu->regs, sizeof(cpu->regs));
	memcpy(snapshot->segregs, cpu->segregs, sizeof(cpu->segregs));
	memcpy(snapshot->segcache, cpu->segcache, sizeof(cpu->segcache));
	memcpy(snapshot->segis32, cpu->segis32, sizeof(cpu->segis32));
	memcpy(snapshot->seglimit, cpu->seglimit, sizeof(cpu->seglimit));
	snapshot->flags = makeflagsword(cpu);
	snapshot->cpl = cpu->cpl;
	snapshot->isCS32 = cpu->isCS32;
	snapshot->protected_mode = cpu->protected_mode;
	snapshot->paging = cpu->paging;
	snapshot->usegdt = cpu->usegdt;
	snapshot->gdtr = cpu->gdtr;
	snapshot->gdtl = cpu->gdtl;
	snapshot->idtr = cpu->idtr;
	snapshot->idtl = cpu->idtl;
	snapshot->ldtr = cpu->ldtr;
	snapshot->ldtl = cpu->ldtl;
	snapshot->trbase = cpu->trbase;
	snapshot->trlimit = cpu->trlimit;
	snapshot->ldt_selector = cpu->ldt_selector;
	snapshot->tr_selector = cpu->tr_selector;
	snapshot->trtype = cpu->trtype;
}

static FUNC_FORCE_INLINE void cpu_restore_exec_flags_exact(CPU_t* cpu, uint32_t flags) {
	cpu->cf = flags & 1;
	cpu->pf = (flags >> 2) & 1;
	cpu->af = (flags >> 4) & 1;
	cpu->zf = (flags >> 6) & 1;
	cpu->sf = (flags >> 7) & 1;
	cpu->tf = (flags >> 8) & 1;
	// Rollback must restore the exact pre-fault flags, not apply POPF privilege filtering.
	cpu->ifl = (flags >> 9) & 1;
	cpu->df = (flags >> 10) & 1;
	cpu->of = (flags >> 11) & 1;
	cpu->iopl = (flags >> 12) & 3;
	cpu->nt = (flags >> 14) & 1;
	cpu->rf = (flags >> 16) & 1;
	cpu->v86f = (flags >> 17) & 1;
	cpu->acf = (flags >> 18) & 1;
	cpu->idf = (flags >> 21) & 1;
}

static FUNC_FORCE_INLINE void cpu_restore_exec_state(CPU_t* cpu, const CPU_EXEC_SNAPSHOT_t* snapshot) {
	memcpy(&cpu->regs, &snapshot->regs, sizeof(cpu->regs));
	memcpy(cpu->segregs, snapshot->segregs, sizeof(cpu->segregs));
	memcpy(cpu->segcache, snapshot->segcache, sizeof(cpu->segcache));
	memcpy(cpu->segis32, snapshot->segis32, sizeof(cpu->segis32));
	memcpy(cpu->seglimit, snapshot->seglimit, sizeof(cpu->seglimit));
	cpu->isCS32 = snapshot->isCS32;
	cpu->protected_mode = snapshot->protected_mode;
	cpu->paging = snapshot->paging;
	cpu->usegdt = snapshot->usegdt;
	cpu->gdtr = snapshot->gdtr;
	cpu->gdtl = snapshot->gdtl;
	cpu->idtr = snapshot->idtr;
	cpu->idtl = snapshot->idtl;
	cpu->ldtr = snapshot->ldtr;
	cpu->ldtl = snapshot->ldtl;
	cpu->trbase = snapshot->trbase;
	cpu->trlimit = snapshot->trlimit;
	cpu->ldt_selector = snapshot->ldt_selector;
	cpu->tr_selector = snapshot->tr_selector;
	cpu->trtype = snapshot->trtype;
	cpu_restore_exec_flags_exact(cpu, snapshot->flags);
	cpu->cpl = cpu->v86f ? 3 : snapshot->cpl;
}

#endif

