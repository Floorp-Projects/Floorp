/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "uiapi.h"
#include "mozilla/Assertions.h"

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

// used in early init code where config has not been setup
const boolean gHardCodeSDPMode = TRUE;
boolean gStopTickTask = FALSE;


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

cprMsgQueue_t ccapp_msgq = NULL;
cprThread_t ccapp_thread = NULL;

cprMsgQueue_t sip_msgq = NULL;
cprThread_t sip_thread = NULL;
#ifdef NO_SOCKET_POLLING
cprThread_t sip_msgqwait_thread = NULL;
#endif

cprMsgQueue_t gsm_msgq = NULL;
cprThread_t gsm_thread = NULL;

cprMsgQueue_t misc_app_msgq = NULL;
cprThread_t misc_app_thread = NULL;

#ifdef JINDO_DEBUG_SUPPORTED
cprMsgQueue_t debug_msgq = NULL;
cprThread_t debug_thread = NULL;
#endif

#ifdef EXTERNAL_TICK_REQUIRED
cprMsgQueue_t ticker_msgq = NULL;
cprThread_t ticker_thread = NULL;
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

    TNP_DEBUG(DEB_F_PREFIX"started init of SIP call control", DEB_F_PREFIX_ARGS(SIP_CC_INIT, "ccInit"));

    platInit();

    strlib_init();

    /* Initialize threads, queues etc. */
    (void) thread_init();

    platform_initialized = TRUE;

    return 0;
}

static int
thread_init ()
{
    gStopTickTask = FALSE;
    /*
     * This will have already been called for CPR CNU code,
     * but may be called here for Windows emulation.
     */
    (void) cprPreInit();


    PHNChangeState(STATE_FILE_CFG);

    /*
     * Initialize the command parser and debug infrastructure
     */
    debugInit();

    CCApp_prepare_task();
    GSM_prepare_task();

    config_init();
    vcmInit();

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
    TNP_DEBUG(DEB_F_PREFIX"Ticker Task initialized..", DEB_F_PREFIX_ARGS(SIP_CC_INIT, "TickerTask"));
    while (FALSE == gStopTickTask) {
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
    gsm_set_initialized();
    PHNChangeState(STATE_CONNECTED);
    ui_set_sip_registration_state(CC_ALL_LINES, TRUE);
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
    cprBuffer_t  msg;
    int  sdpmode = 0;

    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));

    DEF_DEBUG(DEB_F_PREFIX"send Unload message to %s task ..",
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

            if (!sdpmode) {
                cprSleep(2000);
            }
            /* send a unload message to the SIP Task to kill sip thread*/
            msg =  SIPTaskGetBuffer(len);
            if (msg == NULL) {
                CSFLogError("common",
                  "%s: failed to allocate sip msg buffer\n", fname);
                return;
            }

            if (SIPTaskSendMsg(THREAD_UNLOAD, (cprBuffer_t)msg, len, NULL) == CPR_FAILURE)
            {
                cpr_free(msg);
                CSFLogError("common",
                  "%s: Unable to send THREAD_UNLOAD msg to sip thread", fname);
            }
        }
        break;
        case CC_SRC_GSM:
        {
            msg =  gsm_get_buffer(len);
            if (msg == NULL) {
                CSFLogError("common",
                  "%s: failed to allocate  gsm msg cprBuffer_t\n", fname);
                return;
            }
            if (CPR_FAILURE == gsm_send_msg(THREAD_UNLOAD, msg, len)) {
                CSFLogError("common",
                  "%s: Unable to send THREAD_UNLOAD msg to gsm thread", fname);
            }
        }
        break;
        case CC_SRC_MISC_APP:
        {
            msg = cpr_malloc(len);
            if (msg == NULL) {
                CSFLogError("common",
                  "%s: failed to allocate  misc msg cprBuffer_t\n", fname);
                return;
            }
            if (CPR_FAILURE == MiscAppTaskSendMsg(THREAD_UNLOAD, msg, len)) {
                CSFLogError("common",
                  "%s: Unable to send THREAD_UNLOAD msg to Misc App thread",
                  fname);
            }
        }
        break;
        case CC_SRC_CCAPP:
        {
            msg = cpr_malloc(len);
            if (msg == NULL) {
                CSFLogError("common",
                  "%s: failed to allocate  ccapp msg cprBuffer_t\n", fname);
                return;
            }
            if (ccappTaskPostMsg(CCAPP_THREAD_UNLOAD, msg, len, CCAPP_CCPROVIER) == CPR_FAILURE )
            {
                CSFLogError("common",
                  "%s: Unable to send THREAD_UNLOAD msg to CCapp thread",
                  fname);
            }
            CSFLogError("common", "%s:  send UNLOAD msg to CCapp thread good",
              fname);
            /* Unlike other similar functions, ccappTaskPostMsg does not take
             * ownership. */
            cpr_free(msg);
            msg = NULL;
        }
        break;

        default:
            CSFLogError("common", "%s: Unknown destination task passed=%d.",
              fname, dest_id);
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

    DEF_DEBUG(DEB_F_PREFIX"ccUnload called..", DEB_F_PREFIX_ARGS(SIP_CC_INIT, fname));
    if (platform_initialized == FALSE)
    {
        TNP_DEBUG(DEB_F_PREFIX"system is not loaded, ignore unload", DEB_F_PREFIX_ARGS(SIP_CC_INIT, fname));
        return;
    }
    /*
     * We are going to send an unload msg to each of the thread, which on
     * receiving the msg, will notify us back with async dispatch before
     * killing itself. Main thread is assumed here. We have to do this to
     * avoid deadlock because the threads use SyncRunnable to main.
    */
    send_task_unload_msg(CC_SRC_SIP);
    send_task_unload_msg(CC_SRC_GSM);

    if (!gHardCodeSDPMode) {
    	send_task_unload_msg(CC_SRC_MISC_APP);
    }

    send_task_unload_msg(CC_SRC_CCAPP);

    gStopTickTask = TRUE;
}

