/*
 * Copyright (C) 2018-2019 by Embedded and Real-Time Systems Laboratory
 *               Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 * Copyright (c) 2013-2014, ARM Limited and Contributors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of ARM nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __TOPPERS_PRIVATE_H__
#define __TOPPERS_PRIVATE_H__

#include <arch.h>
#include <context.h>

#include <bl31/interrupt_mgmt.h>

#include <platform_def.h>

#include <lib/psci/psci.h>
#include <lib/spinlock.h>

#include "toppers_config.h"

/*******************************************************************************
 * TOPPERS kernel PM state information e.g. TOPPERS kernel is suspended,
 * uninitialised etc
 * and macros to access the state information in the per-cpu 'state' flags
 ******************************************************************************/
#define TOPPERS_PSTATE_OFF		0
#define TOPPERS_PSTATE_ON		1
#define TOPPERS_PSTATE_SUSPEND	2
#define TOPPERS_PSTATE_SHIFT	0
#define TOPPERS_PSTATE_MASK		0x3
#define get_toppers_pstate(state)	((state >> TOPPERS_PSTATE_SHIFT) & \
				 TOPPERS_PSTATE_MASK)
#define clr_toppers_pstate(state)	(state &= ~(TOPPERS_PSTATE_MASK \
					    << TOPPERS_PSTATE_SHIFT))
#define set_toppers_pstate(st, pst) do {					       \
					clr_toppers_pstate(st);		       \
					st |= (pst & TOPPERS_PSTATE_MASK) <<     \
						TOPPERS_PSTATE_SHIFT;	       \
				} while (0)


/*******************************************************************************
 * TOPPERS kernel execution state information i.e. aarch32 or aarch64
 ******************************************************************************/
#define TOPPERS_AARCH32			MODE_RW_32
#define TOPPERS_AARCH64			MODE_RW_64

/*******************************************************************************
 * The TOPPERS kernel should know the specific type of TOPPERS kernel
 ******************************************************************************/
#define TOPPERS_TYPE_UP			PSCI_TOS_NOT_UP_MIG_CAP
#define TOPPERS_TYPE_UPM		PSCI_TOS_UP_MIG_CAP
#define TOPPERS_TYPE_MP			PSCI_TOS_NOT_PRESENT_MP

/*******************************************************************************
 * TOPPERS kernel migrate type information as known to the TOPPERS dispatcher.
 * We assume that the TOPPERS kernel is dealing with an MP Secure Payload.
 ******************************************************************************/
#define TOPPERS_MIGRATE_INFO	TOPPERS_TYPE_MP

/*******************************************************************************
 * Number of cpus that the present on this platform. TODO: Rely on a topology
 * tree to determine this in the future to avoid assumptions about mpidr
 * allocation
 ******************************************************************************/
#define TOPPERS_CORE_COUNT		PLATFORM_CORE_COUNT

/*******************************************************************************
 * Initial value of GIC interrupt priorty mask
 ******************************************************************************/
#define TOPPERS_S_INTPRI_MASK		0x80
#define TOPPERS_NS_INTPRI_MASK		0xFE

/*******************************************************************************
 * Constants that allow assembler code to preserve callee-saved registers of the
 * C runtime context while performing a security state switch.
 ******************************************************************************/
#define TOPPERS_C_RT_CTX_X19		0x0
#define TOPPERS_C_RT_CTX_X20		0x8
#define TOPPERS_C_RT_CTX_X21		0x10
#define TOPPERS_C_RT_CTX_X22		0x18
#define TOPPERS_C_RT_CTX_X23		0x20
#define TOPPERS_C_RT_CTX_X24		0x28
#define TOPPERS_C_RT_CTX_X25		0x30
#define TOPPERS_C_RT_CTX_X26		0x38
#define TOPPERS_C_RT_CTX_X27		0x40
#define TOPPERS_C_RT_CTX_X28		0x48
#define TOPPERS_C_RT_CTX_X29		0x50
#define TOPPERS_C_RT_CTX_X30		0x58
#define TOPPERS_C_RT_CTX_SIZE		0x60
#define TOPPERS_C_RT_CTX_ENTRIES	(TOPPERS_C_RT_CTX_SIZE >> DWORD_SHIFT)

/*******************************************************************************
 * SMC Function call IDs
 ******************************************************************************/
#define TOPPERS_SMC_RV(func_num) \
		((SMC_TYPE_FAST << FUNCID_TYPE_SHIFT) | \
		 ((SMC_32) << FUNCID_CC_SHIFT) | \
		 (62 << FUNCID_OEN_SHIFT) | \
		 ((func_num) & FUNCID_NUM_MASK))

/* Set TOPPERS kernel entry points (fiq vector etc.) */
#define TOPPERS_SMC_SET_VECTOR			TOPPERS_SMC_RV(0)
/* Notification TOPPERS/FMP initialize complete */
#define TOPPERS_SMC_INIT_COMPLETE		TOPPERS_SMC_RV(1)
/* Trun on a cpu core */
#define TOPPERS_SMC_CPU_ON				TOPPERS_SMC_RV(2)
/* Change the secure state */
#define TOPPERS_SMC_SWITCH_SECURE_STATE	TOPPERS_SMC_RV(3)
/* Generate interrupt to Secure state */
#define TOPPERS_SMC_GEN_INT				TOPPERS_SMC_RV(4)
/* Obtains the ID of a system call */
#define TOPPERS_SMC_GETID				TOPPERS_SMC_RV(5)
/* Set permissions for a safeg system call */
#define TOPPERS_SMC_SETPERM				TOPPERS_SMC_RV(6)
/* Signals an interrupt to the opposite world */
#define TOPPERS_SMC_SIGNAL				TOPPERS_SMC_RV(7)
/* Register a dynamic system call */
#define TOPPERS_SMC_REGDYN				TOPPERS_SMC_RV(8)
/* Register a notifier call */
#define TOPPERS_SMC_REGNOT				TOPPERS_SMC_RV(9)
/* Read specified address from EL3 */
#define TOPPERS_SMC_READL				TOPPERS_SMC_RV(10)
/* Write specified address from EL3 */
#define TOPPERS_SMC_WRITEL				TOPPERS_SMC_RV(11)
/* Restart NonTrusted OS */
#define TOPPERS_SMC_RESTARNT			TOPPERS_SMC_RV(12)
/* Invoke a dynamic system call */
#define TOPPERS_SMC_INVOKE				TOPPERS_SMC_RV(13)
/* Temporary diagnostic: Secure EL1 entry */
#define TOPPERS_SMC_DIAG_ENTRY          TOPPERS_SMC_RV(14)

/*******************************************************************************
 * SMC Function call return codes
 ******************************************************************************/
#define	TOPPERS_SMC_RET_SUCCESS				0
#define	TOPPERS_SMC_RET_INVALID_STATE		-1
#define	TOPPERS_SMC_RET_PARAM_ERROR			-2
#define	TOPPERS_SMC_RET_NOT_READY			-3
#define TOPPERS_SMC_RET_FULL				-4
#define TOPPERS_SMC_RET_INEXISTENT			-5
#define TOPPERS_SMC_RET_NOT_SUPPORTED		-6
#define TOPPERS_SMC_RET_UNEXPECTED_ERROR	-7

#ifndef __ASSEMBLY__

#include <lib/cassert.h>

#include <stdint.h>

typedef uint32_t toppers_vector_isn_t;

typedef struct toppers_vectors {
	toppers_vector_isn_t	cpu_on_entry;
	toppers_vector_isn_t	fiq_entry;
} toppers_vectors_t;


/* AArch64 callee saved general purpose register context structure. */
DEFINE_REG_STRUCT(c_rt_regs, TOPPERS_C_RT_CTX_ENTRIES);

/*
 * Compile time assertion to ensure that both the compiler and linker
 * have the same double word aligned view of the size of the C runtime
 * register context.
 */
CASSERT(TOPPERS_C_RT_CTX_SIZE == sizeof(c_rt_regs_t),	\
	assert_spd_c_rt_regs_size_mismatch);

/*******************************************************************************
 * Structure which helps the TOPPERS dispatcher to maintain the per-cpu state of
 * TOPPERS kernel.
 * 'state'          - collection of flags to track TOPPERS kernel state e.g. on/off
 * 'mpidr'          - mpidr to associate a context with a cpu
 * 'c_rt_ctx'       - stack address to restore C runtime context from after
 *                    returning from a synchronous entry into TOPPERS kernel.
 * 'cpu_ctx'        - space to maintain TOPPERS kernel architectural state
 ******************************************************************************/
typedef struct toppers_context {
	uint32_t state;
	uint64_t mpidr;
	uint64_t c_rt_ctx;
	uint64_t int_eret_addr;
	cpu_context_t cpu_ctx;
	uint64_t gic_pri_mask[2];
} toppers_context_t;

/*******************************************************************************
 * SafeG
 ******************************************************************************/
#define	SAFEG_NUM_STATIC_SYSCALL	9
#define	SAFEG_NUM_SYSCALL			(SAFEG_NUM_STATIC_SYSCALL + MONITOR_MAX_DYN_SYSCALLS)
#define SAFEG_SYSCALL_NAME_LENGTH	8

/* SafeG Syscall ID */
#define SAFEG_SYSCALL_ID__GETID		(0)
#define SAFEG_SYSCALL_ID__SETPERM	(1)
#define SAFEG_SYSCALL_ID__SWITCH	(2)
#define SAFEG_SYSCALL_ID__SIGNAL	(3)
#define SAFEG_SYSCALL_ID__REGDYN	(4)
#define SAFEG_SYSCALL_ID__REGNOT	(5)
#define SAFEG_SYSCALL_ID__READL		(6)
#define SAFEG_SYSCALL_ID__WRITEL	(7)
#define SAFEG_SYSCALL_ID__RESTARTNT	(8)

/* type defination */
typedef struct {
	uint32_t is_used;
	uint32_t is_t_callable;
	uint32_t is_nt_callable;
	uint8_t  name[SAFEG_SYSCALL_NAME_LENGTH];
	uint32_t (*function)(uint32_t core_id, uint32_t ns,
						 uint32_t a, uint32_t b, uint32_t c);
} safeg_syscall_t;

typedef struct {
	uint32_t is_t_callable;
	uint32_t is_nt_callable;
	uint8_t  name[SAFEG_SYSCALL_NAME_LENGTH];
	uint32_t (*function)(uint32_t core_id, uint32_t ns,
						 uint32_t a, uint32_t b, uint32_t c);
} smc_syscall_t;

/* prototypes */
extern safeg_syscall_t	the_syscalls[SAFEG_NUM_SYSCALL];

extern void
	safeg_syscalls_init(void);
extern uint32_t
	safeg_ssc_getid(uint32_t current_sec, char *sc_name, uint32_t *sc_id);
extern uint32_t
	safeg_ssc_setperm(uint32_t current_sec, uint32_t sc_id, uint32_t is_t_callable, uint32_t is_nt_callable);
extern uint32_t
	safeg_ssc_signal(uint32_t current_sec);
extern uint32_t
	safeg_ssc_regdyn(uint32_t current_sec, smc_syscall_t *dsc_context, uint32_t *reg_sc_id);
extern uint32_t
	safeg_ssc_readl(uint32_t current_sec, volatile uint32_t *address, uint32_t *rdata_ref);
extern uint32_t
	safeg_ssc_writel(uint32_t current_sec, volatile uint32_t *address, uint32_t wdata);
extern uint32_t
	safeg_ssc_invoke(uint32_t current_sec, uint32_t sc_id, uint64_t param1, uint64_t param2, uint64_t param3);

extern uint32_t
	safeg_get_signal_intno(uint32_t proc_id, uint32_t current_sec);
extern uint32_t
	safeg_is_valid_memory(uint32_t current_sec, uint64_t address, uint64_t size);
extern uint32_t
	safeg_is_valid_readl_address(uint32_t core_id, uint32_t current_sec, uint64_t address);
extern uint32_t
	safeg_is_valid_writel_address(uint32_t core_id, uint32_t current_sec, uint64_t address);


/* macro */
inline int
safeg_check_perm(uint32_t syscall_id, uint32_t current_sec)
{
	if( current_sec == SECURE )  {
		return the_syscalls[syscall_id].is_t_callable;
	}
	else  {
		return the_syscalls[syscall_id].is_nt_callable;
	}
}


/*******************************************************************************
 * TOPPERS kernel power management handlers
 ******************************************************************************/
extern const spd_pm_ops_t toppers_pm;

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
struct toppers_vectors;

/*******************************************************************************
 * Function & Data prototypes
 ******************************************************************************/
uint64_t toppers_enter_sp(uint64_t *c_rt_ctx);
void __dead2 toppers_exit_sp(uint64_t c_rt_ctx, uint64_t ret);
uint64_t toppers_synchronous_sp_entry(toppers_context_t *toppers_ctx);
void __dead2 toppers_synchronous_sp_exit(toppers_context_t *toppers_ctx, uint64_t ret);
void toppers_init_toppers_ep_state(struct entry_point_info *toppers_ep,
				uint32_t rw,
				uint64_t pc,
				toppers_context_t *toppers_ctx);
extern int plat_gen_int(uint32_t int_id);

extern toppers_context_t toppers_sp_context[TOPPERS_CORE_COUNT];
extern struct toppers_vectors *toppers_vectors;
extern void	toppers_el1_gicregs_context_save(uint32_t security_state);
extern void	toppers_el1_gicregs_context_restore(uint32_t security_state);
extern spinlock_t      toppers_lock;

#endif /*__ASSEMBLY__*/

#endif /* __TOPPERS_PRIVATE_H__ */
