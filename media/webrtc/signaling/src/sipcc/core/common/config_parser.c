/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include "cc_constants.h"
#include "cc_types.h"
#include "cc_config.h"
#include "phone_debug.h"
#include "debug.h"
#include "ccapi.h"
#include "prot_configmgr.h"
#include "call_logger.h"
#include "sip_common_transport.h"
#include "sip_ccm_transport.h"
#include "config_parser.h"
#include "cc_device_feature.h"
#include "ccapi_snapshot.h"
#include "config_api.h"
#include "capability_set.h"
#include "util_string.h"

#define MAC_ADDR_SIZE   6
#define FILE_PATH 256
#define MAX_MULTI_LEVEL_CONFIG 2

#define MLCFG_VIDEO_CAPABILITY    0
#define MLCFG_CISCO_CAMERA        1
#define MLCFG_CAPABILITY_MAX      2
#define MLCFG_NOT_SET -1

#define VERSION_LENGTH_MAX 100

#define ID_BLOCK_PREF1   1
#define ID_BLOCK_PREF3   3
/*
 * File location is hardcoded for getting mac and IP addr
 */
#define IP_ADDR_FILE  "/sdcard/myip.txt"
static char autoreg_name[MAX_LINE_NAME_SIZE];

static char fcpTemplateFile[FILE_PATH] = "";

char g_cfg_version_stamp[MAX_CFG_VERSION_STAMP_LEN + 1] = {0};
int line = -1;  //initialize line to -1, as 0 is valid line
boolean apply_config = FALSE;
cc_apply_config_result_t apply_config_result = APPLY_CONFIG_NONE;
extern var_t prot_cfg_table[];
void print_config_value (int id, char *get_set, const char *entry_name, void *buffer, int length);

static int sip_port[MAX_CCM];
static int secured_sip_port[MAX_CCM];
static int security_mode = 3; /*SECURE*/
extern accessory_cfg_info_t g_accessoryCfgInfo;

// Configurable settings
static int gTransportLayerProtocol = 4;   //  4 = tcp, 2 = udp
static boolean gP2PSIP = FALSE;
static boolean gSDPMODE = FALSE;
static int gVoipControlPort = 5060;
static int gCcm1_sip_port = 5060;

/*
 * This function determine whether the passed config parameter should be used
 * in comparing the new and old config value for apply-config purpose.  Only
 * those config ids on whose change phone needs to restart are part of this
 * function. For remaining parameters, it is assumed that any change can be
 * applied dynamically.
 *
 */
boolean is_cfgid_in_restart_list(int cfgid) {

    if ((cfgid >= CFGID_LINE_FEATURE && cfgid < (CFGID_LINE_FEATURE + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_INDEX && cfgid < (CFGID_LINE_INDEX + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_PROXY_ADDRESS && cfgid < (CFGID_PROXY_ADDRESS + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_PROXY_PORT && cfgid < (CFGID_PROXY_PORT + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_NAME && cfgid < (CFGID_LINE_NAME + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_DISPLAYNAME && cfgid < (CFGID_LINE_DISPLAYNAME + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_SPEEDDIAL_NUMBER && cfgid < (CFGID_LINE_SPEEDDIAL_NUMBER + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_MESSAGES_NUMBER && cfgid < (CFGID_LINE_MESSAGES_NUMBER + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_FWD_CALLER_NAME_DIPLAY && cfgid < (CFGID_LINE_FWD_CALLER_NAME_DIPLAY + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_FWD_CALLER_NUMBER_DIPLAY && cfgid < (CFGID_LINE_FWD_CALLER_NUMBER_DIPLAY + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_FWD_REDIRECTED_NUMBER_DIPLAY && cfgid < (CFGID_LINE_FWD_REDIRECTED_NUMBER_DIPLAY + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_FWD_DIALED_NUMBER_DIPLAY && cfgid < (CFGID_LINE_FWD_DIALED_NUMBER_DIPLAY + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_CALL_WAITING && cfgid < (CFGID_LINE_CALL_WAITING + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_AUTHNAME && cfgid < (CFGID_LINE_AUTHNAME + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_PASSWORD && cfgid < (CFGID_LINE_PASSWORD + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_FEATURE_OPTION_MASK && cfgid < (CFGID_LINE_FEATURE_OPTION_MASK + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_MSG_WAITING_LAMP && cfgid < (CFGID_LINE_MSG_WAITING_LAMP + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_MESSAGE_WAITING_AMWI && cfgid < (CFGID_LINE_MESSAGE_WAITING_AMWI + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_RING_SETTING_IDLE && cfgid < (CFGID_LINE_RING_SETTING_IDLE + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_RING_SETTING_ACTIVE && cfgid < (CFGID_LINE_RING_SETTING_ACTIVE + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_CONTACT && cfgid < (CFGID_LINE_CONTACT + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_AUTOANSWER_ENABLED && cfgid < (CFGID_LINE_AUTOANSWER_ENABLED + MAX_CONFIG_LINES)) ||
        (cfgid >= CFGID_LINE_AUTOANSWER_MODE && cfgid < (CFGID_LINE_AUTOANSWER_MODE + MAX_CONFIG_LINES))
       )
    {
        return TRUE;
    }
    switch (cfgid) {
        case CFGID_CCM1_ADDRESS:
        case CFGID_CCM2_ADDRESS:
        case CFGID_CCM3_ADDRESS:
        case CFGID_CCM1_SIP_PORT:
        case CFGID_CCM2_SIP_PORT:
        case CFGID_CCM3_SIP_PORT:

        case CFGID_PROXY_BACKUP:
        case CFGID_PROXY_BACKUP_PORT:
        case CFGID_PROXY_EMERGENCY:
        case CFGID_PROXY_EMERGENCY_PORT:
        case CFGID_OUTBOUND_PROXY:
        case CFGID_OUTBOUND_PROXY_PORT:

        case CFGID_PROXY_REGISTER:
        case CFGID_REMOTE_CC_ENABLED:

        case CFGID_SIP_INVITE_RETX:
        case CFGID_SIP_RETX:
        case CFGID_TIMER_INVITE_EXPIRES:
        case CFGID_TIMER_KEEPALIVE_EXPIRES:
        case CFGID_TIMER_SUBSCRIBE_EXPIRES:
        case CFGID_TIMER_SUBSCRIBE_DELTA:
        case CFGID_TIMER_T1:
        case CFGID_TIMER_T2:

        case CFGID_SIP_MAX_FORWARDS:
        case CFGID_REMOTE_PARTY_ID:
        case CFGID_REG_USER_INFO:

        case CFGID_PREFERRED_CODEC:
        case CFGID_VOIP_CONTROL_PORT:
        case CFGID_NAT_ENABLE:
        case CFGID_NAT_ADDRESS:
        case CFGID_NAT_RECEIVED_PROCESSING:

        case CFGID_DTMF_AVT_PAYLOAD:
        case CFGID_DTMF_DB_LEVEL:
        case CFGID_DTMF_OUTOFBAND:

        case CFGID_KPML_ENABLED:
        case CFGID_MEDIA_PORT_RANGE_START:
        case CFGID_TRANSPORT_LAYER_PROT:

        case CFGID_TIMER_REGISTER_EXPIRES:
        case CFGID_TIMER_REGISTER_DELTA:
        case CFGID_DSCP_FOR_CALL_CONTROL:
            return TRUE;
        default:
            return FALSE;
    }
}


/*
 * This function either compare the new and old config value or set the value
 * for the config_id passed depending upon whether apply-config is true or not.
 */
void compare_or_set_byte_value(int cfgid, unsigned char value, const unsigned char * config_name) {
    int temp_value ;
    const var_t *entry;
    if (apply_config == TRUE) {
        if (is_cfgid_in_restart_list(cfgid) == TRUE) {
            config_get_value(cfgid, &temp_value, sizeof(temp_value));
            if (((int)value) !=  temp_value) {
                apply_config_result = RESTART_NEEDED;
                entry = &prot_cfg_table[cfgid];
                print_config_value(cfgid, "changed Get Val", entry->name, &temp_value, sizeof(temp_value));
                DEF_DEBUG(CFG_F_PREFIX "config %s[%d] changed. Old value=%d new value=%d\n", "compare_or_set_byte_value", config_name, cfgid, temp_value, value);
            }
        }
    } else {
        CC_Config_setByteValue(cfgid, value);
    }
}

/*
 * This function either compare the new and old config value or set the value
 * for the config_id passed depending upon whether apply-config is true or not.
 */
void compare_or_set_boolean_value(int cfgid, cc_boolean value, const unsigned char * config_name) {
    int temp_value ;
    const var_t *entry;
    if (apply_config == TRUE) {
        if (is_cfgid_in_restart_list(cfgid) == TRUE) {
            config_get_value(cfgid, &temp_value, sizeof(temp_value));
            if (((int)value) !=  temp_value) {
                apply_config_result = RESTART_NEEDED;
                entry = &prot_cfg_table[cfgid];
                print_config_value(cfgid, "changed Get Val", entry->name, &temp_value, sizeof(temp_value));
                DEF_DEBUG(CFG_F_PREFIX "config %s[%d] changed. Old value=%d new value=%d\n", "compare_or_set_boolean_value", config_name, cfgid, temp_value, value);
            }
        }
    } else {
        CC_Config_setBooleanValue(cfgid, value);
    }
}

/*
 * This function either compare the new and old config value or set the value
 * for the config_id passed depending upon whether apply-config is true or not.
 */
void compare_or_set_int_value(int cfgid, int value, const unsigned char * config_name) {
    int temp_value;
    const var_t *entry;
    if (apply_config == TRUE) {
        if (is_cfgid_in_restart_list(cfgid) == TRUE) {
            config_get_value(cfgid, &temp_value, sizeof(temp_value));
            if (value !=  temp_value) {
                apply_config_result = RESTART_NEEDED;
                entry = &prot_cfg_table[cfgid];
                print_config_value(cfgid, "changed Get Val", entry->name, &temp_value, sizeof(temp_value));

                DEF_DEBUG(CFG_F_PREFIX "config %s[%d] changed. new value=%d Old value=%d\n", "compare_or_set_int_value", config_name, cfgid, value, temp_value);
            }
        }
    } else {
        CC_Config_setIntValue(cfgid, value);
    }
}

/*
 * This function either compare the new and old config value or set the value
 * for the config_id passed depending upon whether apply-config is true or not.
 */
void compare_or_set_string_value (int cfgid, const char* value, const unsigned char * config_name) {
    static char temp_value[MAX_SIP_URL_LENGTH];
    const var_t *entry;
    if (apply_config == TRUE ) {
        if (is_cfgid_in_restart_list(cfgid) == TRUE) {
            config_get_string(cfgid, temp_value, MAX_SIP_URL_LENGTH);
            if (strcmp(value, temp_value) != 0) {
                apply_config_result = RESTART_NEEDED;
                entry = &prot_cfg_table[cfgid];
                print_config_value(cfgid, "changed Get Val", entry->name, &temp_value, sizeof(temp_value));
                DEF_DEBUG(CFG_F_PREFIX "config %s[%d] changed. new value=%s Old value=%s\n", "compare_or_set_string_value", config_name, cfgid, value, temp_value);
            }
        }
    } else {
        CC_Config_setStringValue(cfgid, value);
    }
}

int lineConfig = 0;
int portConfig = 0;
int proxyConfig = 0;

/*
 * config_set_autoreg_properties
 *
 */
void config_set_autoreg_properties ()
{
    CC_Config_setIntValue(CFGID_LINE_INDEX + 0, 1);
	CC_Config_setIntValue(CFGID_LINE_FEATURE + 0, 9);
    CC_Config_setStringValue(CFGID_PROXY_ADDRESS + 0, "USECALLMANAGER");
    CC_Config_setIntValue(CFGID_PROXY_PORT + 0, 5060);
    CC_Config_setStringValue(CFGID_LINE_NAME + 0, autoreg_name);
    CC_Config_setBooleanValue(CFGID_PROXY_REGISTER, 1);
    CC_Config_setIntValue(CFGID_TRANSPORT_LAYER_PROT, 2);

    /* timerRegisterExpires = 3600 */
    CC_Config_setIntValue(CFGID_TIMER_REGISTER_EXPIRES, 3600);
    /* sipRetx = 10 */
   CC_Config_setIntValue(CFGID_SIP_RETX, 10);
    /* sipInviteRetx = 6 */
    CC_Config_setIntValue(CFGID_SIP_INVITE_RETX, 6);
    /* timerRegisterDelta = 5 */
    CC_Config_setIntValue(CFGID_TIMER_REGISTER_DELTA, 5);
    /* MaxRedirects = 70 */
    CC_Config_setIntValue(CFGID_SIP_MAX_FORWARDS, 70);
    /* timerInviteExpires = 180 */
    CC_Config_setIntValue(CFGID_TIMER_INVITE_EXPIRES, 180);
	/* timerSubscribeDelta = 5 */
    CC_Config_setIntValue(CFGID_TIMER_SUBSCRIBE_DELTA, 5);
	/* timerSubscribeExpires = 120 */
    CC_Config_setIntValue(CFGID_TIMER_SUBSCRIBE_EXPIRES, 120);

    CC_Config_setIntValue(CFGID_REMOTE_CC_ENABLED, 1);
    CC_Config_setIntValue(CFGID_VOIP_CONTROL_PORT, 5060);
}

/*
 * update_security_mode_and_ports
 *
 */
void update_security_mode_and_ports(void) {
        sec_level_t sec_level = NON_SECURE;

    // convert security mode (from UCM xml) into internal enum
    switch (security_mode)
    {
       case 1:  sec_level = NON_SECURE; break;
       case 2:  sec_level = AUTHENTICATED; break;
       case 3:  sec_level = ENCRYPTED; break;
       default:
          CONFIG_ERROR(CFG_F_PREFIX "unable to translate securite mode [%d]\n", "update_security_mode_and_ports", (int)security_mode);
          break;
        }

	compare_or_set_int_value(CFGID_CCM1_SEC_LEVEL, sec_level,
							 (const unsigned char *)"deviceSecurityMode");
	compare_or_set_int_value(CFGID_CCM2_SEC_LEVEL, sec_level,
							 (const unsigned char *)"deviceSecurityMode");
	compare_or_set_int_value(CFGID_CCM3_SEC_LEVEL, sec_level,
							 (const unsigned char *)"deviceSecurityMode");

	if (sec_level == NON_SECURE) {
		compare_or_set_int_value(CFGID_CCM1_SIP_PORT, sip_port[0],
								 (const unsigned char *)"ccm1_sip_port");
		compare_or_set_int_value(CFGID_CCM2_SIP_PORT, sip_port[1],
								 (const unsigned char *)"ccm2_sip_port");
		compare_or_set_int_value(CFGID_CCM3_SIP_PORT, sip_port[2],
								 (const unsigned char *)"ccm3_sip_port");
	} else {
		compare_or_set_int_value(CFGID_CCM1_SIP_PORT, secured_sip_port[0],
								 (const unsigned char *)"ccm1_secured_sip_port");
		compare_or_set_int_value(CFGID_CCM2_SIP_PORT, secured_sip_port[1],
								 (const unsigned char *)"ccm2_secured_sip_port");
		compare_or_set_int_value(CFGID_CCM3_SIP_PORT, secured_sip_port[2],
								 (const unsigned char *)"ccm3_secured_sip_port");
	}
}


#define MISSEDCALLS "Application:Cisco/MissedCalls"
#define PLACEDCALLS "Application:Cisco/PlacedCalls"
#define RECEIVEDCALLS "Application:Cisco/ReceivedCalls"

/*
 * config_get_mac_addr
 *
 * Get the filename that has the mac address and parse the string
 * convert it into an mac address stored in the bytearray maddr
*/
void config_get_mac_addr (char *maddr)
{
    platGetMacAddr(maddr);

}

/*
 * Set the MAC address in the config table
 */
void config_set_ccm_ip_mac ()
{

    char macaddr[MAC_ADDR_SIZE];

    compare_or_set_int_value(CFGID_DSCP_FOR_CALL_CONTROL , 1, (const unsigned char *) "DscpCallControl");
    compare_or_set_int_value(CFGID_SPEAKER_ENABLED, 1, (const unsigned char *) "speakerEnabled");

    if (apply_config == FALSE) {
        config_get_mac_addr(macaddr);

        CONFIG_DEBUG(CFG_F_PREFIX ": MAC Address IS:  %x:%x:%x:%x:%x:%x  \n",
                          "config_get_mac_addr", macaddr[0], macaddr[1],
                           macaddr[2], macaddr[3], macaddr[4], macaddr[5]);

        CC_Config_setArrayValue(CFGID_MY_MAC_ADDR, macaddr, MAC_ADDR_SIZE);
        CC_Config_setArrayValue(CFGID_MY_ACTIVE_MAC_ADDR, macaddr, MAC_ADDR_SIZE);
    }
}

/*
 * config_setup_element
 * Setup elements that once were downloaded from CUCM in an XML file.
 * Settings are stored in config.h
 */
void config_setup_elements (const char *sipUser, const char *sipPassword, const char *sipDomain)
{
    unsigned int i;
    char buf[MAX_SIP_URL_LENGTH] = {'\0'};
    char ip[MAX_SIP_URL_LENGTH] = {'\0'};
    char option[MAX_SIP_URL_LENGTH] = {'\0'};
    int line = 0;
    cc_boolean isSecure = FALSE, isValid = TRUE;
    char macaddr[MAC_ADDR_SIZE];

    compare_or_set_int_value(CFGID_MEDIA_PORT_RANGE_START, gStartMediaPort, (const unsigned char *) "startMediaPort");
    compare_or_set_int_value(CFGID_MEDIA_PORT_RANGE_END, gStopMediaPort, (const unsigned char *) "stopMediaPort");
    compare_or_set_boolean_value(CFGID_CALLERID_BLOCKING, gCallerIdBlocking, (const unsigned char *) "callerIdBlocking");
    compare_or_set_boolean_value(CFGID_ANONYMOUS_CALL_BLOCK, gAnonblock, (const unsigned char *) "anonymousCallBlock");
    compare_or_set_string_value(CFGID_PREFERRED_CODEC, gPreferredCodec, (const unsigned char *) "preferredCodec");
    compare_or_set_string_value(CFGID_DTMF_OUTOFBAND, gDtmfOutOfBand, (const unsigned char *) "dtmfOutofBand");
    compare_or_set_int_value(CFGID_DTMF_AVT_PAYLOAD, gDtmfAvtPayload, (const unsigned char *) "dtmfAvtPayload");
    compare_or_set_int_value(CFGID_DTMF_DB_LEVEL, gDtmfDbLevel, (const unsigned char *) "dtmfDbLevel");
    compare_or_set_int_value(CFGID_SIP_RETX, gSipRetx, (const unsigned char *) "sipRetx");
    compare_or_set_int_value(CFGID_SIP_INVITE_RETX, gSipInviteRetx, (const unsigned char *) "sipInviteRetx");
    compare_or_set_int_value(CFGID_TIMER_T1, gTimerT1, (const unsigned char *) "timerT1");
    compare_or_set_int_value(CFGID_TIMER_T2, gTimerT2, (const unsigned char *) "timerT2");
    compare_or_set_int_value(CFGID_TIMER_INVITE_EXPIRES, gTimerInviteExpires, (const unsigned char *) "timerInviteExpires");
    compare_or_set_int_value(CFGID_TIMER_REGISTER_EXPIRES, gTimerRegisterExpires, (const unsigned char *) "timerRegisterExpires");
    compare_or_set_boolean_value(CFGID_PROXY_REGISTER, gRegisterWithProxy, (const unsigned char *) "registerWithProxy");
    compare_or_set_string_value(CFGID_PROXY_BACKUP, gBackupProxy, (const unsigned char *) "backupProxy");
    compare_or_set_int_value(CFGID_PROXY_BACKUP_PORT, gBackupProxyPort, (const unsigned char *) "backupProxyPort");
    compare_or_set_string_value(CFGID_PROXY_EMERGENCY, gEmergencyProxy, (const unsigned char *) "emergencyProxy");
    compare_or_set_int_value(CFGID_PROXY_EMERGENCY_PORT, gEmergencyProxyPort, (const unsigned char *) "emergencyProxyPort");
    compare_or_set_string_value(CFGID_OUTBOUND_PROXY, gOutboundProxy, (const unsigned char *) "outboundProxy");
    compare_or_set_int_value(CFGID_OUTBOUND_PROXY_PORT, gOutboundProxyPort, (const unsigned char *) "outboundProxyPort");
    compare_or_set_boolean_value(CFGID_NAT_RECEIVED_PROCESSING, gNatRecievedProcessing, (const unsigned char *) "natRecievedProcessing");
    compare_or_set_string_value(CFGID_REG_USER_INFO, gUserInfo, (const unsigned char *) "userInfo");
    compare_or_set_boolean_value(CFGID_REMOTE_PARTY_ID, gRemotePartyID, (const unsigned char *) "remotePartyID");
    compare_or_set_boolean_value (CFGID_SEMI_XFER, gSemiAttendedTransfer, (const unsigned char *) "semiAttendedTransfer");
    compare_or_set_int_value(CFGID_CALL_HOLD_RINGBACK, gCallHoldRingback, (const unsigned char *) "callHoldRingback");
    compare_or_set_boolean_value(CFGID_STUTTER_MSG_WAITING, gStutterMsgWaiting, (const unsigned char *) "stutterMsgWaiting");
    compare_or_set_string_value(CFGID_CALL_FORWARD_URI, gCallForwardURI, (const unsigned char *) "callForwardURI");
    compare_or_set_boolean_value(CFGID_CALL_STATS, gCallStats, (const unsigned char *) "callStats");
    compare_or_set_int_value(CFGID_TIMER_REGISTER_DELTA, gTimerRegisterDelta, (const unsigned char *) "timerRegisterDelta");
    compare_or_set_int_value(CFGID_SIP_MAX_FORWARDS, gMaxRedirects, (const unsigned char *) "maxRedirects");
    compare_or_set_boolean_value(CFGID_2543_HOLD, gRfc2543Hold, (const unsigned char *) "rfc2543Hold");
    compare_or_set_boolean_value(CFGID_LOCAL_CFWD_ENABLE, gLocalCfwdEnable, (const unsigned char *) "localCfwdEnable");
    compare_or_set_int_value(CFGID_CONN_MONITOR_DURATION, gConnectionMonitorDuration, (const unsigned char *) "connectionMonitorDuration");
    compare_or_set_int_value(CFGID_CALL_LOG_BLF_ENABLED, gCallLogBlfEnabled, (const unsigned char *) "callLogBlfEnabled");
    compare_or_set_boolean_value(CFGID_RETAIN_FORWARD_INFORMATION, gRetainForwardInformation, (const unsigned char *) "retainForwardInformation");
    compare_or_set_int_value(CFGID_REMOTE_CC_ENABLED, gRemoteCcEnable, (const unsigned char *) "remoteCcEnable");
    compare_or_set_int_value(CFGID_TIMER_KEEPALIVE_EXPIRES, gTimerKeepAliveExpires, (const unsigned char *) "timerKeepAliveExpires");
    compare_or_set_int_value(CFGID_TIMER_SUBSCRIBE_EXPIRES, gTimerSubscribeExpires, (const unsigned char *) "timerSubscribeExpires");
    compare_or_set_int_value(CFGID_TIMER_SUBSCRIBE_DELTA, gTimerSubscribeDelta, (const unsigned char *) "timerSubscribeDelta");
    compare_or_set_int_value(CFGID_TRANSPORT_LAYER_PROT, gTransportLayerProtocol, (const unsigned char *) "transportLayerProtocol");
    compare_or_set_int_value(CFGID_KPML_ENABLED, gKpml, (const unsigned char *) "kpml");
    compare_or_set_boolean_value(CFGID_NAT_ENABLE, gNatEnabled, (const unsigned char *) "natEnabled");
    compare_or_set_string_value(CFGID_NAT_ADDRESS, gNatAddress, (const unsigned char *) "natAddress");
    compare_or_set_int_value(CFGID_VOIP_CONTROL_PORT, gVoipControlPort, (const unsigned char *) "voipControlPort");
    compare_or_set_boolean_value(CFGID_ENABLE_VAD, gAnableVad, (const unsigned char *) "enableVad");
    compare_or_set_boolean_value(CFGID_AUTOANSWER_IDLE_ALTERNATE, gAutoAnswerAltBehavior, (const unsigned char *) "autoAnswerAltBehavior");
    compare_or_set_int_value(CFGID_AUTOANSWER_TIMER, gAutoAnswerTimer, (const unsigned char *) "autoAnswerTimer");
    compare_or_set_boolean_value(CFGID_AUTOANSWER_OVERRIDE, gAutoAnswerOverride, (const unsigned char *) "autoAnswerOverride");
    compare_or_set_int_value(CFGID_OFFHOOK_TO_FIRST_DIGIT_TIMER, gOffhookToFirstDigitTimer, (const unsigned char *) "offhookToFirstDigitTimer");
    compare_or_set_int_value(CFGID_CALL_WAITING_SILENT_PERIOD, gSilentPeriodBetweenCallWaitingBursts, (const unsigned char *) "silentPeriodBetweenCallWaitingBursts");
    compare_or_set_int_value(CFGID_RING_SETTING_BUSY_POLICY, gRingSettingBusyStationPolicy, (const unsigned char *) "ringSettingBusyStationPolicy");
    compare_or_set_int_value (CFGID_BLF_ALERT_TONE_IDLE, gBlfAudibleAlertSettingOfIdleStation, (const unsigned char *) "blfAudibleAlertSettingOfIdleStation");
    compare_or_set_int_value (CFGID_BLF_ALERT_TONE_BUSY, gBlfAudibleAlertSettingOfBusyStation, (const unsigned char *) "blfAudibleAlertSettingOfBusyStation");
    compare_or_set_int_value (CFGID_JOIN_ACROSS_LINES, gJoinAcrossLines, (const unsigned char *) "joinAcrossLines");
    compare_or_set_boolean_value(CFGID_CNF_JOIN_ENABLE, gCnfJoinEnabled, (const unsigned char *) "cnfJoinEnabled");
    compare_or_set_int_value (CFGID_ROLLOVER, gRollover, (const unsigned char *) "rollover");
    compare_or_set_boolean_value(CFGID_XFR_ONHOOK_ENABLED, gTransferOnhookEnabled, (const unsigned char *) "transferOnhookEnabled");
    compare_or_set_int_value(CFGID_DSCP_AUDIO, gDscpForAudio, (const unsigned char *) "dscpForAudio");
    compare_or_set_int_value(CFGID_DSCP_VIDEO, gDscpVideo, (const unsigned char *) "dscpVideo");
    compare_or_set_int_value(CFGID_INTER_DIGIT_TIMER, gT302Timer, (const unsigned char *) "T302Timer");

    // TODO(emannion): You had line=1; line<= ....
    // Debugging suggests that alghouth *line* is 1-indexed, the config entries
    // are 1-indexed. See. config_get_line_id().
    // You may want to rewrite this in terms of config_get_line_id().
    // Please check -- EKR
    for(line = 0; line < MAX_REG_LINES; line++) {

 	    compare_or_set_int_value(CFGID_LINE_INDEX + line, gLineIndex, (const unsigned char *)"lineIndex");
        compare_or_set_int_value(CFGID_LINE_FEATURE + line, gFeatureID, (const unsigned char *) "featureID");
        compare_or_set_string_value(CFGID_PROXY_ADDRESS + line, gProxy, (const unsigned char *) "proxy");
        compare_or_set_int_value(CFGID_PROXY_PORT + line, gPort, (const unsigned char *) "port");

        if ( apply_config == FALSE )  {
            ccsnap_set_line_label(line+1, "LINELABEL");
        }

        compare_or_set_string_value(CFGID_LINE_NAME + line, sipUser, (const unsigned char *) "name");
        compare_or_set_string_value(CFGID_LINE_DISPLAYNAME + line, gDisplayName, (const unsigned char *) "displayName");
        compare_or_set_string_value(CFGID_LINE_MESSAGES_NUMBER + line, gMessagesNumber, (const unsigned char *) "messagesNumber");
        compare_or_set_boolean_value(CFGID_LINE_FWD_CALLER_NAME_DIPLAY + line, gCallerName, (const unsigned char *) "callerName");
        compare_or_set_boolean_value(CFGID_LINE_FWD_CALLER_NUMBER_DIPLAY + line, gCallerNumber, (const unsigned char *) "callerNumber");
        compare_or_set_boolean_value(CFGID_LINE_FWD_REDIRECTED_NUMBER_DIPLAY + line, gRedirectedNumber, (const unsigned char *) "redirectedNumber");
        compare_or_set_boolean_value(CFGID_LINE_FWD_DIALED_NUMBER_DIPLAY + line, gDialedNumber, (const unsigned char *) "dialedNumber");
        compare_or_set_byte_value(CFGID_LINE_MSG_WAITING_LAMP + line, gMessageWaitingLampPolicy, (const unsigned char *) "messageWaitingLampPolicy");
        compare_or_set_byte_value(CFGID_LINE_MESSAGE_WAITING_AMWI + line, gMessageWaitingAMWI, (const unsigned char *) "messageWaitingAMWI");
        compare_or_set_byte_value(CFGID_LINE_RING_SETTING_IDLE + line, gRingSettingIdle, (const unsigned char *) "ringSettingIdle");
        compare_or_set_byte_value(CFGID_LINE_RING_SETTING_ACTIVE + line, gRingSettingActive, (const unsigned char *) "ringSettingActive");
        compare_or_set_string_value(CFGID_LINE_CONTACT + line, sipUser, (const unsigned char *) "contact");
        compare_or_set_byte_value(CFGID_LINE_AUTOANSWER_ENABLED + line, gAutoAnswerEnabled, (const unsigned char *) "autoAnswerEnabled");
        compare_or_set_byte_value(CFGID_LINE_CALL_WAITING + line, gCallWaiting, (const unsigned char *) "callWaiting");
        compare_or_set_string_value(CFGID_LINE_AUTHNAME + line, sipUser, (const unsigned char *)"authName");
        compare_or_set_string_value(CFGID_LINE_PASSWORD + line, sipPassword, (const unsigned char *)"authPassword");
    }

    compare_or_set_int_value(CFGID_CCM1_SEC_LEVEL, gDeviceSecurityMode,(const unsigned char *)"deviceSecurityMode");
    compare_or_set_int_value(CFGID_CCM1_SIP_PORT, gCcm1_sip_port,(const unsigned char *)"ccm1_sip_port");
    compare_or_set_int_value(CFGID_CCM2_SIP_PORT, gCcm2_sip_port,(const unsigned char *)"ccm2_sip_port");
    compare_or_set_int_value(CFGID_CCM3_SIP_PORT, gCcm3_sip_port, (const unsigned char *)"ccm3_sip_port");


    isSecure = FALSE;
    sstrncpy(ip, "", MAX_SIP_URL_LENGTH);
    sstrncpy(option, "User Specific", MAX_SIP_URL_LENGTH);

    compare_or_set_string_value(CFGID_CCM1_ADDRESS+0, sipDomain, (const unsigned char *) "ccm1_addr");
    compare_or_set_boolean_value(CFGID_CCM1_IS_VALID + 0, gCcm1_isvalid, (const unsigned char *)"ccm1_isvalid");
    compare_or_set_int_value(CFGID_DSCP_FOR_CALL_CONTROL , gDscpCallControl, (const unsigned char *) "DscpCallControl");
    compare_or_set_int_value(CFGID_SPEAKER_ENABLED, gSpeakerEnabled, (const unsigned char *) "speakerEnabled");

    if (apply_config == FALSE) {
        config_get_mac_addr(macaddr);

        CONFIG_DEBUG(CFG_F_PREFIX ": MAC Address IS:  %x:%x:%x:%x:%x:%x  \n",
                                  "config_get_mac_addr", macaddr[0], macaddr[1],
                                   macaddr[2], macaddr[3], macaddr[4], macaddr[5]);

        CC_Config_setArrayValue(CFGID_MY_MAC_ADDR, macaddr, MAC_ADDR_SIZE);
        CC_Config_setArrayValue(CFGID_MY_ACTIVE_MAC_ADDR, macaddr, MAC_ADDR_SIZE);
    }

    CONFIG_DEBUG(CFG_F_PREFIX "%s \n", "config_parse_element", "phoneServices");
    CONFIG_DEBUG(CFG_F_PREFIX "%s \n", "config_parse_element", "versionStamp");
    CONFIG_ERROR(CFG_F_PREFIX "%s new=%s old=%s \n", "config_parser_element", "versionStamp",
        		"1284570837-bbc096ed-7392-427d-9694-5ce49d5c3acb", g_cfg_version_stamp);

    if (apply_config == FALSE) {
        memset(g_cfg_version_stamp, 0, sizeof(g_cfg_version_stamp));
        i = strlen("1284570837-bbc096ed-7392-427d-9694-5ce49d5c3acb");
        if (i > MAX_CFG_VERSION_STAMP_LEN) {
            CONFIG_ERROR(CFG_F_PREFIX "config version %d, bigger than allocated space %d\n", "config_parser_element", i, MAX_CFG_VERSION_STAMP_LEN);
        }

        sstrncpy(g_cfg_version_stamp, "1284570837-bbc096ed-7392-427d-9694-5ce49d5c3acb", sizeof(g_cfg_version_stamp));
    }
    else {
        CONFIG_ERROR(CFG_F_PREFIX "got NULL value for %s\n", "config_parser_element", "versionStamp");
    }

    CONFIG_DEBUG(CFG_F_PREFIX "%s \n", "config_parser_element", "externalNumberMask");
    compare_or_set_string_value(CFGID_CCM_EXTERNAL_NUMBER_MASK, gExternalNumberMask, (const unsigned char *) "externalNumberMask");

    /* Set SIP P2P boolean */
    compare_or_set_boolean_value(CFGID_P2PSIP, gP2PSIP, (const unsigned char *) "p2psip");

    /* Set product version */
    compare_or_set_string_value(CFGID_VERSION, gVersion, (const unsigned char *) "version");

    /* Set rtcp-mux, right now to always true */
    compare_or_set_boolean_value(CFGID_RTCPMUX, gRTCPMUX, (const unsigned char *) "rtcpmux");

    /* Set RTP/SAVPF, right now to always true */
    compare_or_set_boolean_value(CFGID_RTPSAVPF, gRTPSAVPF, (const unsigned char *) "rtpsavpf");

    compare_or_set_boolean_value(CFGID_MAXAVBITRATE, gMAXAVBITRATE, (const unsigned char *) "maxavbitrate");

    compare_or_set_boolean_value(CFGID_MAXCODEDAUDIOBW, gMAXCODEDAUDIOBW, (const unsigned char *) "maxcodedaudiobw");

    compare_or_set_boolean_value(CFGID_USEDTX, gUSEDTX, (const unsigned char *) "usedtx");

    compare_or_set_boolean_value(CFGID_STEREO, gSTEREO, (const unsigned char *) "stereo");

    compare_or_set_boolean_value(CFGID_USEINBANDFEC, gUSEINBANDFEC, (const unsigned char *) "useinbandfec");

    compare_or_set_boolean_value(CFGID_CBR, gCBR, (const unsigned char *) "cbr");

    compare_or_set_boolean_value(CFGID_MAXPTIME, gMAXPTIME, (const unsigned char *) "maxptime");

    compare_or_set_int_value(CFGID_SCTP_PORT, gSCTPPort, (const unsigned char *) "sctp_port");

    compare_or_set_int_value(CFGID_NUM_DATA_STREAMS, gNumDataStreams, (const unsigned char *) "num_data_streams");

    (void) isSecure; // XXX set but not used
    (void) isValid; // XXX set but not used
}

void config_setup_server_address (const char *sipDomain) {
	compare_or_set_string_value(CFGID_CCM1_ADDRESS+0, sipDomain, (const unsigned char *) "ccm1_addr");
}

void config_setup_transport_udp(const cc_boolean is_udp) {
	gTransportLayerProtocol = is_udp ? 2 : 4;
	compare_or_set_int_value(CFGID_TRANSPORT_LAYER_PROT, gTransportLayerProtocol, (const unsigned char *) "transportLayerProtocol");
}

void config_setup_local_voip_control_port(const int voipControlPort) {
	gVoipControlPort = voipControlPort;
	compare_or_set_int_value(CFGID_VOIP_CONTROL_PORT, voipControlPort, (const unsigned char *) "voipControlPort");
}

void config_setup_remote_voip_control_port(const int voipControlPort) {
	gCcm1_sip_port = voipControlPort;
	compare_or_set_int_value(CFGID_CCM1_SIP_PORT, voipControlPort,(const unsigned char *)"ccm1_sip_port");
}

int config_get_local_voip_control_port() {
	return gVoipControlPort;
}

int config_get_remote_voip_control_port() {
	return gCcm1_sip_port;
}

const char* config_get_version() {
	return gVersion;
}

void config_setup_p2p_mode(const cc_boolean is_p2p) {
	gP2PSIP = is_p2p;
	compare_or_set_boolean_value(CFGID_P2PSIP, is_p2p, (const unsigned char *) "p2psip");
}

void config_setup_sdp_mode(const cc_boolean is_sdp) {
	gSDPMODE = is_sdp;
	compare_or_set_boolean_value(CFGID_SDPMODE, is_sdp, (const unsigned char *) "sdpsip");
}

void config_setup_avp_mode(const cc_boolean is_rtpsavpf) {
	gRTPSAVPF = is_rtpsavpf;
	compare_or_set_boolean_value(CFGID_RTPSAVPF, is_rtpsavpf, (const unsigned char *) "rtpsavpf");
}

/**
 * Process/Parse the FCP file if specified in the master config file.
*/
void config_parser_handle_fcp_file (char* fcpTemplateFile)
{

    // if no fcp file specified in master config file, then set the default dialplan
    if (strcmp (fcpTemplateFile, "") == 0)
    {
        CC_Config_setFcp(NULL, 0);
        ccsnap_gen_deviceEvent(CCAPI_DEVICE_EV_CONFIG_CHANGED, CC_DEVICE_ID);

        return;
    }

    ccsnap_gen_deviceEvent(CCAPI_DEVICE_EV_CONFIG_CHANGED, CC_DEVICE_ID);
}


/**
 * Function called as part of registration without using cnf device file download.
 */
int config_setup_main( const char *sipUser, const char *sipPassword, const char *sipDomain)
{
    config_setup_elements(sipUser, sipPassword, sipDomain);
	update_security_mode_and_ports();

    // Take care of Fetch and apply of FCP and DialPlan if configured and necessary
    if (apply_config == FALSE) {
        config_parser_handle_fcp_file (fcpTemplateFile);
    }

    return 0;
}
