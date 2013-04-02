/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _PROT_CONFIGMGR_H_
#define _PROT_CONFIGMGR_H_

#include "cpr_types.h"
#include "phone_types.h"
#include "rtp_defs.h"
#include "ccsip_platform.h"
#include "configmgr.h"
#include "cfgfile_utils.h"
#include "phone_platform_constants.h"
#include "cc_config.h"
#include "cc_constants.h"
#include "ccsdp.h"

#define UNPROVISIONED "UNPROVISIONED"

/*********************************************************
 *
 *  The following parameters set Config system settings
 *  for SIP
 *
 *********************************************************/
#define HWTYPE "SIP"
#define MAX_LINE_NAME_SIZE  128
#define AUTH_NAME_SIZE 129
#define MAX_LINE_PASSWORD_SIZE  32
#define MAX_LINE_DISPLAY_SIZE  32
#define MAX_LINE_CONTACT_SIZE  128
#define MAX_LINE_AUTO_ANS_MODE_SIZE  32
#define MAX_REG_USER_INFO_LEN 32
#define MAX_JOIN_DXFER_POLICY_SIZE 40
#define MAX_EXTERNAL_NUMBER_MASK_SIZE 40

/*********************************************************
 *!!!!!!!!!!!!!!!!!!!!!!WARNING!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 *  TNP SIP Phone Configuration IDs
 *
 *!!!!!!!!!!!!!!!!!!!!!!WARNING!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * The following macro definitions are defined in cc_config.h.
 * Change should be made in the cc_config.h and add reference here.
 *
 *         <------ Original notes ------>
 * Before changing this code, please read the following:
 *
 * The Configuration system for the TNP phones is simply a cache
 * that exists for the GSM/SIP DLL to use.  The property values are
 * sent from Java across the JNI to the cache.  This prevents
 * the SIP and GSM code from having to suffer through a JNI call
 * every time they wish to retrieve a configuration parameter.
 *
 * These ID's need to match the definitions in JplatConfigConstants.java
 *
 * To add a new value to the table,
 * In general, you will have to:
 *
 * 1) Create an index for the new CFG param below
 * 2) Update prot_cfg_table either in prot_cfgmgr_private.h
 *    or in prot_configmgr.c (for line specific params)
 * 3) Update JPlatConfigConstants.h with the new ID
 * 4) Create a property on JAVA side and update it from XML config
 * 5) Update show_cfg_cmd if adding a new line param
 *
 *!!!!!!!!!!!!!!!!!!!!!!WARNING!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *********************************************************/

/* Keep non line specific params here */

#define CFGID_MEDIA_PORT_RANGE_START       CFGID_MEDIA_PORT_RANGE_START_INT
#define CFGID_MEDIA_PORT_RANGE_END         CFGID_MEDIA_PORT_RANGE_END_INT
#define CFGID_CALLERID_BLOCKING            CFGID_CALLERID_BLOCKING_BOOL
#define CFGID_ANONYMOUS_CALL_BLOCK         CFGID_ANONYMOUS_CALL_BLOCK_BOOL
#define CFGID_DND_CALL_ALERT               CFGID_DND_CALL_ALERT_BYTE
#define CFGID_DND_REMINDER_TIMER           CFGID_DND_REMINDER_TIMER_INT
#define CFGID_PREFERRED_CODEC              CFGID_PREFERRED_CODEC_STRING
#define CFGID_DTMF_OUTOFBAND               CFGID_DTMF_OUTOFBAND_STRING
#define CFGID_DTMF_AVT_PAYLOAD             CFGID_DTMF_AVT_PAYLOAD_INT
#define CFGID_DTMF_DB_LEVEL                CFGID_DTMF_DB_LEVEL_INT

#define CFGID_SIP_RETX                     CFGID_SIP_RETX_INT
#define CFGID_SIP_INVITE_RETX              CFGID_SIP_INVITE_RETX_INT
#define CFGID_TIMER_T1                     CFGID_TIMER_T1_INT
#define CFGID_TIMER_T2                     CFGID_TIMER_T2_INT
#define CFGID_TIMER_INVITE_EXPIRES         CFGID_TIMER_INVITE_EXPIRES_INT
#define CFGID_TIMER_REGISTER_EXPIRES       CFGID_TIMER_REGISTER_EXPIRES_INT

#define CFGID_PROXY_REGISTER               CFGID_PROXY_REGISTER_BOOL
#define CFGID_PROXY_BACKUP                 CFGID_PROXY_BACKUP_STRING
#define CFGID_PROXY_BACKUP_PORT            CFGID_PROXY_BACKUP_PORT_INT
#define CFGID_PROXY_EMERGENCY              CFGID_PROXY_EMERGENCY_STRING
#define CFGID_PROXY_EMERGENCY_PORT         CFGID_PROXY_EMERGENCY_PORT_INT
#define CFGID_OUTBOUND_PROXY               CFGID_OUTBOUND_PROXY_STRING
#define CFGID_OUTBOUND_PROXY_PORT          CFGID_OUTBOUND_PROXY_PORT_INT

#define CFGID_NAT_RECEIVED_PROCESSING      CFGID_NAT_RECEIVED_PROCESSING_BOOL
#define CFGID_REG_USER_INFO                CFGID_REG_USER_INFO_STRING
#define CFGID_CNF_JOIN_ENABLE              CFGID_CNF_JOIN_ENABLE_BOOL
#define CFGID_REMOTE_PARTY_ID              CFGID_REMOTE_PARTY_ID_BOOL
#define CFGID_SEMI_XFER                    CFGID_SEMI_XFER_BOOL
#define CFGID_CALL_HOLD_RINGBACK           CFGID_CALL_HOLD_RINGBACK_BOOL
#define CFGID_STUTTER_MSG_WAITING          CFGID_STUTTER_MSG_WAITING_BOOL
/**
 * The CFGID_CFWD_URL was consolidated for RT and CIUS and should be for TNP as well.
 */
#define CFGID_CFWD_URL                     CFGID_CFWD_URL_STRING

#define CFGID_CALL_STATS                   CFGID_CALL_STATS_BOOL
#define CFGID_LOCAL_CFWD_ENABLE            CFGID_LOCAL_CFWD_ENABLE_BOOL
#define CFGID_TIMER_REGISTER_DELTA         CFGID_TIMER_REGISTER_DELTA_INT
#define CFGID_SIP_MAX_FORWARDS             CFGID_SIP_MAX_FORWARDS_INT
#define CFGID_2543_HOLD                    CFGID_2543_HOLD_BOOL

#define CFGID_CCM1_ADDRESS                 CFGID_CCM1_ADDRESS_STRING
#define CFGID_CCM2_ADDRESS                 CFGID_CCM2_ADDRESS_STRING
#define CFGID_CCM3_ADDRESS                 CFGID_CCM3_ADDRESS_STRING

// Note:  IPv6 Not currently supported on Cius
#define CFGID_CCM1_IPV6_ADDRESS            CFGID_CCM1_IPV6_ADDRESS_STRING
#define CFGID_CCM2_IPV6_ADDRESS            CFGID_CCM2_IPV6_ADDRESS_STRING
#define CFGID_CCM3_IPV6_ADDRESS            CFGID_CCM3_IPV6_ADDRESS_STRING

#define CFGID_CCM1_SIP_PORT                CFGID_CCM1_SIP_PORT_INT
#define CFGID_CCM2_SIP_PORT                CFGID_CCM2_SIP_PORT_INT
#define CFGID_CCM3_SIP_PORT                CFGID_CCM3_SIP_PORT_INT

#define CFGID_CCM1_SEC_LEVEL               CFGID_CCM1_SEC_LEVEL_INT
#define CFGID_CCM2_SEC_LEVEL               CFGID_CCM2_SEC_LEVEL_INT
#define CFGID_CCM3_SEC_LEVEL               CFGID_CCM3_SEC_LEVEL_INT

#define CFGID_CCM1_IS_VALID                CFGID_CCM1_IS_VALID_BOOL
#define CFGID_CCM2_IS_VALID                CFGID_CCM2_IS_VALID_BOOL
#define CFGID_CCM3_IS_VALID                CFGID_CCM3_IS_VALID_BOOL

#define CFGID_CCM_TFTP_IP_ADDR             CFGID_CCM_TFTP_IP_ADDR_STRING
#define CFGID_CCM_TFTP_PORT                CFGID_CCM_TFTP_PORT_INT
#define CFGID_CCM_TFTP_IS_VALID            CFGID_CCM_TFTP_IS_VALID_BOOL
#define CFGID_CCM_TFTP_SEC_LEVEL           CFGID_CCM_TFTP_SEC_LEVEL_INT

#define CFGID_CONN_MONITOR_DURATION        CFGID_CONN_MONITOR_DURATION_INT
#define CFGID_CALL_PICKUP_URI              CFGID_CALL_PICKUP_URI_STRING
#define CFGID_CALL_PICKUP_LIST_URI         CFGID_CALL_PICKUP_LIST_URI_STRING
#define CFGID_CALL_PICKUP_GROUP_URI        CFGID_CALL_PICKUP_GROUP_URI_STRING
#define CFGID_MEET_ME_SERVICE_URI          CFGID_MEET_ME_SERVICE_URI_STRING
#define CFGID_CALL_FORWARD_URI             CFGID_CALL_FORWARD_URI_STRING
#define CFGID_ABBREVIATED_DIAL_URI         CFGID_ABBREVIATED_DIAL_URI_STRING
#define CFGID_CALL_LOG_BLF_ENABLED         CFGID_CALL_LOG_BLF_ENABLED_BOOL
#define CFGID_REMOTE_CC_ENABLED            CFGID_REMOTE_CC_ENABLED_BOOL
#define CFGID_RETAIN_FORWARD_INFORMATION   CFGID_RETAIN_FORWARD_INFORMATION_BOOL

#define CFGID_TIMER_KEEPALIVE_EXPIRES      CFGID_TIMER_KEEPALIVE_EXPIRES_INT
#define CFGID_TIMER_SUBSCRIBE_EXPIRES      CFGID_TIMER_SUBSCRIBE_EXPIRES_INT
#define CFGID_TIMER_SUBSCRIBE_DELTA        CFGID_TIMER_SUBSCRIBE_DELTA_INT
#define CFGID_TRANSPORT_LAYER_PROT         CFGID_TRANSPORT_LAYER_PROT_INT
#define CFGID_KPML_ENABLED                 CFGID_KPML_ENABLED_INT

#define CFGID_NAT_ENABLE                   CFGID_NAT_ENABLE_BOOL
#define CFGID_NAT_ADDRESS                  CFGID_NAT_ADDRESS_STRING
#define CFGID_VOIP_CONTROL_PORT            CFGID_VOIP_CONTROL_PORT_INT
#define CFGID_MY_IP_ADDR                   CFGID_MY_IP_ADDR_STRING
#define CFGID_MY_MAC_ADDR                  CFGID_MY_MAC_ADDR_STRING
#define CFGID_ENABLE_VAD                   CFGID_ENABLE_VAD_BOOL

#define CFGID_AUTOANSWER_IDLE_ALTERNATE    CFGID_AUTOANSWER_IDLE_ALTERNATE_BOOL
#define CFGID_AUTOANSWER_TIMER             CFGID_AUTOANSWER_TIMER_INT
#define CFGID_AUTOANSWER_OVERRIDE          CFGID_AUTOANSWER_OVERRIDE_BOOL

#define CFGID_OFFHOOK_TO_FIRST_DIGIT_TIMER CFGID_OFFHOOK_TO_FIRST_DIGIT_TIMER_INT
#define CFGID_CALL_WAITING_SILENT_PERIOD   CFGID_CALL_WAITING_SILENT_PERIOD_INT
#define CFGID_RING_SETTING_BUSY_POLICY     CFGID_RING_SETTING_BUSY_POLICY_INT
#define CFGID_DSCP_FOR_CALL_CONTROL        CFGID_DSCP_FOR_CALL_CONTROL_INT
#define CFGID_SPEAKER_ENABLED              CFGID_SPEAKER_ENABLED_BOOL
#define CFGID_XFR_ONHOOK_ENABLED           CFGID_XFR_ONHOOK_ENABLED_BOOL
#define CFGID_ROLLOVER                     CFGID_ROLLOVER_INT
#define CFGID_LOAD_FILE                    CFGID_LOAD_FILE_STRING

#define CFGID_BLF_ALERT_TONE_IDLE          CFGID_BLF_ALERT_TONE_IDLE_INT
#define CFGID_BLF_ALERT_TONE_BUSY          CFGID_BLF_ALERT_TONE_BUSY_INT
#define CFGID_AUTO_PICKUP_ENABLED          CFGID_AUTO_PICKUP_ENABLED_BOOL

#define CFGID_JOIN_ACROSS_LINES            CFGID_JOIN_ACROSS_LINES_INT

#define CFGID_MY_ACTIVE_MAC_ADDR           CFGID_MY_ACTIVE_MAC_ADDR_STRING
#define CFGID_DSCP_AUDIO                   CFGID_DSCP_AUDIO_INT
#define CFGID_DEVICE_NAME                  CFGID_DEVICE_NAME_STRING
#define CFGID_USER_AGENT                   CFGID_USER_AGENT_STRING
#define CFGID_MODEL_NUMBER                 CFGID_MODEL_NUMBER_STRING
#define CFGID_DSCP_VIDEO                   CFGID_DSCP_VIDEO_INT

#define CFGID_IP_ADDR_MODE                 CFGID_IP_ADDR_MODE_INT
#define CFGID_INTER_DIGIT_TIMER            CFGID_INTER_DIGIT_TIMER_INT

// Note - EMCC not currently supported on CIUS
#define CFGID_EMCC_MODE                    CFGID_EMCC_MODE_BOOL
#define CFGID_VISITING_EM_PORT             CFGID_VISITING_EM_PORT_INT
#define CFGID_VISITING_EM_IP               CFGID_VISITING_EM_IP_STRING

#define CFGID_CCM_EXTERNAL_NUMBER_MASK     CFGID_CCM_EXTERNAL_NUMBER_MASK_STRING
#define CFGID_MEDIA_IP_ADDR                CFGID_MEDIA_IP_ADDR_STRING

/* All non Line specific params should be added above */
/* All Line specific params should be added below */

#define CFGID_LINE_FEATURE                 CFGID_LINE_FEATURE_INT
#define CFGID_LINE_INDEX                   CFGID_LINE_INDEX_INT
#define CFGID_LINE_NAME                    CFGID_LINE_NAME_STRING
#define CFGID_LINE_AUTHNAME                CFGID_LINE_AUTHNAME_STRING
#define CFGID_LINE_PASSWORD                CFGID_LINE_PASSWORD_STRING
#define CFGID_LINE_DISPLAYNAME             CFGID_LINE_DISPLAYNAME_STRING
#define CFGID_LINE_CONTACT                 CFGID_LINE_CONTACT_STRING
#define CFGID_PROXY_ADDRESS                CFGID_PROXY_ADDRESS_STRING
#define CFGID_PROXY_PORT                   CFGID_PROXY_PORT_INT
#define CFGID_LINE_AUTOANSWER_ENABLED      CFGID_LINE_AUTOANSWER_ENABLED_BYTE
#define CFGID_LINE_AUTOANSWER_MODE         CFGID_LINE_AUTOANSWER_MODE_STRING
#define CFGID_LINE_CALL_WAITING            CFGID_LINE_CALL_WAITING_BYTE
#define CFGID_LINE_MSG_WAITING_LAMP        CFGID_LINE_MSG_WAITING_LAMP_BYTE
#define CFGID_LINE_MESSAGE_WAITING_AMWI    CFGID_LINE_MESSAGE_WAITING_AMWI_BYTE
#define CFGID_LINE_RING_SETTING_IDLE       CFGID_LINE_RING_SETTING_IDLE_BYTE
#define CFGID_LINE_RING_SETTING_ACTIVE     CFGID_LINE_RING_SETTING_ACTIVE_BYTE
#define CFGID_LINE_CFWDALL                 CFGID_LINE_CFWDALL_STRING

#define CFGID_LINE_SPEEDDIAL_NUMBER                  CFGID_LINE_SPEEDDIAL_NUMBER_STRING
#define CFGID_LINE_RETRIEVAL_PREFIX                  CFGID_LINE_RETRIEVAL_PREFIX_STRING
#define CFGID_LINE_MESSAGES_NUMBER                   CFGID_LINE_MESSAGES_NUMBER_STRING
#define CFGID_LINE_FWD_CALLER_NAME_DIPLAY            CFGID_LINE_FWD_CALLER_NAME_DIPLAY_BOOL
#define CFGID_LINE_FWD_CALLER_NUMBER_DIPLAY          CFGID_LINE_FWD_CALLER_NUMBER_DIPLAY_BOOL
#define CFGID_LINE_FWD_REDIRECTED_NUMBER_DIPLAY      CFGID_LINE_FWD_REDIRECTED_NUMBER_DIPLAY_BOOL
#define CFGID_LINE_FWD_DIALED_NUMBER_DIPLAY          CFGID_LINE_FWD_DIALED_NUMBER_DIPLAY_BOOL
#define CFGID_LINE_FEATURE_OPTION_MASK               CFGID_LINE_FEATURE_OPTION_MASK_INT
#define CFGID_P2PSIP                                 CFGID_P2PSIP_BOOL
#define CFGID_VERSION                                CFGID_VERSION_STRING
#define CFGID_SDPMODE                                CFGID_SDPMODE_BOOL
#define CFGID_RTCPMUX                                CFGID_RTCPMUX_BOOL
#define CFGID_RTPSAVPF                               CFGID_RTPSAVPF_BOOL
#define CFGID_MAXAVBITRATE                           CFGID_MAXAVBITRATE_BOOL
#define CFGID_MAXCODEDAUDIOBW                        CFGID_MAXCODEDAUDIOBW_BOOL
#define CFGID_USEDTX                                 CFGID_USEDTX_BOOL
#define CFGID_STEREO                                 CFGID_STEREO_BOOL
#define CFGID_USEINBANDFEC                           CFGID_USEINBANDFEC_BOOL
#define CFGID_CBR                                    CFGID_CBR_BOOL
#define CFGID_MAXPTIME                               CFGID_MAXPTIME_BOOL
#define CFGID_SCTP_PORT                              CFGID_SCTP_PORT_INT
#define CFGID_NUM_DATA_STREAMS                       CFGID_NUM_DATA_STREAMS_INT

/*********************************************************
 *
 *  Value Definitions
 *
 *********************************************************/
// Line feature
typedef enum {
    cfgLineFeatureNone                = CC_LINE_FEATURE_NONE,
    cfgLineFeatureRedial              = CC_LINE_FEATURE_REDIAL,
    cfgLineFeatureSpeedDial           = CC_LINE_FEATURE_SPEEDDIAL,
    cfgLineFeatureDN                  = CC_LINE_FEATURE_DN,
    cfgLineFeatureService             = CC_LINE_FEATURE_SERVICE,
    cfgLineFeatureSpeedDialBLF        = CC_LINE_FEATURE_SPEEDDIALBLF,
    cfgLineFeatureMaliciousCallID     = CC_LINE_FEATURE_MALICIOUSCALLID,
    cfgLineFeatureAllCalls            = CC_LINE_FEATURE_ALLCALLS,
    cfgLineFeatureAnswerOldest        = CC_LINE_FEATURE_ANSWEROLDEST,
    cfgLineFeatureServices            = CC_LINE_FEATURE_SERVICES,
    cfgLineFeatureBLF                 = CC_LINE_FEATURE_BLF
} cfgLineFeatureType_e;

/*********************************************************
 *
 *  Function Prototypes
 *
 *********************************************************/
void protocol_cfg_init(void);
void sip_config_get_net_device_ipaddr(cpr_ip_addr_t *ip_addr);
void sip_config_get_net_ipv6_device_ipaddr(cpr_ip_addr_t *ip_addr);
void sip_config_get_nat_ipaddr(cpr_ip_addr_t *ip_addr);
void sip_config_set_nat_ipaddr(cpr_ip_addr_t *ip_address);
uint16_t sip_config_local_supported_codecs_get(rtp_ptype aSupportedCodecs[],
                                               uint16_t supportedCodecsLen);
uint16_t sip_config_video_supported_codecs_get(rtp_ptype aSupportedCodecs[],
                              uint16_t supportedCodecsLen, boolean isOffer);

boolean prot_config_check_line_name(char *line_name);
//const key_table_entry_t * sip_config_local_codec_entry_find(const rtp_ptype codec);
line_t sip_config_get_button_from_line(line_t line);
line_t sip_config_get_line_from_button(line_t button);
boolean sip_config_check_line(line_t line);
line_t sip_config_local_line_get(void);
void sip_config_get_display_name(line_t line, char *buffer, int buffer_len);
line_t sip_config_get_line_by_called_number(line_t start_line, const char *called_number);
int sip_minimum_config_check(void);
void config_set_codec_table(int codec_mask);
int sip_config_get_keepalive_expires();
rtp_ptype sip_config_preferred_codec(void);

#endif /* PROT_CONFIGMGR_H_ */
