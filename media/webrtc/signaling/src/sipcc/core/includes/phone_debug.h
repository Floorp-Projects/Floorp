/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _PHONE_DEBUG_H_
#define _PHONE_DEBUG_H_

#include "cpr_stdio.h"
#include "cc_constants.h"
#include "CSFLog.h"

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

/* SIP debug macros */
#define CCSIP_DEBUG_MESSAGE(format,...) if (SipDebugMessage) \
        CSFLogDebug("ccsip_message", format, ## __VA_ARGS__)

#define CCSIP_DEBUG_MESSAGE_PKT if (SipDebugMessage) platform_print_sip_msg
#define CCSIP_DEBUG_STATE(format,...) if (SipDebugState) \
        CSFLogDebug("ccsip_state", format, ## __VA_ARGS__)

#define CCSIP_DEBUG_TASK(format,...) if (SipDebugTask) \
        CSFLogDebug("ccsip_task", format, ## __VA_ARGS__)

#define CCSIP_DEBUG_REG_STATE(format,...) if (SipDebugRegState) \
        CSFLogDebug("ccsip_reg_state", format, ## __VA_ARGS__)

#define CCSIP_DEBUG_TRX(format,...) if (SipDebugTrx) \
        CSFLogDebug("ccsip_trx", format, ## __VA_ARGS__)

#define CCSIP_DEBUG_DM(format,...) if (SipDebugDM) \
        CSFLogDebug("ccsip_dm", format, ## __VA_ARGS__)


/* Platform debug macros */
#define PHN_DEBUG_SNTP        if (DebugSNTP)
#define PHN_DEBUG_SNTP_PACKET if (DebugSNTPPacket)

/*
 * TNP debugging has been raised from CSFLogDebug to CSFLogNotice
 * to help diagnose some intermittent bugs in which the SIPCC
 * code appears to have cleaned up a call, but the UI layer
 * (PeerConnectionImpl) is not informed. This is less intrusive
 * than running all the tests with logging turned all the way
 * up to debug. This can be reverted back to CSFLogDebug once
 * the WebRTC intermittent bugs are closed. For a current list
 * of such bugs, see the results of the following query:
 * http://tinyurl.com/webrtc-intermittent
 */
#define TNP_DEBUG(format,...) if (TNPDebug) \
        CSFLogNotice("tnp", format, ## __VA_ARGS__)


#define DEF_DISPLAY_BUF          128
/*
 * Only used in the IOS SIP Parser routines
 */
#define CCSIP_ERR_MSG(format,...)  \
        CSFLogError("ccsip", format, ## __VA_ARGS__)

#define CCSIP_DEBUG_ERROR(format,...)  \
        CSFLogError("ccsip", format, ## __VA_ARGS__)

#define CCSIP_ERR_DEBUG         if (1)
#define CCSIP_INFO_DEBUG        if (1)

#define TMR_DEBUG(format,...) if (TMRDebug) \
        CSFLogDebug("tmr", format, ## __VA_ARGS__)

#define GSM_DEBUG(format,...) if (GSMDebug) \
        CSFLogDebug("gsm", format, ## __VA_ARGS__)

#define FIM_DEBUG(format,...) if (FIMDebug) \
        CSFLogDebug("fim", format, ## __VA_ARGS__)

#define LSM_DEBUG(format,...) if (LSMDebug) \
        CSFLogDebug("lsm", format, ## __VA_ARGS__)

#define CALL_EVENT(format,...) if (CCEVENTDebug) \
        CSFLogDebug("call_event", format, ## __VA_ARGS__)

#define FSM_DEBUG_SM(format,...) if (FSMDebugSM) \
        CSFLogNotice("fsm_sm", format, ## __VA_ARGS__)

#define CSM_DEBUG_SM(format,...) if (CSMDebugSM) \
        CSFLogDebug("csm_sm", format, ## __VA_ARGS__)

#define DEF_DEBUG(format,...) if (g_DEFDebug) \
        CSFLogNotice("def", format, ## __VA_ARGS__)


#define CC_DEBUG(format,...) if (CCDebug) \
        CSFLogDebug("cc", format, ## __VA_ARGS__)

#define CC_DEBUG_MSG  if (CCDebugMsg)
#define AUTH_DEBUG(format,...) if (AuthDebug) \
        CSFLogDebug("auth", format, ## __VA_ARGS__)

#define ARP_DEBUG(format,...) if (arp_debug_flag) \
        CSFLogDebug("arp", format, ## __VA_ARGS__)

#define ARP_BCAST_DEBUG(format,...) if (arp_debug_broadcast_flag) \
        CSFLogDebug("arp_bcast", format, ## __VA_ARGS__)

#define CDP_DEBUG(format,...) if (cdp_debug) \
        CSFLogDebug("cdp", format, ## __VA_ARGS__)


#define NOTIFY_CALL_DEBUG(format,...) if (g_NotifyCallDebug) \
        CSFLogDebug("notify_call", format, ## __VA_ARGS__)

#define NOTIFY_LINE_DEBUG(format,...) if (g_NotifyLineDebug) \
        CSFLogDebug("notify_line", format, ## __VA_ARGS__)


/* String library macro */
#define CONFIG_DEBUG(format,...) if (ConfigDebug) \
        CSFLogDebug("config", format, ## __VA_ARGS__)

#define FLASH_DEBUG(format,...) if (DebugFlash) \
        CSFLogDebug("flash", format, ## __VA_ARGS__)

#define KPML_DEBUG(format,...) if (KpmlDebug) \
        CSFLogDebug("kpml", format, ## __VA_ARGS__)

#define CAC_DEBUG(format,...) if (g_cacDebug) \
        CSFLogDebug("cac", format, ## __VA_ARGS__)

#define DCSM_DEBUG(format,...) if (g_dcsmDebug) \
        CSFLogDebug("dcsm", format, ## __VA_ARGS__)

#define BLF_DEBUG(format,...) if (g_blfDebug) \
        CSFLogDebug("blf", format, ## __VA_ARGS__)

#define BLF_ERROR(format,...)  \
        CSFLogError("blf", format, ## __VA_ARGS__)

#define MISC_APP_DEBUG(format,...) if (g_miscAppDebug) \
        CSFLogDebug("misc_app", format, ## __VA_ARGS__)

#define DPINT_DEBUG(format,...) if (DpintDebug) \
        CSFLogDebug("dpint", format, ## __VA_ARGS__)

#define CONFIGAPP_DEBUG(format,...) if (g_configappDebug) \
        CSFLogDebug("configapp", format, ## __VA_ARGS__)

#define CCAPP_DEBUG(format,...) if (g_CCAppDebug) \
        CSFLogDebug("ccapp", format, ## __VA_ARGS__)

#define CCLOG_DEBUG(format,...) if (g_CCLogDebug) \
        CSFLogDebug("cclog", format, ## __VA_ARGS__)

#define MSP_DEBUG(format,...) if (VCMDebug) \
        CSFLogDebug("msp", format, ## __VA_ARGS__)

#define CCAPP_ERROR(format,...)  \
        CSFLogError("ccapp", format, ## __VA_ARGS__)

#define MSP_ERROR(format,...)  \
        CSFLogError("msp", format, ## __VA_ARGS__)

#define CCAPP_ERROR(format,...)  \
        CSFLogError("ccapp", format, ## __VA_ARGS__)

#define TNP_ERR(format,...)  \
        CSFLogError("tnp", format, ## __VA_ARGS__)

#define VCM_ERR(format,...)  \
        CSFLogError("vcm", format, ## __VA_ARGS__)

#define DEF_ERROR(format,...)  \
        CSFLogError("def", format, ## __VA_ARGS__)

#define DEF_ERR(format,...)  \
        CSFLogError("def", format, ## __VA_ARGS__)


#define APP_NAME "SIPCC-"
#define DEB_L_C_F_PREFIX APP_NAME"%s: %d/%d, %s: "
#define DEB_L_C_F_PREFIX_ARGS(msg_name, line, call_id, func_name) \
        msg_name, line, call_id, func_name
#define DEB_F_PREFIX APP_NAME"%s: %s: "
#define DEB_F_PREFIX_ARGS(msg_name, func_name) msg_name, func_name
#define GSM_L_C_F_PREFIX "GSM : %d/%d : %s : " // requires 3 args: line_id, call_id, fname
#define GSM_F_PREFIX "GSM : %s : " // requires 1 arg: fname
#define GSM_DEBUG_ERROR(format,...)  \
        CSFLogError("gsm", format, ## __VA_ARGS__)

#define LSM_L_C_F_PREFIX "LSM : %d/%d : %s : " // requires 3 args: line_id, call_id, fname
#define LSM_F_PREFIX "LSM : %s : " // requires 1 arg: fname
#define CCA_L_C_F_PREFIX "CCA : %d/%d : %s : " // requires 3 args: line_id, call_id, fname
#define CCA_F_PREFIX "CCA : %s : " // requires 1 arg: fname
#define MSP_F_PREFIX "SIPCC-MSP: %s: "
#define DEB_NOTIFY_PREFIX "AUTO.%s:"

#define MISC_F_PREFIX "MSC : %s : " // requires 1 arg: fname

#define KPML_L_C_F_PREFIX "KPM : %d/%d : %s : " // requires 3 args: line_id, call_id, fname
#define KPML_F_PREFIX "KPM : %s : " // requires 1 arg: fname
#define KPML_ERROR(format,...)  \
        CSFLogError("kpml", format, ## __VA_ARGS__)

#define CAC_ERROR(format,...)  \
        CSFLogError("cac", format, ## __VA_ARGS__)

#define DCSM_ERROR(format,...)  \
        CSFLogError("dcsm", format, ## __VA_ARGS__)

#define CONFIG_ERROR(format,...)  \
        CSFLogError("config", format, ## __VA_ARGS__)

#define PLAT_ERROR(format,...)  \
        CSFLogError("plat", format, ## __VA_ARGS__)


#define CAC_L_C_F_PREFIX "CAC : %d/%d : %s : " // requires 3 args: line_id, call_id, fname
#define DCSM_L_C_F_PREFIX "DCSM : %d/%d : %s : " // requires 3 args: line_id, call_id, fname
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
