/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CC_CONFIG_H_
#define _CC_CONFIG_H_

/* Keep non line specific params here */
#include "cc_constants.h"

#define CC_MAX_CONFIG_LINES     MAX_CONFIG_LINES

#define CFGID_BEGIN_INDEX                      0
#define CFGID_MEDIA_PORT_RANGE_START_INT       CFGID_BEGIN_INDEX           //tag:startMediaPort
#define CFGID_MEDIA_PORT_RANGE_END_INT         CFGID_BEGIN_INDEX + 1       //tag:stopMediaPort
#define CFGID_CALLERID_BLOCKING_BOOL           CFGID_BEGIN_INDEX + 2       //tag:callIdBlocking
#define CFGID_ANONYMOUS_CALL_BLOCK_BOOL        CFGID_BEGIN_INDEX + 3       //tag:anonymousCallBlock
#define CFGID_PREFERRED_CODEC_STRING           CFGID_BEGIN_INDEX + 6       //tag:preferredCodec
#define CFGID_DTMF_OUTOFBAND_STRING            CFGID_BEGIN_INDEX + 7       //tag:dtmfOutofBand
#define CFGID_DTMF_AVT_PAYLOAD_INT             CFGID_BEGIN_INDEX + 8       //tag:dtmfAvtPayload
#define CFGID_DTMF_DB_LEVEL_INT                CFGID_BEGIN_INDEX + 9       //tag:dtmfDbLevel

#define CFGID_SIP_RETX_INT                     CFGID_BEGIN_INDEX + 10      //tag:sipRetx
#define CFGID_SIP_INVITE_RETX_INT              CFGID_BEGIN_INDEX + 11      //tag:sipInviteRetx
#define CFGID_TIMER_T1_INT                     CFGID_BEGIN_INDEX + 12      //tag:timerT1
#define CFGID_TIMER_T2_INT                     CFGID_BEGIN_INDEX + 13      //tag:timerT2
#define CFGID_TIMER_INVITE_EXPIRES_INT         CFGID_BEGIN_INDEX + 14      //tag:timerInviteExpires
#define CFGID_TIMER_REGISTER_EXPIRES_INT       CFGID_BEGIN_INDEX + 15      //tag:timerRegisterExpires

#define CFGID_PROXY_REGISTER_BOOL              CFGID_BEGIN_INDEX + 16      //tag:proxy
#define CFGID_PROXY_BACKUP_STRING              CFGID_BEGIN_INDEX + 17      //tag:backupProxy
#define CFGID_PROXY_BACKUP_PORT_INT            CFGID_BEGIN_INDEX + 18      //tag:backupProxyPort
#define CFGID_PROXY_EMERGENCY_STRING           CFGID_BEGIN_INDEX + 19      //tag:emergencyProxy
#define CFGID_PROXY_EMERGENCY_PORT_INT        CFGID_BEGIN_INDEX + 20       //tag:emergencyProxyPort
#define CFGID_OUTBOUND_PROXY_STRING            CFGID_BEGIN_INDEX + 21      //tag:outboundProxy
#define CFGID_OUTBOUND_PROXY_PORT_INT          CFGID_BEGIN_INDEX + 22      //tag:outboundProxyPort

#define CFGID_NAT_RECEIVED_PROCESSING_BOOL     CFGID_BEGIN_INDEX + 23      //tag:natRecievedProcessing
#define CFGID_REG_USER_INFO_STRING             CFGID_BEGIN_INDEX + 24      //tag:userInfo
#define CFGID_CNF_JOIN_ENABLE_BOOL             CFGID_BEGIN_INDEX + 25      //tag:cnfJoinEnable
#define CFGID_REMOTE_PARTY_ID_BOOL             CFGID_BEGIN_INDEX + 26      //tag:remotePartyID
#define CFGID_SEMI_XFER_BOOL                   CFGID_BEGIN_INDEX + 27      //tag:semiAttendedTransfer
#define CFGID_CALL_HOLD_RINGBACK_BOOL          CFGID_BEGIN_INDEX + 28      //tag:callHoldRingback
#define CFGID_STUTTER_MSG_WAITING_BOOL         CFGID_BEGIN_INDEX + 29      //tag:stutterMsgWaiting
#define CFGID_CFWD_URL_STRING                  CFGID_BEGIN_INDEX + 30      //tag:callForwardURI
#define CFGID_CALL_STATS_BOOL                  CFGID_BEGIN_INDEX + 31      //tag:callStats
#define CFGID_AUTO_ANSWER_BOOL                 CFGID_BEGIN_INDEX + 32      //tag:autoAnswerEnabled
#define CFGID_LOCAL_CFWD_ENABLE_BOOL           CFGID_BEGIN_INDEX + 33      //tag:localCfwdEnable
#define CFGID_TIMER_REGISTER_DELTA_INT         CFGID_BEGIN_INDEX + 34      //tag:timerRegisterDelta
#define CFGID_SIP_MAX_FORWARDS_INT             CFGID_BEGIN_INDEX + 35      //tag:maxRedirects
#define CFGID_2543_HOLD_BOOL                   CFGID_BEGIN_INDEX + 36      //tag:rfc2543Hold

#define CFGID_CCM1_ADDRESS_STRING              CFGID_BEGIN_INDEX + 37      //tag:processNodeName
#define CFGID_CCM2_ADDRESS_STRING              CFGID_BEGIN_INDEX + 38      //tag:processNodeName
#define CFGID_CCM3_ADDRESS_STRING              CFGID_BEGIN_INDEX + 39      //tag:processNodeName

#define CFGID_CCM1_IPV6_ADDRESS_STRING         CFGID_BEGIN_INDEX + 40      //tag:ipv6Addr, Not used
#define CFGID_CCM2_IPV6_ADDRESS_STRING         CFGID_BEGIN_INDEX + 41      //tag:ipv6Addr
#define CFGID_CCM3_IPV6_ADDRESS_STRING         CFGID_BEGIN_INDEX + 42      //tag:ipv6Addr

#define CFGID_CCM1_SIP_PORT_INT                CFGID_BEGIN_INDEX + 43      //tag:sipPort
#define CFGID_CCM2_SIP_PORT_INT                CFGID_BEGIN_INDEX + 44      //tag:sipPort
#define CFGID_CCM3_SIP_PORT_INT                CFGID_BEGIN_INDEX + 45      //tag:sipPort

#define CFGID_CCM1_SEC_LEVEL_INT               CFGID_BEGIN_INDEX + 46      //not from configuration
#define CFGID_CCM2_SEC_LEVEL_INT               CFGID_BEGIN_INDEX + 47      //not from configuration
#define CFGID_CCM3_SEC_LEVEL_INT               CFGID_BEGIN_INDEX + 48      //not from configuration

#define CFGID_CCM1_IS_VALID_BOOL               CFGID_BEGIN_INDEX + 49      //not from configuration
#define CFGID_CCM2_IS_VALID_BOOL               CFGID_BEGIN_INDEX + 50      //not from configuration
#define CFGID_CCM3_IS_VALID_BOOL               CFGID_BEGIN_INDEX + 51      //not from configuration

#define CFGID_CCM_TFTP_IP_ADDR_STRING          CFGID_BEGIN_INDEX + 52      //not from configuration
#define CFGID_CCM_TFTP_PORT_INT                CFGID_BEGIN_INDEX + 53      //not from configuration
#define CFGID_CCM_TFTP_IS_VALID_BOOL           CFGID_BEGIN_INDEX + 54      //not from configuration
#define CFGID_CCM_TFTP_SEC_LEVEL_INT           CFGID_BEGIN_INDEX + 55      //not from configuration

#define CFGID_CONN_MONITOR_DURATION_INT        CFGID_BEGIN_INDEX + 60      //tag:connectionMonitorDuration
#define CFGID_CALL_PICKUP_URI_STRING           CFGID_BEGIN_INDEX + 61      //tag:callPickupURI
#define CFGID_CALL_PICKUP_LIST_URI_STRING      CFGID_BEGIN_INDEX + 62      //tag:callPickupListURI
#define CFGID_CALL_PICKUP_GROUP_URI_STRING     CFGID_BEGIN_INDEX + 63      //tag:callPickupGroupURI
#define CFGID_CALL_FORWARD_URI_STRING          CFGID_BEGIN_INDEX + 65      //tag:callForwardURI
#define CFGID_ABBREVIATED_DIAL_URI_STRING      CFGID_BEGIN_INDEX + 66      //tag:abbreviatedDialURI
#define CFGID_CALL_LOG_BLF_ENABLED_BOOL        CFGID_BEGIN_INDEX + 67      //tag:callLogBlfEnabled
#define CFGID_REMOTE_CC_ENABLED_BOOL           CFGID_BEGIN_INDEX + 68      //tag:remoteCcEnable
#define CFGID_RETAIN_FORWARD_INFORMATION_BOOL  CFGID_BEGIN_INDEX + 69      //tag:retainForwardInformation

#define CFGID_TIMER_KEEPALIVE_EXPIRES_INT      CFGID_BEGIN_INDEX + 70      //tag:timerKeepAliveExpires
#define CFGID_TIMER_SUBSCRIBE_EXPIRES_INT      CFGID_BEGIN_INDEX + 71      //tag:timerSubscribeExpires
#define CFGID_TIMER_SUBSCRIBE_DELTA_INT        CFGID_BEGIN_INDEX + 72      //tag:timerSubscribeDelta
#define CFGID_TRANSPORT_LAYER_PROT_INT         CFGID_BEGIN_INDEX + 73      //tag:transportLayerProtocol
#define CFGID_KPML_ENABLED_INT                 CFGID_BEGIN_INDEX + 74      //tag:kpml

#define CFGID_NAT_ENABLE_BOOL                  CFGID_BEGIN_INDEX + 75      //tag:natEnabled
#define CFGID_NAT_ADDRESS_STRING               CFGID_BEGIN_INDEX + 76      //tag:natAddress
#define CFGID_VOIP_CONTROL_PORT_INT            CFGID_BEGIN_INDEX + 77      //tag:voipControlPort
#define CFGID_MY_IP_ADDR_STRING                CFGID_BEGIN_INDEX + 78      //not applicable
#define CFGID_MY_MAC_ADDR_STRING               CFGID_BEGIN_INDEX + 79      //not applicable
#define CFGID_ENABLE_VAD_BOOL                  CFGID_BEGIN_INDEX + 80      //tag:enableVad

#define CFGID_AUTOANSWER_IDLE_ALTERNATE_BOOL   CFGID_BEGIN_INDEX + 81      //tag:autoAnswerAltBehavior
#define CFGID_AUTOANSWER_TIMER_INT             CFGID_BEGIN_INDEX + 82      //tag:autoAnswerTimer
#define CFGID_AUTOANSWER_OVERRIDE_BOOL         CFGID_BEGIN_INDEX + 83      //tag:autoAnswerOverride

#define CFGID_OFFHOOK_TO_FIRST_DIGIT_TIMER_INT CFGID_BEGIN_INDEX + 84      //tag:offhookToFirstDigitTimer
#define CFGID_CALL_WAITING_SILENT_PERIOD_INT   CFGID_BEGIN_INDEX + 85      //tag:silentPeriodBetweenCallWaitingBursts
#define CFGID_RING_SETTING_BUSY_POLICY_INT     CFGID_BEGIN_INDEX + 86      //tag:ringSettingBusyStationPolicy
#define CFGID_DSCP_FOR_CALL_CONTROL_INT        CFGID_BEGIN_INDEX + 87      //tag:dscpForCm2Dvce
#define CFGID_SPEAKER_ENABLED_BOOL             CFGID_BEGIN_INDEX + 88      //tag:disableSpeaker
#define CFGID_XFR_ONHOOK_ENABLED_BOOL          CFGID_BEGIN_INDEX + 89      //tag:disableSpeaker
#define CFGID_ROLLOVER_INT                     CFGID_BEGIN_INDEX + 90      //tag:rollover
#define CFGID_LOAD_FILE_STRING                 CFGID_BEGIN_INDEX + 91      //not from config file

#define CFGID_BLF_ALERT_TONE_IDLE_INT          CFGID_BEGIN_INDEX + 92      //tag:blfAudibleAlertSettingOfIdleStation
#define CFGID_BLF_ALERT_TONE_BUSY_INT          CFGID_BEGIN_INDEX + 93      //tag:blfAudibleAlertSettingOfBusyStation
#define CFGID_AUTO_PICKUP_ENABLED_BOOL         CFGID_BEGIN_INDEX + 94      //tag:autoCallPickupEnable

#define CFGID_JOIN_ACROSS_LINES_INT            CFGID_BEGIN_INDEX + 95      //tag:joinAcrossLines

#if defined(_TNP_)
#define ROUNDTABLE_INDEX_OFFSET            0
#else
#define CFGID_MY_ACTIVE_MAC_ADDR_STRING        CFGID_BEGIN_INDEX + 96      //not from configuration
#define CFGID_DSCP_AUDIO_INT                   CFGID_BEGIN_INDEX + 97      //tag:dscpForAudio
#define CFGID_DEVICE_NAME_STRING               CFGID_BEGIN_INDEX + 98      //not applicable
#define CFGID_USER_AGENT_STRING                CFGID_BEGIN_INDEX + 99      //not from config file
#define CFGID_MODEL_NUMBER_STRING              CFGID_BEGIN_INDEX + 100     //not from config file
#define CFGID_DSCP_VIDEO_INT                   CFGID_BEGIN_INDEX + 101     //tag:dscpVideo
#define ROUNDTABLE_INDEX_OFFSET            6
#endif

#define CFGID_IP_ADDR_MODE_INT                 CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 96   //Not used
#define CFGID_INTER_DIGIT_TIMER_INT            CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 97   //tag:t302

#define CFGID_EMCC_MODE_BOOL                   CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 98   // Not from config file
#define CFGID_VISITING_EM_PORT_INT             CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 99   // Not from config file
#define CFGID_VISITING_EM_IP_STRING            CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 100  // Not from config file

#define CFGID_JOIN_DXFER_POLICY_STRING         CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 102  //tag:<join-dxfer-policy>used only for RTLite CTI appliation
#define CFGID_CCM_EXTERNAL_NUMBER_MASK_STRING  CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 103  //tag:<externalNumberMask>
#define CFGID_MEDIA_IP_ADDR_STRING             CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 104  //tag:<videoCapability>
#define CFGID_P2PSIP_BOOL                      CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 105
#define CFGID_VERSION_STRING                   CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 106
#define CFGID_SDPMODE_BOOL                     CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 107
#define CFGID_RTCPMUX_BOOL                     CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 108
#define CFGID_RTPSAVPF_BOOL                    CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 109
#define CFGID_MAXAVBITRATE_BOOL                CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 110
#define CFGID_MAXCODEDAUDIOBW_BOOL             CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 111
#define CFGID_USEDTX_BOOL                      CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 112
#define CFGID_STEREO_BOOL                      CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 113
#define CFGID_USEINBANDFEC_BOOL                CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 114
#define CFGID_CBR_BOOL                         CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 115
#define CFGID_MAXPTIME_BOOL                    CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 116
#define CFGID_SCTP_PORT_INT                    CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 117
#define CFGID_NUM_DATA_STREAMS_INT             CFGID_BEGIN_INDEX + ROUNDTABLE_INDEX_OFFSET + 118

/* All non Line specific params should be added above */
/* All Line specific params should be added below */
/*
   Following configuration settings are coming from the <line> element in the configuration file.
   The line configuration is block based. Each configuration item has a block, e.g., the  CFGID_LINE_FEATURE_INT
   has a block with the size equal to the maxinum number of line that a type of phone can be configured with.
   The following depicts the structure of line feature configuration.
    config_table[
        ...
        cfgid_line_feature_id_line_1,//line feature id block start
        ...
        cfgid_line_feature_id_line_max,//line feature id block end
        cfgid_line_index_id_line_1, //line index block start
        ...
        cfgid_line_index_id_line_max,//line index block end
        ...CFGID_LINE_CFWDALL_STRING
        cfgid_line_cfwdall_string_line_max //line cfwd all string block end
        ];

  Based on this, when setting directory name (DN, line name) for the line 3 (line_id = 3),
  the config id should be CFGID_LINE_NAME_STRING + (line_id -1), or CFGID_LINE_NAME_STRING + 2.
  Keep in mind the following config ids are defined for the first line.
 */

#define CFGID_LINE_FEATURE_INT                 CFGID_NUM_DATA_STREAMS_INT + 1                              //tag:featureID
#define CFGID_LINE_INDEX_INT                   CFGID_LINE_FEATURE_INT + CC_MAX_CONFIG_LINES                //tag:lineIndex
#define CFGID_LINE_NAME_STRING                 CFGID_LINE_INDEX_INT + CC_MAX_CONFIG_LINES            //tag:name
#define CFGID_LINE_AUTHNAME_STRING             CFGID_LINE_NAME_STRING + CC_MAX_CONFIG_LINES                //tag:authName
#define CFGID_LINE_PASSWORD_STRING             CFGID_LINE_AUTHNAME_STRING + CC_MAX_CONFIG_LINES            //tag:authPassword
#define CFGID_LINE_DISPLAYNAME_STRING          CFGID_LINE_PASSWORD_STRING + CC_MAX_CONFIG_LINES            //tag:displayName
#define CFGID_LINE_CONTACT_STRING              CFGID_LINE_DISPLAYNAME_STRING + CC_MAX_CONFIG_LINES         //tag:contact
#define CFGID_PROXY_ADDRESS_STRING             CFGID_LINE_CONTACT_STRING + CC_MAX_CONFIG_LINES             //tag:proxy
#define CFGID_PROXY_PORT_INT                   CFGID_PROXY_ADDRESS_STRING + CC_MAX_CONFIG_LINES            //tag:port
#define CFGID_LINE_AUTOANSWER_ENABLED_BYTE     CFGID_PROXY_PORT_INT + CC_MAX_CONFIG_LINES                  //tag:autoAnswerEnabled
#define CFGID_LINE_AUTOANSWER_MODE_STRING      CFGID_LINE_AUTOANSWER_ENABLED_BYTE + CC_MAX_CONFIG_LINES    //tag:autoAnswerMode
#define CFGID_LINE_CALL_WAITING_BYTE           CFGID_LINE_AUTOANSWER_MODE_STRING + CC_MAX_CONFIG_LINES     //tag:callWaiting
#define CFGID_LINE_SHARED_LINE_BOOL            CFGID_LINE_CALL_WAITING_BYTE + CC_MAX_CONFIG_LINES          //tag:sharedLine
#define CFGID_LINE_MSG_WAITING_LAMP_BYTE       CFGID_LINE_SHARED_LINE_BOOL + CC_MAX_CONFIG_LINES           //tag:messageWaitingLampPolicy
#define CFGID_LINE_MESSAGE_WAITING_AMWI_BYTE   CFGID_LINE_MSG_WAITING_LAMP_BYTE + CC_MAX_CONFIG_LINES      //tag:messageWaitingAMWI
#define CFGID_LINE_RING_SETTING_IDLE_BYTE      CFGID_LINE_MESSAGE_WAITING_AMWI_BYTE + CC_MAX_CONFIG_LINES  //tag:ringSettingIdle
#define CFGID_LINE_RING_SETTING_ACTIVE_BYTE    CFGID_LINE_RING_SETTING_IDLE_BYTE + CC_MAX_CONFIG_LINES     //tag:ringSettingActive
#define CFGID_LINE_CFWDALL_STRING              CFGID_LINE_RING_SETTING_ACTIVE_BYTE + CC_MAX_CONFIG_LINES           //not from configuration

#define CFGID_LINE_SPEEDDIAL_NUMBER_STRING                CFGID_LINE_CFWDALL_STRING + MAX_CONFIG_LINES
#define CFGID_LINE_RETRIEVAL_PREFIX_STRING                CFGID_LINE_SPEEDDIAL_NUMBER_STRING + MAX_CONFIG_LINES
#define CFGID_LINE_MESSAGES_NUMBER_STRING                 CFGID_LINE_RETRIEVAL_PREFIX_STRING + MAX_CONFIG_LINES
#define CFGID_LINE_FWD_CALLER_NAME_DIPLAY_BOOL            CFGID_LINE_MESSAGES_NUMBER_STRING + MAX_CONFIG_LINES
#define CFGID_LINE_FWD_CALLER_NUMBER_DIPLAY_BOOL          CFGID_LINE_FWD_CALLER_NAME_DIPLAY_BOOL + MAX_CONFIG_LINES
#define CFGID_LINE_FWD_REDIRECTED_NUMBER_DIPLAY_BOOL      CFGID_LINE_FWD_CALLER_NUMBER_DIPLAY_BOOL + MAX_CONFIG_LINES
#define CFGID_LINE_FWD_DIALED_NUMBER_DIPLAY_BOOL          CFGID_LINE_FWD_REDIRECTED_NUMBER_DIPLAY_BOOL + MAX_CONFIG_LINES
#define CFGID_LINE_FEATURE_OPTION_MASK_INT                CFGID_LINE_FWD_DIALED_NUMBER_DIPLAY_BOOL + MAX_CONFIG_LINES
#define HLAPI_INDEX_OFFSET                                CFGID_LINE_FEATURE_OPTION_MASK_INT

#define CFGID_PROTOCOL_MAX                     HLAPI_INDEX_OFFSET + MAX_CONFIG_LINES             //For notation only.


#define CFGID_CONFIG_FILE                      1001

/*********************************************************
 *
 *  Value Definitions
 *
 *********************************************************/
// Line feature

/**
 * Set the maximum line available for registration
 * @param lines the maximum line could be configured.
 * @return
 */
int CC_Config_SetAvailableLines(cc_lineid_t lines);

/**
 * Sets the integer value for the config property.
 * @param [in] cfgid - config property identifier
 * @param [in] value - integer value to be set.
 * @return
 */
void CC_Config_setIntValue(int cfgid, int value);

/**
 * Sets the boolean value for the config property.
 * @param [in] cfgid - config property identifier
 * @param [in] value - boolean value to be set.
 * @return
 */
void CC_Config_setBooleanValue(int cfgid, cc_boolean value);

/**
 * Sets the string value for the config property.
 * @param [in] cfgid - config property identifier
 * @param [in] value - string value to be set.
 * @return
 */
void CC_Config_setStringValue(int cfgid, const char* value);

/**
 * Sets the byte value for the config property.
 * @param [in] cfgid - config property identifier
 * @param [in] value - byte value to be set.
 * @return
 */
void CC_Config_setByteValue(int cfgid, unsigned char value);

/**
 * Sets the byte array value for the config property.
 * @param [in] cfgid - config property identifier
 * @param [in] byte_array - byte array value to be set.
 * @param [in] length - lenght of array.
 * @return
 */
void CC_Config_setArrayValue(int cfgid, char *byte_array, int length);

/**
 * Set the dialplan file
 * @param dial_plan_string the dial plan content string
 * @param length the length of dial plan string, the maximum size will be 0x2000.
 * @return string dial plan version stamp
 */
char* CC_Config_setDialPlan(const char *dial_plan_string, int len);

/**
 * Set the fcp file
 * @param fcp_plan_)string - path to the fcp plan)
 * @param len - length, the maximum size will be 0xXXXX (tbd- pfg))
 * @return string - fcp plan version stamp
 */

char* CC_Config_setFcp(const char *fcp_plan_string, int len);

#endif /* _CC_CONFIG_H_ */
