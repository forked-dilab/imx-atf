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


/*******************************************************************************
 * This is the Secure Payload Dispatcher (SPD). The dispatcher is meant to be a
 * plug-in component to the Secure Monitor, registered as a runtime service. The
 * SPD is expected to be a functional extension of the Secure Payload (SP) that
 * executes in Secure EL1. The Secure Monitor will delegate all SMCs targeting
 * the Trusted OS/Applications range to the dispatcher. The SPD will either
 * handle the request locally or delegate it to the Secure Payload. It is also
 * responsible for initialising and maintaining communication with the SP.
 ******************************************************************************/
#include <arch_helpers.h>
#include <assert.h>

#include <bl31/bl31.h>
#include <common/bl_common.h>
#include <lib/el3_runtime/context_mgmt.h>
#include <common/debug.h>

#include <errno.h>

#include <plat/common/platform.h>
#include <common/runtime_svc.h>

#include <stddef.h>

#include <tools_share/uuid.h>

#include "toppers_config.h"
#include "toppers_private.h"


extern int psci_validate_mpidr(u_register_t mpidr);
extern int psci_get_ns_ep_info(entry_point_info_t *ep, uintptr_t entrypoint,
	u_register_t context_id);
extern int psci_cpu_on_start_toppers(u_register_t target_cpu,
	entry_point_info_t *ep);
extern int plat_gen_int(uint32_t int_id);


#define MASK_SPSR_M			0x000000000000000F
#define SPSR_M_EL1h			5
#define SPSR_D				(u_register_t)(1 << 9)
#define SPSR_A				(u_register_t)(1 << 8)
#define SPSR_I				(u_register_t)(1 << 7)
#define SPSR_F				(u_register_t)(1 << 6)

/*******************************************************************************
 * TOPPERS kernel IRQ handler (SP_EL1). It is initialised once on the primary
 * core after a cold boot.
 ******************************************************************************/
toppers_vectors_t	*toppers_vectors;

/*******************************************************************************
 * Array to keep track of per-cpu TOPPERS kernel state
 ******************************************************************************/
toppers_context_t toppers_sp_context[TOPPERS_CORE_COUNT];

spinlock_t     toppers_lock;

static int32_t toppers_init(void);

/*******************************************************************************
 * This function is the handler registered for S-EL1 interrupts by the
 * TOPPERS kernel. It validates the interrupt and upon success arranges entry
 * into the TOPPERS kernel IRQ handler (SP_EL1) for handling the interrupt.
 ******************************************************************************/
static uint64_t toppers_sel1_interrupt_handler(uint32_t id,
						uint32_t flags,
						void *handle,
						void *cookie)
{
	uint32_t linear_id;
	toppers_context_t *toppers_ctx;

	linear_id = plat_my_core_pos();
	toppers_ctx = &toppers_sp_context[linear_id];

	if( get_interrupt_src_ss(flags) == NON_SECURE )
	{
		/* Sanity check the pointer to this cpu's context */
		assert(handle == cm_get_context(NON_SECURE));

		/* Save the non-secure context before entering the TOPPERS kernel */
	#if    CTX_INCLUDE_FPREGS
		fpregs_context_save(get_fpregs_ctx(cm_get_context(NON_SECURE)));
	#endif /* CTX_INCLUDE_FPREGS */
		cm_el1_sysregs_context_save(NON_SECURE);
		toppers_el1_gicregs_context_save(NON_SECURE);

		/* Get a reference to this cpu's TOPPERS kernel context */
		assert(&toppers_ctx->cpu_ctx == cm_get_context(SECURE));

		/* Setup secure context */
		cm_el1_sysregs_context_restore(SECURE);
		toppers_el1_gicregs_context_restore(SECURE);
	#if    CTX_INCLUDE_FPREGS
		fpregs_context_restore(get_fpregs_ctx(cm_get_context(SECURE)));
	#endif /* CTX_INCLUDE_FPREGS */
		cm_set_next_eret_context(SECURE);

		/* Return to Secure OS */
		SMC_RET1(&toppers_ctx->cpu_ctx, TOPPERS_SMC_RET_SUCCESS);
	}
	return	0;
}


/*******************************************************************************
 * TOPPERS kernel Dispatcher setup. The TOPPERS kernel finds out the TOPPERS
 * entrypoint and type (aarch32/aarch64) if not already known and initialises
 *  the context for entry into TOPPERS kernel for its initialization.
 ******************************************************************************/
int32_t toppers_setup(void)
{
    // NOTICE("[TOPPERS] toppers_setup: ENTER\n");

#if		MONITOR_BL32_DISABLE == false
	entry_point_info_t *toppers_ep_info;
	uint32_t linear_id;

	linear_id = plat_my_core_pos();

	/*
	 * Get information about the Secure Payload (BL32) image. Its
	 * absence is a critical failure.  TODO: Add support to
	 * conditionally include the SPD service
	 */
	toppers_ep_info = bl31_plat_get_next_image_ep_info(SECURE);
	if (!toppers_ep_info) {
		WARN("No TOPPERS kernel provided by BL2 boot loader, Booting device"
			" without TOPPERS kernel initialization. SMC`s destined for"
			"TOPPERS kernel will return SMC_UNK\n");
		return 1;
	}

	/*
	 * If there's no valid entry point for SP, we return a non-zero value
	 * signalling failure initializing the service. We bail out without
	 * registering any handlers
	 */
	if (!toppers_ep_info->pc)
	{
		ERROR("TOPPERS kernel image hasn't no valid entry point for SP\n");
		return 1;
	}

	if( linear_id == 0 )  {
		safeg_syscalls_init();
		toppers_lock.lock = 0;
	}

	/*
	 * We could inspect the SP image and determine it's execution
	 * state i.e whether AArch32 or AArch64. Assuming it's AArch32
	 * for the time being.
	 */
	toppers_init_toppers_ep_state(toppers_ep_info,
	#ifdef	BL32_AARCH32
				TOPPERS_AARCH32,
	#else	/* BL32_AARCH32 */
				TOPPERS_AARCH64,
	#endif	/* BL32_AARCH32 */
				toppers_ep_info->pc,
				&toppers_sp_context[linear_id]);

	/*
	 * All TOPPERS kernel initialization done. Now register our init function with
	 * BL31 for deferred invocation
	 */
	bl31_register_bl32_init(&toppers_init);
#endif	/* MONITOR_BL32_DISABLE == false */

	return 0;
}

/*******************************************************************************
 * This function passes control to the TOPPERS kernel image (BL32) for the first
 * time on the primary cpu after a cold boot. It assumes that a valid secure
 * context has already been created by toppers_setup() which can be directly
 * used.  It also assumes that a valid non-secure context has been
 * initialised by PSCI so it does not need to save and restore any
 * non-secure state. This function performs a synchronous entry into
 * TOPPERS kernel. TOPPERS kernel passes control back to this routine through a
 * SMC.
 ******************************************************************************/
static int32_t toppers_init(void)
{
	uint32_t linear_id = plat_my_core_pos();
	toppers_context_t *toppers_ctx = &toppers_sp_context[linear_id];
	entry_point_info_t *toppers_entry_point;
	uint64_t rc;
	volatile uint64_t	reg64_val;

	/*
	 * Get information about the TOPPERS kernel(BL32) image. Its
	 * absence is a critical failure.
	 */
	toppers_entry_point = bl31_plat_get_next_image_ep_info(SECURE);
	assert(toppers_entry_point);

	cm_init_my_context(toppers_entry_point);

	/* Unlock CPUECTLR.SMCEN protection for S-EL1 */
	reg64_val = read_actlr_el3() | (uint64_t)(1 << 1);
	write_actlr_el3(reg64_val);

	/*
	 * Arrange for an entry into TOPPERS kernel. It will be returned via
	 * TOPPERS_SMC_INIT_COMPLETE case
	 */
	rc = toppers_synchronous_sp_entry(toppers_ctx);

    if (rc != TOPPERS_SMC_RET_SUCCESS) {
        ERROR("TOPPERS: BL32 initialization failed (rc=%llu)\n",
            (unsigned long long)rc);
        panic();
    }


	/* Restore value of system registers. */
	cm_el1_sysregs_context_restore(NON_SECURE);
	toppers_el1_gicregs_context_restore(NON_SECURE);

    /*
    * TOPPERS synchronous initialization:
    *   0 = success
    *
    * TF-A runtime service initialization:
    *   1 = success
    */
    return 1;
}


/*******************************************************************************
 * This function is responsible for handling all SMCs in the Trusted OS/App
 * range from the non-secure state as defined in the SMC Calling Convention
 * Document. It is also responsible for communicating with the Secure
 * payload to delegate work and return results back to the non-secure
 * state. Lastly it will also return any information that TOPPERS kernel needs
 * to do the work assigned to it.
 ******************************************************************************/
static uintptr_t toppers_smc_handler(uint32_t smc_fid,
			 u_register_t x1,
			 u_register_t x2,
			 u_register_t x3,
			 u_register_t x4,
			 void *cookie,
			 void *handle,
			 u_register_t flags)
{
	cpu_context_t		*ns_cpu_ctx;
	uint32_t			linear_id = plat_my_core_pos();
	toppers_context_t	*toppers_ctx = &toppers_sp_context[linear_id];
	entry_point_info_t	ep;
	el3_state_t			*state;
	uint32_t 			current_sec, sc_id, rdata;
	uint64_t            rc, ctx_elr_el3;
	int					rc32, core_idx;

	/*
	 * Determine which security state this SMC originated from
	 */
	if( is_caller_secure(flags) )  {
		current_sec = SECURE;
	}
	else  {
		current_sec = NON_SECURE;
	}

	switch( smc_fid )
	{
    case TOPPERS_SMC_DIAG_ENTRY:
        if (current_sec == SECURE) {
            SMC_RET1(handle, TOPPERS_SMC_RET_SUCCESS);
        }

        NOTICE("[TOPPERS] DIAG: rejected non-secure caller\n");
        SMC_RET1(handle, TOPPERS_SMC_RET_INVALID_STATE);
        break;
	/*
	 * Set TOPPERS kernel entry points (fiq vector etc.)
	 *
	 * Parameter (Registers) :
	 * 	 r0/x0  SMC Function ID, TOPPERS_SMC_SET_VECTOR
	 *   r1/x1	TOPPERS kernel entry points (fiq vector etc.)
	 *
	 * Return value:
	 *   r0/x0	TOPPERS_SMC_RET_SUCCESS -       Success
	 *   		TOPPERS_SMC_RET_INVALID_STATE - Called from non-secure state
	 */
	case TOPPERS_SMC_SET_VECTOR:
		if( current_sec == SECURE )  {
		#ifdef DEBUG_SPD
			INFO("[ATF] SMC Func: TOPPERS_SMC_SET_VECTOR\n");
		#endif /* DEBUG_SPD */
			/*
			 * Stash the TOPPERS kernel entry points information. This is done
			 * only once on the primary cpu
			 */
			assert(toppers_vectors == NULL);
			toppers_vectors = (toppers_vectors_t *) x1;

			if (toppers_vectors)  {
				/*
				 * TOPPERS kernel has been successfully initialized.
				 * Register power management hooks with PSCI
				 */
				psci_register_spd_pm_hook(&toppers_pm);
			}

			rc32 = TOPPERS_SMC_RET_SUCCESS;
		}
		else  {
			rc32 = TOPPERS_SMC_RET_INVALID_STATE;
		}
		SMC_RET1((uint64_t)cm_get_context(current_sec), rc32);
		break;

	/*
	 * Notification TOPPERS/FMP initialize complete
	 *
	 * Parameter (Registers) :
	 * 	 r0/x0	SMC Function ID, TOPPERS_SMC_INIT_COMPLETE
	 *   r1/x1	NS-EL1 sub core dummy entry
	 *
	 * Return value:
	 *   r0/x0	TOPPERS_SMC_RET_SUCCESS -       Success
	 *   		TOPPERS_SMC_RET_INVALID_STATE - Called from non-secure state
	 *   		TOPPERS_SMC_RET_NOT_READY -     NS-EL1 OS isn't ready
	 */
	case TOPPERS_SMC_INIT_COMPLETE:
		if( current_sec == SECURE )  {
		#ifdef DEBUG_SPD
			INFO("[ATF] SMC Func: TOPPERS_SMC_INIT_COMPLETE\n");
		#endif /* DEBUG_SPD */

			core_idx = plat_core_pos_by_mpidr(read_mpidr());
			if( core_idx == 0 )
			{
				/* Save secure context */
			#if    CTX_INCLUDE_FPREGS
				fpregs_context_save(get_fpregs_ctx(cm_get_context(SECURE)));
			#endif /* CTX_INCLUDE_FPREGS */
				cm_el1_sysregs_context_save(SECURE);
				toppers_el1_gicregs_context_save(SECURE);

				/* Set power on to PSTATE */
				set_toppers_pstate(toppers_ctx->state, TOPPERS_PSTATE_ON);

				/*
				 * Register an interrupt handler for S-EL1 interrupts
				 * when generated during code executing in the
				 * non-secure state.
				 */
				flags = 0;
				set_interrupt_rm_flag(flags, NON_SECURE);
				rc = register_interrupt_type_handler(INTR_TYPE_S_EL1,
							toppers_sel1_interrupt_handler,
							flags);
				if (rc)
					panic();
			}
			else
			{
				/* Make sure if NS-EL1 OS is ready */
				ns_cpu_ctx = cm_get_context_by_index(core_idx, NON_SECURE);
				state = get_el3state_ctx(ns_cpu_ctx);
				spin_lock(&toppers_lock);
				ctx_elr_el3 = read_ctx_reg(state, CTX_ELR_EL3);
				spin_unlock(&toppers_lock);
				if( ctx_elr_el3 == x1 )  {
					SMC_RET1((uint64_t)cm_get_context(SECURE), TOPPERS_SMC_RET_NOT_READY);
				}

			#ifdef DEBUG_SPD
				INFO("[ATF] NS-EL1 OS booted core%d.\n", core_idx);
			#endif /* DEBUG_SPD */

				/* Save secure context */
			#if    CTX_INCLUDE_FPREGS
				fpregs_context_save(get_fpregs_ctx(cm_get_context(SECURE)));
			#endif /* CTX_INCLUDE_FPREGS */
				cm_el1_sysregs_context_save(SECURE);
				toppers_el1_gicregs_context_save(SECURE);

				/* Set power on to PSTATE */
				set_toppers_pstate(toppers_ctx->state, TOPPERS_PSTATE_ON);

				/*
				 * Set interrupt routing model for S-EL1 interrupts
				 * when generated during code executing in the
				 * non-secure state.
				 */
				flags = 0;
				set_interrupt_rm_flag(flags, NON_SECURE);
				rc = set_routing_model(INTR_TYPE_S_EL1, flags);
				if (rc)
					panic();
			}

			/*
			 * TOPPERS kernel reports completion. The TOPPERS kernelD must have initiated
			 * the original request through a synchronous entry into
			 * TOPPERS kernel. Jump back to the original C runtime context.
			 */
			toppers_synchronous_sp_exit(toppers_ctx, 0);
		}
		else  {
			SMC_RET1((uint64_t)cm_get_context(NON_SECURE), TOPPERS_SMC_RET_INVALID_STATE);
		}
		break;

	/*
	 * Trun on a cpu core from secure state
	 *
	 * Parameter (Registers) :
	 * 	 r0/x0	SMC Function ID, TOPPERS_SMC_CPU_ON
	 * 	 r1/x1  target cpu core (value of mpidr)
	 * 	 r2/x2  S-EL1 entry point
	 * 	 r3/x3  Value of X0 at cpu core is booted up
	 *
	 * Return value:
	 *   r0/x0	TOPPERS_SMC_RET_SUCCESS -          Success
	 *   		TOPPERS_SMC_RET_INVALID_STATE -    Called from non-secure state
	 *   		TOPPERS_SMC_RET_PARAM_ERROR -      Paramter error
	 *   		TOPPERS_SMC_RET_UNEXPECTED_ERROR - Unexpected error
	 */
	case TOPPERS_SMC_CPU_ON:
	#ifdef DEBUG_SPD
		INFO("[ATF] SMC Func: TOPPERS_SMC_CPU_ON\n");
	#endif /* DEBUG_SPD */

		if( current_sec == SECURE )  {
			/* This function supports sub core only */
			if( x1 == 0 )  {
				SMC_RET1((uint64_t)cm_get_context(SECURE), TOPPERS_SMC_RET_PARAM_ERROR);
			}

			/* Determine if the cpu exists of not */
			rc32 = psci_validate_mpidr(x1);
			if( rc32 != PSCI_E_SUCCESS )  {
				SMC_RET1((uint64_t)cm_get_context(SECURE), TOPPERS_SMC_RET_PARAM_ERROR);
			}

			/* Get the entry_point_info */
			/* context_idは、Linuxの指定値 0にする */
			rc32 = psci_get_ns_ep_info(&ep, (uintptr_t)x2, 0);
			if( rc32 != PSCI_E_SUCCESS )  {
				SMC_RET1((uint64_t)cm_get_context(SECURE), TOPPERS_SMC_RET_UNEXPECTED_ERROR);
			}

			/*
			 * To turn this cpu on, specify which power
			 * levels need to be turned on
			 */
			rc32 = psci_cpu_on_start_toppers(x1, &ep);
			if( rc32 != PSCI_E_SUCCESS )  {
				SMC_RET1((uint64_t)cm_get_context(SECURE), TOPPERS_SMC_RET_UNEXPECTED_ERROR);
			}

			SMC_RET1((uint64_t)cm_get_context(SECURE), TOPPERS_SMC_RET_SUCCESS);
		}
		else  {
			SMC_RET1((uint64_t)cm_get_context(NON_SECURE), TOPPERS_SMC_RET_INVALID_STATE);
		}
		break;

	/*
	 * Change the secure state.
	 *
	 * Parameter (Registers) :
	 * 	 r0/x0	SMC Function ID, TOPPERS_SMC_SWITCH_SECURE_STATE
	 *
	 * Return value:
	 *   r0/x0	TOPPERS_SMC_RET_SUCCESS -       Success
	 *   		TOPPERS_SMC_RET_INVALID_STATE - Called from no-permmison state
	 */
	case TOPPERS_SMC_SWITCH_SECURE_STATE:
	#ifdef DEBUG_SPD
		INFO("[ATF] SMC Func: TOPPERS_SMC_SWITCH_SECURE_STATE\n");
	#endif /* DEBUG_SPD */

		if( safeg_check_perm(SAFEG_SYSCALL_ID__SWITCH, current_sec) == false )  {
			SMC_RET1((uint64_t)cm_get_context(current_sec), TOPPERS_SMC_RET_INVALID_STATE);
		}

		if( current_sec == SECURE )  {
			/* Save secure context */
		#if    CTX_INCLUDE_FPREGS
			fpregs_context_save(get_fpregs_ctx(cm_get_context(SECURE)));
		#endif /* CTX_INCLUDE_FPREGS */
			cm_el1_sysregs_context_save(SECURE);
			toppers_el1_gicregs_context_save(SECURE);

			/* Switch to non-secure state */
			cm_el1_sysregs_context_restore(NON_SECURE);
			toppers_el1_gicregs_context_restore(NON_SECURE);
		#if    CTX_INCLUDE_FPREGS
			fpregs_context_restore(get_fpregs_ctx(cm_get_context(NON_SECURE)));
		#endif /* CTX_INCLUDE_FPREGS */
			cm_set_next_eret_context(NON_SECURE);

			SMC_RET0((uint64_t)cm_get_context(NON_SECURE));
		}
		else  {
			assert(handle == cm_get_context(NON_SECURE));
			/* Save non-secure context */
		#if    CTX_INCLUDE_FPREGS
			fpregs_context_save(get_fpregs_ctx(cm_get_context(NON_SECURE)));
		#endif /* CTX_INCLUDE_FPREGS */
			cm_el1_sysregs_context_save(NON_SECURE);
			toppers_el1_gicregs_context_save(NON_SECURE);

			/* Switch to secure state */
			cm_el1_sysregs_context_restore(SECURE);
			toppers_el1_gicregs_context_restore(SECURE);
		#if    CTX_INCLUDE_FPREGS
			fpregs_context_restore(get_fpregs_ctx(cm_get_context(SECURE)));
		#endif /* CTX_INCLUDE_FPREGS */
			cm_set_next_eret_context(SECURE);

			SMC_RET0((uint64_t)cm_get_context(SECURE));
		}
		break;

	/*
	 * Generate secure interrupt to Secure state
	 *
	 * Parameter (Registers) :
	 * 	 r0/x0	SMC Function ID, TOPPERS_SMC_GEN_INT
	 * 	 r1/x1  IRQ number
	 *
	 * Return value:
	 *   r0/x0	TOPPERS_SMC_RET_SUCCESS -       Success
	 *   		TOPPERS_SMC_RET_INVALID_STATE - Called from non-secure state
	 *   		TOPPERS_SMC_RET_PARAM_ERROR -   Paramter error
	 */
	case TOPPERS_SMC_GEN_INT:
	#ifdef DEBUG_SPD
		INFO("[ATF] SMC Func: TOPPERS_SMC_GEN_INT\n");
	#endif /* DEBUG_SPD */

		if( current_sec == NON_SECURE )  {
			rc32 = plat_gen_int((uint32_t)x1);
		}
		else  {
			rc32 = TOPPERS_SMC_RET_INVALID_STATE;
		}
		SMC_RET1((uint64_t)cm_get_context(current_sec), rc32);
		break;

#if		MONITOR_ENABLE_SYSCALLS == true
	/*
	 * Obtains the ID of a system call
	 *
	 * Parameter (Registers) :
	 * 	 r0/x0	SMC Function ID, TOPPERS_SMC_GETID
	 * 	 r1/x1  System call name
	 *
	 * Return value:
	 *   r0/x0	TOPPERS_SMC_RET_SUCCESS -       Success
	 *   		TOPPERS_SMC_RET_INVALID_STATE - Called from no-permmison state
	 *   		TOPPERS_SMC_RET_PARAM_ERROR -   Paramter error
	 * 	 r1/x1  System call ID
	 */
	case TOPPERS_SMC_GETID:
	#ifdef DEBUG_SPD
		INFO("[ATF] SMC Func: TOPPERS_SMC_GETID\n");
	#endif /* DEBUG_SPD */
		rc32 = safeg_ssc_getid(current_sec, (char *)x1, &sc_id);
		SMC_RET2((uint64_t)cm_get_context(current_sec), rc32, sc_id);
		break;

	/*
	 * Set permissions for a certain system call
	 *
	 * Parameter (Registers) :
	 * 	 r0/x0	SMC Function ID, TOPPERS_SMC_SETPERM
	 * 	 r1/x1  System call ID
	 *	 r2/x2  Execution permisson of secure state
	 *	 r3/x3  Execution permisson of non secure state
	 *
	 * Return value:
	 *   r0/x0	TOPPERS_SMC_RET_SUCCESS -       Success
	 *   		TOPPERS_SMC_RET_INVALID_STATE - Called from no-permmison state
	 *   		TOPPERS_SMC_RET_PARAM_ERROR -   Paramter error
	 */
	case TOPPERS_SMC_SETPERM:
	#ifdef DEBUG_SPD
		INFO("[ATF] SMC Func: TOPPERS_SMC_SETPERM\n");
	#endif /* DEBUG_SPD */
		rc32 = safeg_ssc_setperm(current_sec, x1, x2, x3);
		SMC_RET1((uint64_t)cm_get_context(current_sec), rc32);
		break;

	/*
	 * Signals an interrupt to the opposite world
	 *
	 * Parameter (Registers) :
	 * 	 r0/x0	SMC Function ID, TOPPERS_SMC_GEN_INT
	 *
	 * Return value:
	 *   r0/x0	TOPPERS_SMC_RET_SUCCESS -       Success
	 *   		TOPPERS_SMC_RET_INVALID_STATE - Called from no-permmison state
	 */
	case TOPPERS_SMC_SIGNAL:
	#ifdef DEBUG_SPD
		INFO("[ATF] SMC Func: TOPPERS_SMC_SIGNAL\n");
	#endif /* DEBUG_SPD */
		rc32 = safeg_ssc_signal(current_sec);
		SMC_RET1((uint64_t)cm_get_context(current_sec), rc32);
		break;

	/*
	 * Register a dynamic system call
	 *
	 * Parameter (Registers) :
	 * 	 r0/x0	SMC Function ID, TOPPERS_SMC_REGDYN
	 *   r1/x1  Dynamic system call and initial parameters
	 *
	 * Return value:
	 *   r0/x0	TOPPERS_SMC_RET_SUCCESS -       Success
	 *   		TOPPERS_SMC_RET_INVALID_STATE - Called from no-permmison state
	 *   		TOPPERS_SMC_RET_PARAM_ERROR -   Paramter error
	 *   		TOPPERS_SMC_RET_FULL -          Already registered maximum number of dynamic system call
	 *   r1/x1  System call ID of registered dynamic system call
	 */
	case TOPPERS_SMC_REGDYN:
	#ifdef DEBUG_SPD
		INFO("[ATF] SMC Func: TOPPERS_SMC_REGDYN\n");
	#endif /* DEBUG_SPD */
		rc32 = safeg_ssc_regdyn(current_sec, (smc_syscall_t *)x1, &sc_id);
		SMC_RET2((uint64_t)cm_get_context(current_sec), rc32, sc_id);
		break;

	/*
	 * Read specified address from EL3
	 *
	 * Parameter (Registers) :
	 * 	 r0/x0	SMC Function ID, TOPPERS_SMC_READL
	 * 	 r1/x1  Read address
	 *
	 * Return value:
	 *   r0/x0	TOPPERS_SMC_RET_SUCCESS -       Success
	 *   		TOPPERS_SMC_RET_INVALID_STATE - Called from no-permmison state
	 *   		TOPPERS_SMC_RET_PARAM_ERROR -   Paramter error.
	 *   r1/x1	Read data
	 */
	case TOPPERS_SMC_READL:
	#ifdef DEBUG_SPD
		INFO("[ATF] SMC Func: TOPPERS_SMC_READL\n");
	#endif /* DEBUG_SPD */
		rc32 = safeg_ssc_readl(current_sec, (volatile uint32_t *)x1, &rdata);
		SMC_RET2((uint64_t)cm_get_context(current_sec), rc32, rdata);
		break;

	/*
	 * Write specified address from EL3
	 *
	 * Parameter (Registers) :
	 * 	 r0/x0	SMC Function ID, TOPPERS_SMC_WRITEL
	 * 	 r1/x1  Write address (Flat model)
	 * 	 r2/x2  Write data
	 *
	 * Return value:
	 *   r0/x0	TOPPERS_SMC_RET_SUCCESS -       Success
	 *   		TOPPERS_SMC_RET_INVALID_STATE - Called from no-permmison state
	 *   		TOPPERS_SMC_RET_PARAM_ERROR -   Paramter error.
	 */
	case TOPPERS_SMC_WRITEL:
	#ifdef DEBUG_SPD
		INFO("[ATF] SMC Func: TOPPERS_SMC_WRITEL\n");
	#endif /* DEBUG_SPD */
		rc32 = safeg_ssc_writel(current_sec, (volatile uint32_t *)x1, (uint32_t)x2);
		SMC_RET1((uint64_t)cm_get_context(current_sec), rc32);
		break;

	/*
	 * RESTARNT: restart NT OS
	 *
	 * Parameter (Registers) :
	 * 	 r0/x0	SMC Function ID, TOPPERS_SMC_RESTARNT
	 *
	 * Return value:
	 *   r0/x0	TOPPERS_SMC_RET_SUCCESS -       Success
	 *   		TOPPERS_SMC_RET_INVALID_STATE - Called from no-permmison state
	 *			TOPPERS_SMC_RET_NOT_SUPPORTED - Not supported (No implemented)
	 */
	case TOPPERS_SMC_RESTARNT:
	#ifdef DEBUG_SPD
		INFO("[ATF] SMC Func: TOPPERS_SMC_RESTARNT\n");
	#endif /* DEBUG_SPD */

		if( safeg_check_perm(SAFEG_SYSCALL_ID__RESTARTNT, current_sec) == false )  {
			SMC_RET1((uint64_t)cm_get_context(current_sec), TOPPERS_SMC_RET_INVALID_STATE);
		}

		if( current_sec == SECURE )  {
			/* If you need this function, you have to implement this. */
			rc32 = TOPPERS_SMC_RET_NOT_SUPPORTED;
		}
		else  {
			rc32 = TOPPERS_SMC_RET_INVALID_STATE;
		}
		SMC_RET1((uint64_t)cm_get_context(current_sec), rc32);
		break;

	/*
	 * INVOKE: Invoke a dynamic system call
	 *
	 * Parameter (Registers) :
	 * 	 r0/x0	SMC Function ID, TOPPERS_SMC_INVOKE
	 * 	 r1/x1	System call ID
	 * 	 r2/x2	1st parameter
	 * 	 r3/x3	2nd parameter
	 * 	 r4/x4	3rd parameter
	 *
	 * Return value:
	 *   r0/x0	Return code of a dynamic system call
	 */
	case TOPPERS_SMC_INVOKE:
	#ifdef DEBUG_SPD
		INFO("[ATF] SMC Func: TOPPERS_SMC_INVOKE\n");
	#endif /* DEBUG_SPD */
		rc32 = safeg_ssc_invoke(current_sec, x1, x2, x3, x4);
		SMC_RET1((uint64_t)cm_get_context(current_sec), rc32);
		break;
#endif	/* MONITOR_ENABLE_SYSCALLS == true */

	default:
		SMC_RET1((uint64_t)cm_get_context(current_sec), TOPPERS_SMC_RET_NOT_SUPPORTED);
		break;
	}

	return 0;
}


/* Define an TOPPERS kernel runtime service descriptor for fast SMC calls */
DECLARE_RT_SVC(
	toppers_fast,

	OEN_TOS_START,
	OEN_TOS_END,
	SMC_TYPE_FAST,
	toppers_setup,
	toppers_smc_handler
);
