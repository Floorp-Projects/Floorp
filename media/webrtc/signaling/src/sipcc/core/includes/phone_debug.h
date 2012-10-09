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

#ifndef _PHONE_DEBUG_H_
#define _PHONE_DEBUG_H_

#include "cpr_stdio.h"
#include "cc_constants.h"

extern cc_int32_t SipDebugMessage;
extern cc_int32_t SipDebugState;
extern cc_int32_t SipDebugTask;
extern int32_t SipRelDevEnabled;
extern cc_int32_t SipDebugRegState;
extern int32_t SipDebugGenContactHeader;
extern cc_int32_t SipDebugTrx;

extern int32_t DebugPlatform;
extern int32_t DebugSOCTask;
extern int32_t DebugSNTP;
extern int32_t DebugSNTPPacket;
extern cc_int32_t TMRDebug;
extern cc_int32_t GSMDebug;
extern cc_int32_t FIMDebug;
extern cc_int32_t LSMDebug;
extern cc_int32_t VCMDebug;
extern cc_int32_t PLATDebug;
extern cc_int32_t CCEVENTDebug;
extern cc_int32_t FSMDebugSM;
extern int32_t CSMDebugSM;
extern cc_int32_t CCDebug;
extern cc_int32_t CCDebugMsg;
extern cc_int32_t AuthDebug;
extern int32_t DebugDTMFOutofBand;
extern int32_t DebugFlash;

extern int32_t StrlibDebug;
extern cc_int32_t ConfigDebug;

extern int32_t DebugMalloc;
extern int32_t DebugMallocTable;
extern int32_t SoftkeyDebug;
extern int32_t arp_debug_flag;
extern int32_t arp_debug_broadcast_flag;
extern int32_t cdp_debug;
extern int32_t timestamp_debug;
extern int32_t cdp_debug;

extern int32_t HTTPDebug;
extern cc_int32_t KpmlDebug;
extern cc_int32_t DpintDebug;
extern cc_int32_t g_cacDebug;
extern cc_int32_t g_blfDebug;
extern int32_t Dpint32_tDebug;
extern cc_int32_t TNPDebug;
extern cc_int32_t g_configappDebug;
extern cc_int32_t g_CCAppDebug;
extern cc_int32_t g_CCLogDebug;
extern cc_int32_t g_dcsmDebug;
extern cc_int32_t g_DEFDebug;

extern cc_int32_t g_NotifyCallDebug;
extern cc_int32_t g_NotifyLineDebug;

// ---------------- definitions based on bug_printf -----------------

 // Now some level definitions for buf_printf
#define BUG_BROADCAST 0  // can't be disabled
#define BUG_EMERGENCY 1
#define BUG_ALERT     2
#define BUG_CRITICAL  3
#define BUG_ERROR     4
#define BUG_WARNING   5
#define BUG_NOTICE    6
#define BUG_INFO      7
#define BUG_DEBUG     8  // debugs and such

extern int32_t bug_printf(int32_t level, const char *_format, ...);
extern int32_t nbuginf(const char *_format, ...);

#define logMsg buginf

/* SIP debug macros */
#define CCSIP_DEBUG_MESSAGE     if (SipDebugMessage) buginf
#define CCSIP_DEBUG_MESSAGE_PKT if (SipDebugMessage) platform_print_sip_msg
#define CCSIP_DEBUG_STATE       if (SipDebugState) buginf
#define CCSIP_DEBUG_TASK        if (SipDebugTask) buginf
#define CCSIP_DEBUG_REG_STATE   if (SipDebugRegState) buginf
#define CCSIP_DEBUG_TRX         if (SipDebugTrx) buginf
#define CCSIP_DEBUG_DM          if (SipDebugDM) buginf

/* Platform debug macros */
#define PHN_DEBUG_SNTP        if (DebugSNTP)
#define PHN_DEBUG_SNTP_PACKET if (DebugSNTPPacket)

/* TNP adapter debugs */
#define TNP_DEBUG  if (TNPDebug) buginf

#define DEF_DISPLAY_BUF          128
/*
 * Only used in the IOS SIP Parser routines
 */
#define CCSIP_ERR_MSG           err_msg
#define CCSIP_DEBUG_ERROR       err_msg
#define CCSIP_ERR_DEBUG         if (1)
#define CCSIP_INFO_DEBUG        if (1)

#define TMR_DEBUG     if (TMRDebug)    buginf
#define GSM_DEBUG     if (GSMDebug)    buginf
#define FIM_DEBUG     if (FIMDebug)    buginf
#define LSM_DEBUG     if (LSMDebug)    buginf
#define CALL_EVENT     if (CCEVENTDebug)    buginf
#define FSM_DEBUG_SM  if (FSMDebugSM)  buginf
#define CSM_DEBUG_SM  if (CSMDebugSM)  buginf
#define DEF_DEBUG     if (g_DEFDebug) notice_msg

#define CC_DEBUG      if (CCDebug)       buginf
#define CC_DEBUG_MSG  if (CCDebugMsg)
#define AUTH_DEBUG    if (AuthDebug)     buginf
#define ARP_DEBUG     if (arp_debug_flag) buginf
#define ARP_BCAST_DEBUG if (arp_debug_broadcast_flag) buginf
#define CDP_DEBUG     if (cdp_debug) buginf

#define NOTIFY_CALL_DEBUG if (g_NotifyCallDebug) buginf
#define NOTIFY_LINE_DEBUG if (g_NotifyLineDebug) buginf

/* String library macro */
#define CONFIG_DEBUG  if (ConfigDebug) buginf
#define FLASH_DEBUG   if (DebugFlash) buginf
#define KPML_DEBUG    if (KpmlDebug) buginf
#define CAC_DEBUG    if (g_cacDebug) buginf
#define DCSM_DEBUG    if (g_dcsmDebug) buginf
#define BLF_DEBUG    if (g_blfDebug) buginf
#define BLF_ERROR  err_msg
#define MISC_APP_DEBUG  if (g_miscAppDebug) buginf
#define DPINT_DEBUG   if (DpintDebug) buginf
#define CONFIGAPP_DEBUG  if (g_configappDebug) buginf
#define CCAPP_DEBUG  if (g_CCAppDebug) buginf
#define CCLOG_DEBUG  if (g_CCLogDebug) buginf
#define MSP_DEBUG    if (VCMDebug) buginf
#define CCAPP_ERROR    err_msg
#define MSP_ERROR    err_msg
#define CCAPP_ERROR  err_msg
#define TNP_ERR  err_msg
#define VCM_ERR  err_msg
#define DEF_ERROR  err_msg
#define DEF_ERR    err_msg

#define APP_NAME "SIPCC-"
#define DEB_L_C_F_PREFIX APP_NAME"%s: %d/%d, %s: "
#define DEB_L_C_F_PREFIX_ARGS(msg_name, line, call_id, func_name) \
	msg_name, line, call_id, func_name
#define DEB_F_PREFIX APP_NAME"%s: %s: "
#define DEB_F_PREFIX_ARGS(msg_name, func_name) msg_name, func_name
#define GSM_L_C_F_PREFIX "GSM : %d/%d : %s : " // requires 3 args: line_id, call_id, fname
#define GSM_F_PREFIX "GSM : %s : " // requires 1 arg: fname
#define GSM_DEBUG_ERROR       err_msg
#define LSM_L_C_F_PREFIX "LSM : %d/%d : %s : " // requires 3 args: line_id, call_id, fname
#define LSM_F_PREFIX "LSM : %s : " // requires 1 arg: fname
#define CCA_L_C_F_PREFIX "CCA : %d/%d : %s : " // requires 3 args: line_id, call_id, fname
#define CCA_F_PREFIX "CCA : %s : " // requires 1 arg: fname
#define MSP_F_PREFIX "SIPCC-MSP: %s: "
#define DEB_NOTIFY_PREFIX "AUTO.%s:"

#define MISC_F_PREFIX "MSC : %s : " // requires 1 arg: fname

#define KPML_L_C_F_PREFIX "KPM : %d/%d : %s : " // requires 3 args: line_id, call_id, fname
#define KPML_F_PREFIX "KPM : %s : " // requires 1 arg: fname
#define KPML_ERROR       err_msg
#define CAC_ERROR        err_msg
#define DCSM_ERROR        err_msg
#define CONFIG_ERROR        err_msg
#define PLAT_ERROR        err_msg

#define CAC_L_C_F_PREFIX "CAC : %d/%d : %s : " // requires 3 args: line_id, call_id, fname
#define CAC_F_PREFIX "CAC : %s : " // requires 1 arg: fname
#define DCSM_L_C_F_PREFIX "DCSM : %d/%d : %s : " // requires 3 args: line_id, call_id, fname
#define DCSM_F_PREFIX "DCSM : %s : " // requires 1 arg: fname
#define CFG_F_PREFIX "CFG : %s : " // requires 1 arg: fname
#define PLAT_COMMON_F_PREFIX "PLAT_COMMON : %s : " // requires 1 arg: fname
#define MED_F_PREFIX "MED : %s : "
#define MED_L_C_F_PREFIX "MED :  %d/%d : %s : " // line/call/fname as arg


/* Debug MSGNAMEs */
#define CC_API "CC_API" // CCAPI debugs
#define FSM "FSM" // FSM debugs
#define GSM "GSM" // gsm related debugs
#define DCSM "DCSM" // gsm related debugs
#define SIP_STATE "SIP_STATE"
#define SIP_EVT "SIP_EVT" // SIP events
#define SIP_MSG_RECV "SIP_MSG_RECV"
#define SIP_MSG_SEND "SIP_MSG_SEND"
#define SIP_REG_STATE "SIP_REG_STATE"
#define SIP_REG_FREE_FALLBACK "SIP_REG_FREE_FALLBACK"
#define DP_API "DP_API" // dial plan
#define KPML_INFO "KPML_INFO"
#define BLF_INFO "BLF_INFO"// presence information
#define MED_API "MED_API" // media api
#define UI_API "UI_API"
#define FIM "FIM" // FIM debugs
#define LSM "LSM"
#define SIP_AUTH "SIP_AUTH"
#define TNP_INIT "TNP_INIT"
#define TNP_BLF "TNP_BLF"
#define RM "RM" // resource manager
#define SIP_KA "SIP_KA" // SIP keepalive
#define JNI "JNI"
#define CONFIG_API "CONFIG_API" // config table API
#define BLF "BLF"
#define CONFIG_APP "CONFIG_APP"
#define SIP_MSG_QUE "SIP_MSG_QUE" // SIP message queue
#define SIP_TRANS "SIP_TRANS" // SIP transport
#define HTTPISH "HTTPISH" // Network to structure translation
#define SIP_MSG "SIP_MSG"
#define SIP_TIMER "SIP_TIMER"
#define SIP_ID "SIP_ID"
#define SIP_FWD "SIP_FWD"
#define SIP_NOTIFY "SIP_NOTIFY"
#define SIP_TASK "SIP_TASK"
#define SIP_IP_MATCH "SIP_IP_MATCH"
#define SIP_SUB "SIP_SUB" // SIP subscription
#define SIP_SDP "SIP_SDP"
#define SIP_STORE "SIP_STORE"
#define SIP_PUB "SIP_PUB" // SIP publish
//#define SIP_UDP "SIP_UDP"
#define SIP_TLS "SIP_TLS"
//#define SIP_TCP "SIP_TCP"
#define SIP_SDP "SIP_SDP"
#define SIP_REP "SIP_REP" // SIP replace info
#define SIP_PROXY "SIP_PROXY"
#define SIP_CALL_STATUS "SIP_CALL_STATUS"
#define SIP_ACK "SIP_ACK" // SIP invite response ack
#define CPR "CPR"
#define SIP_BRANCH "SIP_BRANCH"
#define SIP_CTRL "SIP_CTRL"
#define SIP_TRX "SIP_TRX" // SIP transaction
#define SIP_ROUTE "SIP_ROUTE"
#define SIP_CALL_ID "SIP_CALL_ID"
#define SIP_CSEQ "SIP_CSEQ"
#define SIP_TAG "SIP_TAG"
#define SIP_RESP "SIP_RESP" // SIP response message
#define SIP_REQ_URI "SIP_REQ_URI"
#define SIP "SIP"
#define SIP_REG "SIP_REG"
#define SIP_CCM_RESTART "SIP_CCM_RESTART"
#define SIP_CRED "SIP_CRED" // SIP registration credentials
#define SIP_BUTTON "SIP_BUTTON"
#define SIP_SUB_RESP "SIP_SUB_RESP" // SIP subscription response
#define SIP_NOTIFY "SIP_NOTIFY"
#define SIP_FALLBACK "SIP_FALLBACK"
#define SIP_FAILOVER "SIP_FAILOVER"
#define SIP_STANDBY "SIP_STANDBY"
#define SIP_CC_CONN "SIP_CC_CONN" // SIP CC connections
#define SIP_CONFIG "SIP_CONFIG"
#define DIALPLAN "DIALPLAN"
#define SIP_REQ_DIGEST "SIP_REQ_DIGEST"
#define SIP_CC_INIT "SIP_CC_INIT"
#define SIP_CC_PROV "SIP_CC_PROV" // CC Provider
#define SIP_SES_HASH "SIP_SES_HASH" // session hash
#define PLAT_API "PLAT_API" // platform API
#define CHUNK "CHUNK"
#define MISC_UTIL "MISC_UTIL"
#define SIP_CC_SES "SIP_CC_SES" // CC session
#define CAPF_UI "CAPF_UI"
#define CC_API_NATIVE "CC_API_NATIVE"
#define TANDUN_STUB "TANDUN_STUB"
#define SIP_SOCK "SIP_SOCK"
#define SIP_TCP_MSG "SIP_TCP_MSG"
#define MS_PROVIDER "MSP"
#define SIP_CONTENT_TYPE "SIP_CONTENT_TYPE"
#define SIP_INFO_PACKAGE "SIP_INFO_PACKAGE"

#endif
