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
 *  @(#) $Id: toppers_config.h 90 2021-05-06 07:01:33Z aicp-suzuki $
 */

#ifndef __TOPPERS_CONFIG_H__
#define __TOPPERS_CONFIG_H__

#ifdef SPD_toppers

/*
 * TOPPERSカーネルの設定
 */
/* アドレス配置 */
/*   格納アドレス */
#define TOPPERS_BASE			0xBE000000
/*   サイズ */
#define TOPPERS_SIZE			0x01000000

/* コンソール用UART */
#define NONE_UART				0
#define IMX8MM_UART1			1
#define IMX8MM_UART2			2
#define IMX8MM_UART3			3
#define IMX8MM_UART4			4

#define TOPPERS_CONSOLE_UART	IMX8MM_UART4

/*
 *  SafeG互換システムコール設定
 */
/* Trap NS-EL1 interrupt to EL3 */
#define MONITOR_TRAP_NT_IRQS		false
/* Enable SafeG monitor call */
#define MONITOR_ENABLE_SYSCALLS		true
/* Maximum number of notifiers */
#define MONITOR_MAX_NOTIFIERS		32
/* Maximum number of dynamic system call */
#define MONITOR_MAX_DYN_SYSCALLS	32

/* BL32 disable */
#define MONITOR_BL32_DISABLE		false

/* BL32 アクセス設定 */
/*   BL32メモリ空間をEL3でアクセス可能にする */
//#define MONITOR_ACCESS_BL32_ENABLE
/*   アクセス領域 */
#define MONITOR_ACCESS_BL32_CODE_BASE	0xBE7F0000
#define MONITOR_ACCESS_BL32_CODE_SIZE	0x00010000
#define MONITOR_ACCESS_BL32_DATA_BASE	0xBE800000
#define MONITOR_ACCESS_BL32_DATA_SIZE	0x00010000

/* */

/* Debug message output. if this macro is defined, debug messages are output */
//#define DEBUG_SPD

#endif	/* SPD_toppers */

#endif	/* __TOPPERS_CONFIG_H__ */
