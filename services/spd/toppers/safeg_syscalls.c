/*
 *  TOPPERS/SafeG64 Dual-OS monitor
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Safety Gate Dual-OS monitor
 *
 * Copyright (C) 2018-2019 by Embedded and Real-Time Systems Laboratory
 *               Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 *  上記著作権者は，以下の(1)～(4)の条件を満たす場合に限り，本ソフトウェ
 *  ア（本ソフトウェアを改変したものを含む．以下同じ）を使用・複製・改
 *  変・再配布（以下，利用と呼ぶ）することを無償で許諾する．
 *  (1) 本ソフトウェアをソースコードの形で利用する場合には，上記の著作
 *      権表示，この利用条件および下記の無保証規定が，そのままの形でソー
 *      スコード中に含まれていること．
 *  (2) 本ソフトウェアを，ライブラリ形式など，他のソフトウェア開発に使
 *      用できる形で再配布する場合には，再配布に伴うドキュメント（利用
 *      者マニュアルなど）に，上記の著作権表示，この利用条件および下記
 *      の無保証規定を掲載すること．
 *  (3) 本ソフトウェアを，機器に組み込むなど，他のソフトウェア開発に使
 *      用できない形で再配布する場合には，次のいずれかの条件を満たすこ
 *      と．
 *    (a) 再配布に伴うドキュメント（利用者マニュアルなど）に，上記の著
 *        作権表示，この利用条件および下記の無保証規定を掲載すること．
 *    (b) 再配布の形態を，別に定める方法によって，TOPPERSプロジェクトに
 *        報告すること．
 *  (4) 本ソフトウェアの利用により直接的または間接的に生じるいかなる損
 *      害からも，上記著作権者およびTOPPERSプロジェクトを免責すること．
 *      また，本ソフトウェアのユーザまたはエンドユーザからのいかなる理
 *      由に基づく請求からも，上記著作権者およびTOPPERSプロジェクトを
 *      免責すること．
 *
 *  本ソフトウェアは，無保証で提供されているものである．上記著作権者お
 *  よびTOPPERSプロジェクトは，本ソフトウェアに関して，特定の使用目的
 *  に対する適合性も含めて，いかなる保証も行わない．また，本ソフトウェ
 *  アの利用により直接的または間接的に生じたいかなる損害に関しても，そ
 *  の責任を負わない．
 *
 *  @(#) $Id: safeg_syscalls.c 82 2020-07-14 11:51:29Z ertl-honda $
 */

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

#if		MONITOR_ENABLE_SYSCALLS == true

/*
 * Wrapper for declaring a static system call.
 */
#define STATIC_SYSCALL(suffix, t, nt)				\
{													\
	.is_used		= true,							\
	.is_t_callable  = (t),							\
	.is_nt_callable = (nt),							\
	.name		    = #suffix,						\
	.function	    = NULL							\
}

safeg_syscall_t	the_syscalls[SAFEG_NUM_SYSCALL] = {
	STATIC_SYSCALL(getid,   true, true),	// id=0
	STATIC_SYSCALL(setperm, true, false),	// id=1
	STATIC_SYSCALL(switch,  true, true),	// id=2
	STATIC_SYSCALL(signal,  true, true),	// id=3
	STATIC_SYSCALL(regdyn,  true, false),	// id=4
	STATIC_SYSCALL(regnot,  true, false),	// id=5
	STATIC_SYSCALL(readl,   true, true),	// id=6
	STATIC_SYSCALL(writel,  true, true),	// id=7
	STATIC_SYSCALL(restarnt,true, false),	// id=8
};


/*
 * safeg_syscalls_init()
 *
 * Function :
 *    Initialize SafeG system calls
 *
 * Arguments :
 *   None
 *
 * Return value :
 *   None
 */
void
safeg_syscalls_init(void)
{
	uint32_t	cnt;

	for( cnt = SAFEG_NUM_STATIC_SYSCALL ; cnt < SAFEG_NUM_SYSCALL ; cnt++ )  {
		the_syscalls[cnt].is_used = false;
	}
}


/*
 * safeg_ssc_getid()
 *
 * Function :
 *    Obtains the ID of a system call
 *
 * Arguments :
 * 	 [In]  current_sec :     Current security state (SECURE, NON_SECURE)
 *	 [In]  sc_name :         System call name
 * 	 [In]  sc_id :           System call ID
 *
 * Return value:
 *   System call return code
 */
uint32_t
safeg_ssc_getid(uint32_t current_sec, char *sc_name, uint32_t *sc_id)
{
	uint32_t	tmp_id;

	if( safeg_check_perm(SAFEG_SYSCALL_ID__GETID, current_sec) == false )  {
		return TOPPERS_SMC_RET_INVALID_STATE;
	}

    if( !safeg_is_valid_memory(current_sec, (uint64_t)sc_name, SAFEG_SYSCALL_NAME_LENGTH) )  {
		return TOPPERS_SMC_RET_PARAM_ERROR;
	}

	/* look up the string in the system calls table */
	for( tmp_id = 0 ; tmp_id < SAFEG_NUM_SYSCALL ; tmp_id++ )  {
		if( strncmp((char *)(the_syscalls[tmp_id].name), sc_name, SAFEG_SYSCALL_NAME_LENGTH) == 0 )  {
			break;
		}
	}

	if( tmp_id == SAFEG_NUM_SYSCALL )  {
		return TOPPERS_SMC_RET_INEXISTENT;
	}

	*sc_id = tmp_id;

	return TOPPERS_SMC_RET_SUCCESS;
}


/*
 * safeg_ssc_setperm()
 *
 * Function :
 *    Set permissions for a certain system call
 *
 * Arguments :
 * 	 [In]  current_sec :     Current security state (SECURE, NON_SECURE)
 * 	 [In]  sc_id :           System call ID
 *	 [In]  is_t_callable :   Execution permisson of secure state
 *	 [In]  is_nt_callable :  Execution permisson of non secure state
 *
 * Return value:
 *   System call return code
 */
uint32_t
safeg_ssc_setperm(uint32_t current_sec, uint32_t sc_id, uint32_t is_t_callable, uint32_t is_nt_callable)
{
	if( safeg_check_perm(SAFEG_SYSCALL_ID__SETPERM, current_sec) == false )  {
		return TOPPERS_SMC_RET_INVALID_STATE;
	}

	if( sc_id >= SAFEG_NUM_SYSCALL )  {
		return TOPPERS_SMC_RET_PARAM_ERROR;
	}

	the_syscalls[sc_id].is_t_callable =  is_t_callable;
	the_syscalls[sc_id].is_nt_callable = is_nt_callable;

	return TOPPERS_SMC_RET_SUCCESS;
}


/*
 * safeg_ssc_signal()
 *
 * Function :
 *   Signals an interrupt to the opposite world
 *
 * Arguments :
 * 	 [In]  current_sec :     Current security state (SECURE, NON_SECURE)
 *
 * Return value:
 *   System call return code
 */
uint32_t
safeg_ssc_signal(uint32_t current_sec)
{
	uint32_t	linear_id = plat_my_core_pos();
	uint32_t	ret, int_no;

	if( safeg_check_perm(SAFEG_SYSCALL_ID__SIGNAL, current_sec) == false )  {
		return TOPPERS_SMC_RET_INVALID_STATE;
	}

	int_no = safeg_get_signal_intno(linear_id, current_sec);

	ret = plat_gen_int(int_no);
	if( ret != TOPPERS_SMC_RET_SUCCESS )  {
		return TOPPERS_SMC_RET_UNEXPECTED_ERROR;
	}

	return TOPPERS_SMC_RET_SUCCESS;
}


/*
 * safeg_ssc_regdyn()
 *
 * Function :
 *    Register a dynamic system call
 *
 * Arguments :
 * 	 [In]  current_sec :     Current security state (SECURE, NON_SECURE)
 * 	 [In]  dsc_context :     Dynamic system call and initial parameters
 *   [Out] reg_sc_id :       System call ID of registered dynamic system call
 *
 * Return value:
 *   System call return code
 */
uint32_t
safeg_ssc_regdyn(uint32_t current_sec, smc_syscall_t *dsc_context, uint32_t *reg_sc_id)
{
	uint32_t	tmp_id;

	*reg_sc_id = 0;

	if( safeg_check_perm(SAFEG_SYSCALL_ID__REGDYN, current_sec) == false )  {
		return TOPPERS_SMC_RET_INVALID_STATE;
	}

    if( !safeg_is_valid_memory(current_sec, (uint64_t)dsc_context, sizeof(safeg_syscall_t)) )  {
		return TOPPERS_SMC_RET_PARAM_ERROR;
	}

	/* Get a free syscall id */
	for( tmp_id = 0 ; tmp_id < SAFEG_NUM_SYSCALL ; tmp_id++)  {
		if( !the_syscalls[tmp_id].is_used )  {
			break;
		}
	}

	/* Return error if no slots available */
	if( tmp_id == SAFEG_NUM_SYSCALL )  {
		return TOPPERS_SMC_RET_FULL;
	}

	/* copy the system call information */
	the_syscalls[tmp_id].is_t_callable  = dsc_context->is_t_callable;
	the_syscalls[tmp_id].is_nt_callable = dsc_context->is_nt_callable;
	the_syscalls[tmp_id].function       = dsc_context->function;
	memcpy(the_syscalls[tmp_id].name, dsc_context->name, SAFEG_SYSCALL_NAME_LENGTH);

	/* Flag it as used after a memory barrier to avoid race conditions */
	dmbsy();
	the_syscalls[tmp_id].is_used = true;

	/* Set the id */
	*reg_sc_id = tmp_id;

	return TOPPERS_SMC_RET_SUCCESS;
}


/*
 * safeg_ssc_readl()
 *
 * Function :
 *    Read specified address from EL3
 *
 * Arguments :
 * 	 [In]  current_sec :     Current security state (SECURE, NON_SECURE)
 * 	 [In]  address :         Read address (Flat model)
 * 	 [Out] rdata_ref :       Read data
 *
 * Return value:
 *   System call return code
 */
uint32_t
safeg_ssc_readl(uint32_t current_sec, volatile uint32_t *address, uint32_t *rdata_ref)
{
	uint32_t	linear_id = plat_my_core_pos();
	*rdata_ref = 0;

	if( safeg_check_perm(SAFEG_SYSCALL_ID__READL, current_sec) == false )  {
		return TOPPERS_SMC_RET_INVALID_STATE;
	}

	if( !safeg_is_valid_readl_address(linear_id, current_sec, (uint64_t)address) )  {
		return TOPPERS_SMC_RET_PARAM_ERROR;
	}

	*rdata_ref = *address;

	return TOPPERS_SMC_RET_SUCCESS;
}


/*
 * safeg_ssc_writel()
 *
 * Function :
 *    Write specified address from EL3
 *
 * Arguments :
 * 	 [In]  current_sec :     Current security state (SECURE, NON_SECURE)
 * 	 [In]  address :         Write address (Flat model)
 * 	 [In]  wdata :           Write data
 *
 * Return value:
 *   System call return code
 */
uint32_t
safeg_ssc_writel(uint32_t current_sec, volatile uint32_t *address, uint32_t wdata)
{
	uint32_t	linear_id = plat_my_core_pos();

	if( safeg_check_perm(SAFEG_SYSCALL_ID__READL, current_sec) == false )  {
		return TOPPERS_SMC_RET_INVALID_STATE;
	}

	if( !safeg_is_valid_readl_address(linear_id, current_sec, (uint64_t)address) )  {
		return TOPPERS_SMC_RET_PARAM_ERROR;
	}

	*address = wdata;

	return TOPPERS_SMC_RET_SUCCESS;
}


/*
 * safeg_ssc_invoke()
 *
 * Function :
 *   Invoke a dynamic system call
 *
 * Arguments :
 * 	 [In]  current_sec :     Current security state (SECURE, NON_SECURE)
 *   [In]  sc_id :           System call ID
 * 	 [I/O] param1 : 	     1st parameter
 * 	 [I/O] param2 : 	     2nd parameter
 * 	 [I/O] param3 : 	     3rd parameter
 *
 * Return value:
 *   System call return code
 */
uint32_t
safeg_ssc_invoke(uint32_t current_sec, uint32_t sc_id, uint64_t param1, uint64_t param2, uint64_t param3)
{
	uint32_t		linear_id = plat_my_core_pos();
	safeg_syscall_t *dsc_context;

	/* Parameter checking */
	if( sc_id >= SAFEG_NUM_SYSCALL )  {
		return TOPPERS_SMC_RET_PARAM_ERROR;
	}

	dsc_context = &the_syscalls[sc_id];

	if( dsc_context->function == NULL )  {
		return TOPPERS_SMC_RET_PARAM_ERROR;
	}

	if( dsc_context->is_used == false )  {
		return TOPPERS_SMC_RET_INEXISTENT;
	}

	if( ((current_sec == SECURE) && (dsc_context->is_t_callable != true)) ||
        ((current_sec == NON_SECURE) && (dsc_context->is_nt_callable != true)) )  {
		return TOPPERS_SMC_RET_INVALID_STATE;
	}

	/* call the appropriate system call function */
	return dsc_context->function(linear_id, current_sec, param1, param2, param3);
}

#endif	/* MONITOR_ENABLE_SYSCALLS == true */
