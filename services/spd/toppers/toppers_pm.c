/*
 * Copyright (C) 2018-2019 by Embedded and Real-Time Systems Laboratory
 *               Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 * Copyright (c) 2013-2015, ARM Limited and Contributors. All rights reserved.
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
#include <common/debug.h>
#include <plat/common/platform.h>

#include "toppers_private.h"


/*******************************************************************************
 * This cpu has been turned on. Enter TOPPERS kernel to initialise S-EL1 and other bits
 * before passing control back to the Secure Monitor. Entry in S-El1 is done
 * after initialising minimal architectural state that guarantees safe
 * execution.
 ******************************************************************************/
static void toppers_cpu_on_finish_handler(u_register_t unused)
{
	int32_t rc = 0;
	uint32_t linear_id = plat_my_core_pos();
	toppers_context_t *toppers_ctx = &toppers_sp_context[linear_id];
	entry_point_info_t toppers_on_entrypoint;

	assert(toppers_vectors);
	assert(get_toppers_pstate(toppers_ctx->state) == TOPPERS_PSTATE_OFF);

	toppers_init_toppers_ep_state(&toppers_on_entrypoint,
	#ifdef	BL32_AARCH32
				TOPPERS_AARCH32,
	#else	/* BL32_AARCH32 */
				TOPPERS_AARCH64,
	#endif	/* BL32_AARCH32 */
				(uint64_t)&toppers_vectors->cpu_on_entry,
				toppers_ctx);

	/* Initialise this cpu's secure context */
	cm_init_my_context(&toppers_on_entrypoint);

	/* Enter TOPPERS kernel */
	rc = toppers_synchronous_sp_entry(toppers_ctx);

	/*
	 * Read the response from TOPPERS kernel. A non-zero return means that
	 * something went wrong while communicating with TOPPERS kernel.
	 */
	if (rc != 0)
		panic();

	/* Restore value of system registers. */
	cm_el1_sysregs_context_restore(NON_SECURE);
	toppers_el1_gicregs_context_restore(NON_SECURE);

	/* Update its context to reflect the state TOPPERS kernel is in */
	set_toppers_pstate(toppers_ctx->state, TOPPERS_PSTATE_ON);
}


/*******************************************************************************
 * Structure populated by the TOPPERS Dispatcher to be given a chance to
 * perform any TOPPERS kernel bookkeeping before PSCI executes a power mgmt.
 * operation.
 ******************************************************************************/
const spd_pm_ops_t toppers_pm = {
	.svc_on =             NULL,
	.svc_off =            NULL,
	.svc_suspend =        NULL,
	.svc_on_finish =      toppers_cpu_on_finish_handler,
	.svc_suspend_finish = NULL,
	.svc_migrate =        NULL,
	.svc_migrate_info =   NULL,
	.svc_system_off =     NULL,
	.svc_system_reset =   NULL,
};
