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

    CCAPP_DEBUG("CC_Service_start \n");

    memset ( &proCmd, 0, sizeof(sessionProvider_cmd_t));
    proCmd.sessionType = SESSIONTYPE_CALLCONTROL; //Not used
    proCmd.cmd = CMD_INSERVICE;

    if (ccappTaskPostMsg(CCAPP_SERVICE_CMD, (cprBuffer_t)&proCmd,
                         sizeof(sessionProvider_cmd_t), CCAPP_CCPROVIER) == CPR_FAILURE) {
        CCAPP_DEBUG("CC_Service_start: ccappTaskSendMsg failed\n");
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

    CCAPP_DEBUG("CC_Service_shutdown \n");

    memset ( &proCmd, 0, sizeof(sessionProvider_cmd_t));
    proCmd.sessionType = SESSIONTYPE_CALLCONTROL; //Not used
    proCmd.cmd = CMD_SHUTDOWN;

    if (ccappTaskPostMsg(CCAPP_SERVICE_CMD, (cprBuffer_t)&proCmd,
                         sizeof(sessionProvider_cmd_t), CCAPP_CCPROVIER) == CPR_FAILURE) {
        CCAPP_DEBUG("CC_Service_shutdown: ccappTaskSendMsg failed\n");
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

    CCAPP_DEBUG("CC_Service_shutdown \n");

    memset ( &proCmd, 0, sizeof(sessionProvider_cmd_t));
    proCmd.sessionType = SESSIONTYPE_CALLCONTROL; //Not used
    proCmd.cmd = CMD_SHUTDOWN;
    proCmd.cmdData.ccData.reason = mgmt_reason;
    proCmd.cmdData.ccData.reason_info = reason_string;

    if (ccappTaskPostMsg(CCAPP_SERVICE_CMD, (cprBuffer_t)&proCmd,
                         sizeof(sessionProvider_cmd_t), CCAPP_CCPROVIER) == CPR_FAILURE) {
        CCAPP_DEBUG("CC_Service_shutdown: ccappTaskSendMsg failed\n");
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

    CCAPP_DEBUG("CC_Service_registerAllLines \n");

    memset ( &proCmd, 0, sizeof(sessionProvider_cmd_t));
    proCmd.sessionType = SESSIONTYPE_CALLCONTROL; //Not used
    proCmd.cmd = CMD_REGISTER_ALL_LINES;
    proCmd.cmdData.ccData.reason = mgmt_reason;
    proCmd.cmdData.ccData.reason_info = reason_string;

    if (ccappTaskPostMsg(CCAPP_SERVICE_CMD, (cprBuffer_t)&proCmd,
                         sizeof(sessionProvider_cmd_t), CCAPP_CCPROVIER) == CPR_FAILURE) {
        CCAPP_DEBUG("CC_Service_registerAllLines: ccappTaskSendMsg failed\n");
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

    CCAPP_DEBUG("CC_Service_restart \n");

    memset ( &proCmd, 0, sizeof(sessionProvider_cmd_t));
    proCmd.sessionType = SESSIONTYPE_CALLCONTROL; //Not used
    proCmd.cmd = CMD_RESTART;

    if (ccappTaskPostMsg(CCAPP_SERVICE_CMD, (cprBuffer_t)&proCmd,
                         sizeof(sessionProvider_cmd_t), CCAPP_CCPROVIER) == CPR_FAILURE) {
        CCAPP_DEBUG("CC_Service_restart: ccappTaskSendMsg failed\n");
        return CC_FAILURE;
    }
    return CC_SUCCESS;
}


