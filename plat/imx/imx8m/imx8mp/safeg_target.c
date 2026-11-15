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
 *  @(#) $Id: safeg_target.c 82 2020-07-14 11:51:29Z ertl-honda $
 */

#include <arch.h>
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

#ifdef SPD_toppers

#define UNUSED(x) (x) __attribute__((__unused__))

#define GIC_SPI(x)  (x + 32)
/* Interrupt number (T-OS to NT-OS) */
#define TARGET_T_INT_SIGNAL_ID_0    GIC_SPI(56)
#define TARGET_T_INT_SIGNAL_ID_1    GIC_SPI(58)
#define TARGET_T_INT_SIGNAL_ID_2    GIC_SPI(60)
#define TARGET_T_INT_SIGNAL_ID_3    GIC_SPI(62)
/*   Interrupt number (NT-OS to T-OS) */
#define TARGET_NT_INT_SIGNAL_ID_0   GIC_SPI(57)
#define TARGET_NT_INT_SIGNAL_ID_1   GIC_SPI(59)
#define TARGET_NT_INT_SIGNAL_ID_2   GIC_SPI(61)
#define TARGET_NT_INT_SIGNAL_ID_3   GIC_SPI(63)


/*
 * safeg_get_signal_intno()
 *
 * Function :
 *    This function returns interrupt id for the callee
 *
 * Arguments :
 *   [In]  proc_id :     Current proccessor id
 *   [In]  current_sec : Current security state (SECURE, NON_SECURE)
 *
 * Return value :
 *   Interrupt id
 */
uint32_t
safeg_get_signal_intno(uint32_t proc_id, uint32_t current_sec)
{
    /* Interrupt number (T-OS to NT-OS) */
    static const uint32_t   s_signal_intno[PLATFORM_CORE_COUNT] = {
        TARGET_T_INT_SIGNAL_ID_0,       /* core 0 */
        TARGET_T_INT_SIGNAL_ID_1,       /* core 1 */
        TARGET_T_INT_SIGNAL_ID_2,       /* core 2 */
        TARGET_T_INT_SIGNAL_ID_3        /* core 3 */
    };
    /* Interrupt number (NT-OS to T-OS) */
    static const uint32_t   ns_signal_intno[PLATFORM_CORE_COUNT] = {
        TARGET_NT_INT_SIGNAL_ID_0,      /* core 0 */
        TARGET_NT_INT_SIGNAL_ID_1,      /* core 1 */
        TARGET_NT_INT_SIGNAL_ID_2,      /* core 2 */
        TARGET_NT_INT_SIGNAL_ID_3       /* core 3 */
    };

    if( proc_id >= PLATFORM_CORE_COUNT )  {
        return 0xFFFFFFFF;
    }

    if( current_sec == SECURE )  {
        return s_signal_intno[proc_id];
    }
    else  {
        return ns_signal_intno[proc_id];
    }
}

/*
 * safeg_is_valid_memory()
 *
 * Function :
 *   Check if the address is valid for access.
 *
 * Arguments :
 *   [In]  current_sec : Current security state (SECURE, NON_SECURE)
 *   [In]  address :     Start address of the target memory
 *   [In]  size :        Size of the target memory
 *
 * Return value :
 *   true  : This memory is valid
 *   false : This memory is invalid
 */
uint32_t
safeg_is_valid_memory(uint32_t UNUSED(current_sec),
                      uint64_t UNUSED(address),
                      uint64_t UNUSED(size))
{
    /* TODO: check memory */
    return true;
}

/*
 * safeg_is_valid_readl_address()
 *
 * Function :
 *   Check if the 4bytes memory is valid for read.
 *
 * Arguments :
 *   [In]  proc_id :     Current proccessor id
 *   [In]  current_sec : Current security state (SECURE, NON_SECURE)
 *   [In]  address :     Start address of the 4bytes memory
 *
 * Return value :
 *   true  : This memory is valid
 *   false : This memory is invalid
 */
uint32_t
safeg_is_valid_readl_address(uint32_t UNUSED(core_id),
                             uint32_t UNUSED(current_sec),
                             uint64_t UNUSED(address))
{
    /* TODO: check memory */
    return true;
}

/*
 * safeg_is_valid_writel_address()
 *
 * Function :
 *   Check if the 4bytes memory is valid for write.
 *
 * Arguments :
 *   [In]  proc_id :     Current proccessor id
 *   [In]  current_sec : Current security state (SECURE, NON_SECURE)
 *   [In]  address :     Start address of the 4bytes memory
 *
 * Return value :
 *   true  : This memory is valid
 *   false : This memory is invalid
 */
uint32_t
safeg_is_valid_writel_address(uint32_t UNUSED(core_id),
                              uint32_t UNUSED(current_sec),
                              uint64_t UNUSED(address))
{
    /* TODO: check memory */
    return true;
}

#endif  /* SPD_toppers */
