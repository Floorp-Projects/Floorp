/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "cpr.h"
#include "cpr_in.h"
#include "cpr_stdlib.h"
#include "cpr_ipc.h"
#include "phntask.h"
#include <stdarg.h>
#include "configmgr.h"
#include "debug.h"
#include "config.h"
#include "vcm.h"
#include "dialplan.h"
#include "debug.h"
#include "phone_debug.h"
#include "CCProvider.h"
#include "ccsip_task.h"
#include "gsm.h"
#include "misc_apps_task.h"
#include "plat_api.h"
#include "ccapp_task.h"

#include "phone_platform_constants.h"
/** The following defines are used to tune the total memory that pSIPCC
 * allocates and uses. */
/** Block size for emulated heap space, i.e. 1kB */
#define BLK_SZ  1024

/** 5 MB Heap Based on 0.5 MB initial use + 4 MB for calls (20K * 200) + 0.5 MB misc */
/** The number of supported blocks for the emulated heap space */
#define MEM_BASE_BLK 500 //500 blocks, ~ 0.5M
#define MEM_MISC_BLK 500 //500 blocks, ~ 0.5M
/** Size of the emulated heap space. This is the value passed to the memory
 * management pre-init procedure. The total memory allocated is
 * "PRIVATE_SYS_MEM_SIZE + 2*64"  where the additional numbers are for a gaurd
 * band. */
#define MEM_PER_CALL_BLK 20 //20 block, ~20k
#define PRIVATE_SYS_MEM_SIZE ((MEM_BASE_BLK + MEM_MISC_BLK + (MEM_PER_CALL_BLK) * MAX_CALLS) * BLK_SZ)

extern boolean cpr_memory_mgmt_pre_init(size_t size);
extern boolean cpr_memory_mgmt_post_init(void);
extern void cpr_memory_mgmt_destroy(void);

/**
 * Force a crash dump which will allow a stack trace to be generated
 *
 * @return none
 *
 * @note crash dump is created by an illegal write to memory
 */
extern void cpr_crashdump(void);


/*--------------------------------------------------------------------------
 * Local definitions
 *--------------------------------------------------------------------------
 */
#define  GSMSTKSZ   61440

/*
 * CNU thread queue sizes.
 *
 * On CNU, the message queue size can hold up to 31 entries only.
 * This is too small for a phone that can support the MAX_CALLS that
 * is close to or higher than 31 calls simultaneously. In a scenario when
 * the number of active calls on the phone reaches its MAC_CALLS then
 * there can be a potential MAX_CALLS that is great ther than 31 events on
 * a queue to GSM or SIP thread.
 *
 * One known scenario is a 7970 phone having 50 held calls where all
 * calls are not from the same CCM that this phone registers with but
 * still within the same cluster. If that CCM is restarted, the CCM that
 * this phone registers with will terminate these calls on the phone by
 * sending BYEs, SUBSCRIBE (to terminate) DTMF digi collection and etc. to
 * all these calls very quickly together. Handling these SIP messages
 * can generate many internal events between SIP and GSM threads including
 * events that are sent to self (such as GSM which includes applications
 * that runs as part of GSM). The amount of the events posted on the queues
 * during this time can be far exceeding than 31 and MAX_CALLS entries.
 *
 * The followings define queue sizes for GSM, SIP and the other threads.
 * The GSM's queue size is defined to be larger than SIP's queue size to
 * account for self sending event. The formular below is based on the
 * testing with the above scenario and with 8-3-x phone load. The maximum
 * queue depth observed for GSM under
 * this condition is 129 entries and the maximum queue depth of
 * SIP under the same condition is about 67 entries. Therefore, the queue
 * depth of GSM thread is given to 3 times MAX_CALLS (or 153) and
 * 2 times (or 102) for SIP thread for 7970 case.
 *
 */
#define  GSMQSZ        (MAX_CALLS*3) /* GSM message queue size           */
#define  SIPQSZ        (MAX_CALLS*2) /* SIP message queue size           */
#define  DEFQSZ         0            /* default message queue size       */
#define  DEFAPPQSZ     MAX_REG_LINES

/*--------------------------------------------------------------------------
 * Global data
 *--------------------------------------------------------------------------
 */

cprMsgQueue_t ccapp_msgq;
cprThread_t ccapp_thread;

cprMsgQueue_t sip_msgq;
cprThread_t sip_thread;
#ifdef NO_SOCKET_POLLING
cprThread_t sip_msgqwait_thread;
#endif

cprMsgQueue_t gsm_msgq;
cprThread_t gsm_thread;

cprMsgQueue_t misc_app_msgq;
cprThread_t misc_app_thread;

#ifdef JINDO_DEBUG_SUPPORTED
cprMsgQueue_t debug_msgq;
cprThread_t debug_thread;
#endif

#ifdef EXTERNAL_TICK_REQUIRED
cprMsgQueue_t ticker_msgq;
cprThread_t ticker_thread;
#endif

/* Platform initialized flag */
boolean platform_initialized = FALSE;

static int thread_init(void);

/*--------------------------------------------------------------------------
 * External data references
 * -------------------------------------------------------------------------
 */


/*--------------------------------------------------------------------------
 * External function prototypes
 *--------------------------------------------------------------------------
 */
extern void gsm_set_initialized(void);
extern void vcm_init(void);
extern void dp_init(void *);
extern cprBuffer_t SIPTaskGetBuffer(uint16_t size);

extern void sip_platform_task_loop(void *arg);
#ifdef NO_SOCKET_POLLING
extern void sip_platform_task_msgqwait(void *arg);
#endif
extern void GSMTask(void *);
#ifndef VENDOR_BUILD
extern void debug_task(void *);
#endif
extern void MiscAppTask(void *);
extern void cpr_timer_tick(void);

extern void cprTimerSystemInit(void);
extern int32_t ui_clear_mwi(int32_t argc, const char *argv[]);
void gsm_shutdown(void);
void dp_shutdown(void);
void MiscAppTaskShutdown(void);
void CCAppShutdown(void);
cprBuffer_t gsm_get_buffer (uint16_t size);




/*--------------------------------------------------------------------------
 * Local scope function prototypes
 *--------------------------------------------------------------------------
 */
#ifdef EXTERNAL_TICK_REQUIRED
int TickerTask(void *);
#endif

void send_protocol_config_msg(void);

/**
 * ccMemInit()
 */
extern 
int ccMemInit(size_t size) {
    /*
     * Do not move memory pre init below.
     * This initializes the memory sandbox
     * and must be first thing done here to make sure
     * allocations succeed.
     */
    if (cpr_memory_mgmt_pre_init(size) != TRUE) 
    {
        return CPR_FAILURE;
    }
    return CPR_SUCCESS;
}

/**
 * ccPreInit
 *
 * Initialization routine to call before any application level
 * code initializes.
 *
 * Parameters: None
 *
 * Return Value: CPR_SUCCESS or CPR_FAILURE
 */

int
ccPreInit ()
{
	static boolean ccPreInit_called = FALSE;

	if (ccPreInit_called == FALSE) {
		ccPreInit_called = TRUE;
		//Initializes the memory first
		ccMemInit(PRIVATE_SYS_MEM_SIZE);
		cprPreInit();
	}

    return CPR_SUCCESS;
}

int
ccInit ()
{

    TNP_DEBUG(DEB_F_PREFIX"started init of SIP call control\n", DEB_F_PREFIX_ARGS(SIP_CC_INIT, "ccInit"));

    platInit();

    /*
     * below should move to cprPreInit. keep it here until then
     */
#ifdef _WIN32
    cprTimerSystemInit();
#endif

    /* Initialize threads, queues etc. */
    (void) thread_init();

    platform_initialized = TRUE;

    return 0;
}

static int
thread_init ()
{
    /*
     * This will have already been called for CPR CNU code,
     * but may be called here for Windows emulation.
     */
    (void) cprPreInit();


    PHNChangeState(STATE_FILE_CFG);

    /* initialize message queues */
    sip_msgq = cprCreateMessageQueue("SIPQ", SIPQSZ);
    gsm_msgq = cprCreateMessageQueue("GSMQ", GSMQSZ);
    misc_app_msgq = cprCreateMessageQueue("MISCAPPQ", DEFQSZ);
    ccapp_msgq = cprCreateMessageQueue("CCAPPQ", DEFQSZ);
#ifdef JINDO_DEBUG_SUPPORTED
    debug_msgq = cprCreateMessageQueue("DEBUGAPPQ", DEFQSZ);
#endif
#ifdef EXTERNAL_TICK_REQUIRED
    ticker_msgq = cprCreateMessageQueue("Ticker", DEFQSZ);
#endif


    /*
     * Initialize the command parser and debug infrastructure
     */
    debugInit();

    /* initialize adapter level debugs */
    //debug_bind_keyword("sip-adapter", &TNPDebug);
//    bind_clear_keyword("mwi", &ui_clear_mwi);

    /* create threads */
    ccapp_thread = cprCreateThread("CCAPP Task",
                                 (cprThreadStartRoutine) CCApp_task,
                                 GSMSTKSZ, CCPROVIDER_THREAD_RELATIVE_PRIORITY /* pri */, ccapp_msgq);
    if (ccapp_thread == NULL) {
        err_msg("failed to create CCAPP task \n");
    }

#ifdef JINDO_DEBUG_SUPPORTED
#ifndef VENDOR_BUILD
    debug_thread = cprCreateThread("Debug Task",
                                   (cprThreadStartRoutine) debug_task, STKSZ,
                                   0 /*pri */ , debug_msgq);

    if (debug_thread == NULL) {
        err_msg("failed to create debug task\n");
    }
#endif
#endif

    /* SIP main thread */
    sip_thread = cprCreateThread("SIPStack task",
                                 (cprThreadStartRoutine) sip_platform_task_loop,
                                 STKSZ, SIP_THREAD_RELATIVE_PRIORITY /* pri */, sip_msgq);
    if (sip_thread == NULL) {
        err_msg("failed to create sip task \n");
    }

#ifdef NO_SOCKET_POLLING
    /* SIP message wait queue task */
    sip_msgqwait_thread = cprCreateThread("SIP MsgQueueWait task",
                                          (cprThreadStartRoutine)
                                          sip_platform_task_msgqwait,
                                          STKSZ, SIP_THREAD_RELATIVE_PRIORITY /* pri */, sip_msgq);
    if (sip_msgqwait_thread == NULL) {
        err_msg("failed to create sip message queue wait task\n");
    }
#endif

    gsm_thread = cprCreateThread("GSM Task",
                                 (cprThreadStartRoutine) GSMTask,
                                 GSMSTKSZ, GSM_THREAD_RELATIVE_PRIORITY /* pri */, gsm_msgq);
    if (gsm_thread == NULL) {
        err_msg("failed to create gsm task \n");
    }
    misc_app_thread = cprCreateThread("MiscApp Task",
                                      (cprThreadStartRoutine) MiscAppTask,
                                      STKSZ, 0 /* pri */, misc_app_msgq);
    if (misc_app_thread == NULL) {
        err_msg("failed to create MiscApp task \n");
    }

#ifdef EXTERNAL_TICK_REQUIRED
    ticker_thread = cprCreateThread("Ticker task",
                                    (cprThreadStartRoutine) TickerTask,
                                    STKSZ, 0, ticker_msgq);
    if (ticker_thread == NULL) {
        err_msg("failed to create ticker task \n");
    }
#endif

    /* Associate the threads with the message queues */
    (void) cprSetMessageQueueThread(sip_msgq, sip_thread);
    (void) cprSetMessageQueueThread(gsm_msgq, gsm_thread);
    (void) cprSetMessageQueueThread(misc_app_msgq, misc_app_thread);
    (void) cprSetMessageQueueThread(ccapp_msgq, ccapp_thread);
#ifdef JINDO_DEBUG_SUPPORTED
    (void) cprSetMessageQueueThread(debug_msgq, debug_thread);
#endif
#ifdef EXTERNAL_TICK_REQUIRED
    (void) cprSetMessageQueueThread(ticker_msgq, ticker_thread);
#endif

    /*
     * initialize debugs of other modules.
     *
     * dp_init needs the gsm_msgq id. This
     * is set in a global variable by the
     * GSM task running. However due to timing
     * issues dp_init is sometimes run before
     * the GSM task has set this variable resulting
     * in a NULL msgqueue ptr being passed to CPR
     * which returns an error and does not create
     * the dialplan timer. Thus pass is the same
     * data to dp_init that is passed into the
     * cprCreateThread call for GSM above.
     */
    config_init();
    vcmInit();
    dp_init(gsm_msgq);

    if (sip_minimum_config_check() != 0) {
        PHNChangeState(STATE_UNPROVISIONED);
    } else {
        PHNChangeState(STATE_CONNECTED);
    }

    (void) cprPostInit();

    if ( vcmGetVideoCodecList(VCM_DSP_FULLDUPLEX) ) {
        cc_media_update_native_video_support(TRUE);
    }

    return (0);
}


#ifdef EXTERNAL_TICK_REQUIRED

uint16_t SecTimer = 50;

unsigned long timeofday_in_seconds = 0;

void
MAIN0Timer (void)
{
    if (SecTimer-- == 0) {
        SecTimer = 50;
        timeofday_in_seconds++;
    }
    //gtick +=2;
    cpr_timer_tick();
}

int
TickerTask (void *a)
{
    TNP_DEBUG(DEB_F_PREFIX"Ticker Task initialized..\n", DEB_F_PREFIX_ARGS(SIP_CC_INIT, "TickerTask"));
    while (1) {
        cprSleep(20);
        MAIN0Timer();
    }
    return 0;
}
#endif

void
send_protocol_config_msg (void)
{
    const char *fname = "send_protocol_config_msg";
    char *msg;

    TNP_DEBUG(DEB_F_PREFIX"send TCP_DONE message to sip thread..\n", DEB_F_PREFIX_ARGS(SIP_CC_INIT, fname));

    msg = (char *) SIPTaskGetBuffer(4);
    if (msg == NULL) {
        TNP_DEBUG(DEB_F_PREFIX"failed to allocate message..\n", DEB_F_PREFIX_ARGS(SIP_CC_INIT, fname));
        return;
    }
    /* send a config done message to the SIP Task */
    if (SIPTaskSendMsg(TCP_PHN_CFG_TCP_DONE, msg, 0, NULL) == CPR_FAILURE) {
        err_msg("%s: notify SIP stack ready failed", fname);
        cprReleaseBuffer(msg);
    }
    gsm_set_initialized();
    PHNChangeState(STATE_CONNECTED);
}





/*
 *  Function: send_task_unload_msg
 *
 *  Description:
 *         - send shutdown and thread destroy msg to sip, gsm, ccapp, misc
 *           threads
 *  Parameters:  destination thread
 *
 *  Returns: none
 *
 */
void
send_task_unload_msg(cc_srcs_t dest_id)
{
    const char *fname = "send_task_unload_msg";
    uint16_t len = 4;
    cprBuffer_t  msg =  gsm_get_buffer(len);

    if (msg == NULL) {
        err_msg("%s: failed to allocate  msg cprBuffer_t\n", fname);
        return;
    }

    DEF_DEBUG(DEB_F_PREFIX"send Unload message to %s task ..\n",
        DEB_F_PREFIX_ARGS(SIP_CC_INIT, fname),
        dest_id == CC_SRC_SIP ? "SIP" :
        dest_id == CC_SRC_GSM ? "GSM" :
        dest_id == CC_SRC_MISC_APP ? "Misc App" :
        dest_id == CC_SRC_CCAPP ? "CCApp" : "Unknown");

    switch(dest_id) {
        case CC_SRC_SIP:
        {
            /* send this msg so phone can send unRegister msg */
            SIPTaskPostShutdown(SIP_EXTERNAL, CC_CAUSE_SHUTDOWN, "");
            /* allow unRegister msg to sent out and shutdown to complete */
            cprSleep(2000);
            /* send a unload message to the SIP Task to kill sip thread*/
            msg =  SIPTaskGetBuffer(len);
            if (msg == NULL) {
                err_msg("%s:%d: failed to allocate sip msg buffer\n", fname);
                return;
            }

            if (SIPTaskSendMsg(THREAD_UNLOAD, (cprBuffer_t)msg, len, NULL) == CPR_FAILURE)
            {
                cprReleaseBuffer(msg);
                err_msg("%s: Unable to send THREAD_UNLOAD msg to sip thread", fname);
            }
        }
        break;
        case CC_SRC_GSM:
        {
            msg =  gsm_get_buffer(len);
            if (msg == NULL) {
                err_msg("%s: failed to allocate  gsm msg cprBuffer_t\n", fname);
                return;
            }
            if (CPR_FAILURE == gsm_send_msg(THREAD_UNLOAD, msg, len)) {
                err_msg("%s: Unable to send THREAD_UNLOAD msg to gsm thread", fname);
            }
        }
        break;
        case CC_SRC_MISC_APP:
        {
            msg = cprGetBuffer(len);
            if (msg == NULL) {
                err_msg("%s: failed to allocate  misc msg cprBuffer_t\n", fname);
                return;
            }
            if (CPR_FAILURE == MiscAppTaskSendMsg(THREAD_UNLOAD, msg, len)) {
                err_msg("%s: Unable to send THREAD_UNLOAD msg to Misc App thread", fname);
            }
        }
        break;
        case CC_SRC_CCAPP:
        {
            msg = cprGetBuffer(len);
            if (msg == NULL) {
                err_msg("%s: failed to allocate  ccapp msg cprBuffer_t\n", fname);
                return;
            }
            if (ccappTaskPostMsg(CCAPP_THREAD_UNLOAD, msg, len, CCAPP_CCPROVIER) == CPR_FAILURE )
            {
                err_msg("%s: Unable to send THREAD_UNLOAD msg to CCapp thread", fname);
            }
            err_msg("%s:  send UNLOAD msg to CCapp thread good", fname);
        }
        break;

        default:
            err_msg("%s: Unknown destination task passed=%d.", fname, dest_id);
        break;
    }
}

/*
 *  Function: ccUnload
 *
 *  Description:
 *         - deinit portable runtime.
 *         - Cleanup call control modules, GSM and SIp Stack
 *
 *  Parameters: none
 *
 *  Returns: none
 *
 */
void
ccUnload (void)
{
    static const char fname[] = "ccUnload";

    DEF_DEBUG(DEB_F_PREFIX"ccUnload called..\n", DEB_F_PREFIX_ARGS(SIP_CC_INIT, fname));
    if (platform_initialized == FALSE) 
    {
        TNP_DEBUG(DEB_F_PREFIX"system is not loaded, ignore unload\n", DEB_F_PREFIX_ARGS(SIP_CC_INIT, fname));
        return;
    }
    /*
     * We are going to send an unload msg to each of the thread, which on
     * receiving the msg, will kill itself.
    */
    send_task_unload_msg(CC_SRC_SIP);
    send_task_unload_msg(CC_SRC_GSM);
    send_task_unload_msg(CC_SRC_MISC_APP);
    send_task_unload_msg(CC_SRC_CCAPP);
}

