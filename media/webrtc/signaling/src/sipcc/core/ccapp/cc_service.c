/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cc_service.h"
#include "phone_debug.h"
#include "CCProvider.h"
#include "sessionConstants.h"
#include "ccsip_messaging.h"
#include "ccapp_task.h"

/**
 * External init.c methods
 */
extern int ccPreInit ();
extern int ccInit ();
extern int ccUnload ();
extern void protCfgTblInit();

extern cc_int32_t SipDebugMessage;
extern cc_int32_t SipDebugState;
extern cc_int32_t SipDebugTask;
extern cc_int32_t SipDebugRegState;
extern cc_int32_t GSMDebug;
extern cc_int32_t FIMDebug;
extern cc_int32_t LSMDebug;
extern cc_int32_t FSMDebugSM;
extern int32_t CSMDebugSM;
extern cc_int32_t CCDebug;
extern cc_int32_t CCDebugMsg;
extern cc_int32_t AuthDebug;
extern cc_int32_t ConfigDebug;
extern cc_int32_t DpintDebug;
extern cc_int32_t KpmlDebug;
extern cc_int32_t VCMDebug;
extern cc_int32_t PLATDebug;
extern cc_int32_t CCEVENTDebug;
extern cc_int32_t g_CCAppDebug;
extern cc_int32_t g_CCLogDebug;
extern cc_int32_t TNPDebug;

/**
  * Initialize all the debug variables
  */
void dbg_init(void)
{
    /* This is for the RT/TNP products */
    SipDebugMessage = 1;
    SipDebugState = 1;
    SipDebugTask = 1;
    SipDebugRegState = 1;
    GSMDebug = 1;
    FIMDebug = 1;
    LSMDebug = 1;
    FSMDebugSM = 1;
    CSMDebugSM = 0;
    VCMDebug = 1;
    PLATDebug = 1;
    CCEVENTDebug = 0;
    CCDebug = 0;
    CCDebugMsg = 0;
    AuthDebug = 1;
    TNPDebug = 1;
    ConfigDebug = 1;
    DpintDebug = 0;
    KpmlDebug = 0;
    g_CCAppDebug = 1;
    g_CCLogDebug = 1;
    TNPDebug = 1;
    g_NotifyCallDebug = 0;
    g_NotifyLineDebug = 0;
}
/**
 * Defines the management methods.
 */
/**
 * The following methods are defined to bring up the pSipcc stack
 */
/**
 * Initialize the pSipcc stack.
 * @return
 */
cc_return_t CC_Service_init() {
    //Initialize stack
    return ccInit();
}

/**
 * Pre-initialize the pSipcc stack.
 * @return
 */
cc_return_t CC_Service_create() {
    //Preinitialize memory
    ccPreInit();

    //Initialize debug settings
    dbg_init();

    //Prepopulate the Configuration data table
    protCfgTblInit();

    return CC_SUCCESS;
}

/**
 * Gracefully unload the pSipcc stack
 * @return
 */
cc_return_t CC_Service_destroy() {
    ccUnload();
    return CC_SUCCESS;
}

/**
 * Bring up the pSipcc stack in service
 * @return
 */
cc_return_t CC_Service_start() {
    sessionProvider_cmd_t proCmd;

    CCAPP_DEBUG("CC_Service_start");

    memset ( &proCmd, 0, sizeof(sessionProvider_cmd_t));
    proCmd.sessionType = SESSIONTYPE_CALLCONTROL; //Not used
    proCmd.cmd = CMD_INSERVICE;

    if (ccappTaskPostMsg(CCAPP_SERVICE_CMD, (cprBuffer_t)&proCmd,
                         sizeof(sessionProvider_cmd_t), CCAPP_CCPROVIER) == CPR_FAILURE) {
        CCAPP_DEBUG("CC_Service_start: ccappTaskSendMsg failed");
        return CC_FAILURE;
    }
    return CC_SUCCESS;
}

/**
 * Shutdown pSipcc stack for restarting
 * @param mgmt_reason the reason to shutdown pSipcc stack
 * @param reason_string literal string for shutdown
 * @return
 */
cc_return_t CC_Service_shutdown(cc_shutdown_reason_t mgmt_reason, string_t reason_string) {
    sessionProvider_cmd_t proCmd;

    CCAPP_DEBUG("CC_Service_shutdown");

    memset ( &proCmd, 0, sizeof(sessionProvider_cmd_t));
    proCmd.sessionType = SESSIONTYPE_CALLCONTROL; //Not used
    proCmd.cmd = CMD_SHUTDOWN;

    if (ccappTaskPostMsg(CCAPP_SERVICE_CMD, (cprBuffer_t)&proCmd,
                         sizeof(sessionProvider_cmd_t), CCAPP_CCPROVIER) == CPR_FAILURE) {
        CCAPP_DEBUG("CC_Service_shutdown: ccappTaskSendMsg failed");
        return CC_FAILURE;
    }
    return CC_SUCCESS;
}

/**
 * Unregister all lines of a phone
 * @param mgmt_reason the reason to bring down the registration
 * @param reason_string the literal string for unregistration
 * @return
 */
cc_return_t CC_Service_unregisterAllLines(cc_shutdown_reason_t mgmt_reason, string_t reason_string) {
    sessionProvider_cmd_t proCmd;

    CCAPP_DEBUG("CC_Service_shutdown");

    memset ( &proCmd, 0, sizeof(sessionProvider_cmd_t));
    proCmd.sessionType = SESSIONTYPE_CALLCONTROL; //Not used
    proCmd.cmd = CMD_SHUTDOWN;
    proCmd.cmdData.ccData.reason = mgmt_reason;
    proCmd.cmdData.ccData.reason_info = reason_string;

    if (ccappTaskPostMsg(CCAPP_SERVICE_CMD, (cprBuffer_t)&proCmd,
                         sizeof(sessionProvider_cmd_t), CCAPP_CCPROVIER) == CPR_FAILURE) {
        CCAPP_DEBUG("CC_Service_shutdown: ccappTaskSendMsg failed");
        return CC_FAILURE;
    }
    return CC_SUCCESS;
}

/**
 * Register all lines for a phone.
 * @param mgmt_reason the reason of registration
 * @param reason_string the literal string of the registration
 * @return
 */
cc_return_t CC_Service_registerAllLines(cc_shutdown_reason_t mgmt_reason, string_t reason_string) {
    sessionProvider_cmd_t proCmd;

    CCAPP_DEBUG("CC_Service_registerAllLines");

    memset ( &proCmd, 0, sizeof(sessionProvider_cmd_t));
    proCmd.sessionType = SESSIONTYPE_CALLCONTROL; //Not used
    proCmd.cmd = CMD_REGISTER_ALL_LINES;
    proCmd.cmdData.ccData.reason = mgmt_reason;
    proCmd.cmdData.ccData.reason_info = reason_string;

    if (ccappTaskPostMsg(CCAPP_SERVICE_CMD, (cprBuffer_t)&proCmd,
                         sizeof(sessionProvider_cmd_t), CCAPP_CCPROVIER) == CPR_FAILURE) {
        CCAPP_DEBUG("CC_Service_registerAllLines: ccappTaskSendMsg failed");
        return CC_FAILURE;
    }
    return CC_SUCCESS;
}

/**
 * Restart pSipcc stack
 * @return
 */
cc_return_t CC_Service_restart() {
    sessionProvider_cmd_t proCmd;

    CCAPP_DEBUG("CC_Service_restart");

    memset ( &proCmd, 0, sizeof(sessionProvider_cmd_t));
    proCmd.sessionType = SESSIONTYPE_CALLCONTROL; //Not used
    proCmd.cmd = CMD_RESTART;

    if (ccappTaskPostMsg(CCAPP_SERVICE_CMD, (cprBuffer_t)&proCmd,
                         sizeof(sessionProvider_cmd_t), CCAPP_CCPROVIER) == CPR_FAILURE) {
        CCAPP_DEBUG("CC_Service_restart: ccappTaskSendMsg failed");
        return CC_FAILURE;
    }
    return CC_SUCCESS;
}


