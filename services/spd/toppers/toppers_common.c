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

#include <arch_helpers.h>
#include <assert.h>

#include <common/bl_common.h>
#include <lib/el3_runtime/context_mgmt.h>
#include <plat/common/platform.h>

#include <string.h>
#include "toppers_private.h"

/* EL1 GIC register context management */
void toppers_set_el1_gicregs_context(void)
{
	toppers_context_t *toppers_ctx;

	toppers_ctx = &toppers_sp_context[plat_my_core_pos()];
	toppers_ctx->gic_pri_mask[SECURE] =     TOPPERS_S_INTPRI_MASK;
	toppers_ctx->gic_pri_mask[NON_SECURE] = TOPPERS_NS_INTPRI_MASK;
}

void toppers_el1_gicregs_context_save(uint32_t security_state)
{
	toppers_context_t *toppers_ctx;

	toppers_ctx = &toppers_sp_context[plat_my_core_pos()];
#if		PLAT_GIC_VER <= 2
	toppers_ctx->gic_pri_mask[security_state] = mmio_read_32(PLAT_ARM_GICC_BASE + GICC_PMR);

#else	/* PLAT_GIC_VER > 2 */
	toppers_ctx->gic_pri_mask[security_state] = read_icc_pmr_el1();
#endif	/* PLAT_GIC_VER */
}

void toppers_el1_gicregs_context_restore(uint32_t security_state)
{
	toppers_context_t *toppers_ctx;

	toppers_ctx = &toppers_sp_context[plat_my_core_pos()];
#if		PLAT_GIC_VER <= 2
	mmio_write_32(PLAT_ARM_GICC_BASE + GICC_PMR, toppers_ctx->gic_pri_mask[security_state]);
#else	/* PLAT_GIC_VER > 2 */
	write_icc_pmr_el1(toppers_ctx->gic_pri_mask[security_state]);
	dsb();
#endif	/* PLAT_GIC_VER */
}

/*******************************************************************************
 * Given a TOPPERS kernel entrypoint info pointer, entry point PC, register width,
 * cpu id & pointer to a context data structure, this function will
 * initialize TOPPERS kernel context and entry point info for TOPPERS kernel.
 ******************************************************************************/
void toppers_init_toppers_ep_state(struct entry_point_info *toppers_entry_point,
				uint32_t rw, uint64_t pc,
				toppers_context_t *toppers_ctx)
{
	uint32_t ep_attr;

	/* Passing a NULL context is a critical programming error */
	assert(toppers_ctx);
	assert(toppers_entry_point);
	assert(pc);

	/* Associate this context with the cpu specified */
	toppers_ctx->mpidr = read_mpidr_el1();
	toppers_ctx->state = 0;
	set_toppers_pstate(toppers_ctx->state, TOPPERS_PSTATE_OFF);

	cm_set_context(&toppers_ctx->cpu_ctx, SECURE);
	toppers_set_el1_gicregs_context();

	/* initialize an entrypoint to set up the CPU context */
	ep_attr = SECURE | EP_ST_ENABLE;
	if (read_sctlr_el3() & SCTLR_EE_BIT)
		ep_attr |= EP_EE_BIG;
	SET_PARAM_HEAD(toppers_entry_point, PARAM_EP, VERSION_1, ep_attr);
	toppers_entry_point->pc = pc;
	if (rw == TOPPERS_AARCH64)
		toppers_entry_point->spsr = SPSR_64(MODE_EL1, MODE_SP_ELX,
						  DISABLE_ALL_EXCEPTIONS);
	else
		toppers_entry_point->spsr = SPSR_MODE32(MODE32_svc, SPSR_T_ARM,
						      SPSR_E_LITTLE,
						      DAIF_FIQ_BIT |
							DAIF_IRQ_BIT |
							DAIF_ABT_BIT);
	memset(&toppers_entry_point->args, 0, sizeof(toppers_entry_point->args));
}

/*******************************************************************************
 * This function takes an TOPPERS kernel context pointer and:
 * 1. Applies the S-EL1 system register context from toppers_ctx->cpu_ctx.
 * 2. Saves the current C runtime state (callee saved registers) on the stack
 *    frame and saves a reference to this state.
 * 3. Calls el3_exit() so that the EL3 system and general purpose registers
 *    from the toppers_ctx->cpu_ctx are used to enter the TOPPERS kernel image.
 ******************************************************************************/
uint64_t toppers_synchronous_sp_entry(toppers_context_t *toppers_ctx)
{
	uint64_t rc;

	assert(toppers_ctx != NULL);
	assert(toppers_ctx->c_rt_ctx == 0);

	/* Apply the Secure EL1 system register context and switch to it */
	assert(cm_get_context(SECURE) == &toppers_ctx->cpu_ctx);

	cm_el1_sysregs_context_restore(SECURE);

    toppers_el1_gicregs_context_restore(SECURE);

	cm_set_next_eret_context(SECURE);

	rc = toppers_enter_sp(&toppers_ctx->c_rt_ctx);

#if DEBUG
	toppers_ctx->c_rt_ctx = 0;
#endif

	return rc;
}


/*******************************************************************************
 * This function takes an TOPPERS kernel context pointer and:
 * 1. Saves the S-EL1 system register context tp toppers_ctx->cpu_ctx.
 * 2. Restores the current C runtime state (callee saved registers) from the
 *    stack frame using the reference to this state saved in toppers_enter_sp().
 * 3. It does not need to save any general purpose or EL3 system register state
 *    as the generic smc entry routine should have saved those.
 ******************************************************************************/
void toppers_synchronous_sp_exit(toppers_context_t *toppers_ctx, uint64_t ret)
{
	assert(toppers_ctx != NULL);

	assert(toppers_ctx->c_rt_ctx != 0);
	toppers_exit_sp(toppers_ctx->c_rt_ctx, ret);

	/* Should never reach here */
	assert(0);
}
