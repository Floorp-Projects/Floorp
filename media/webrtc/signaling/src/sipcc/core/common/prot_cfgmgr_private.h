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

#ifndef _PROT_CFGMGR_PRIVATE_H_
#define _PROT_CFGMGR_PRIVATE_H_

#include "cpr_types.h"
#include "ccapi.h"
#include "ccsip_protocol.h"
//TEMPORARY REMOVAL #include "sntp.h"
#include "configmgr.h"
#include "dtmf.h"
#include "phone_platform_constants.h"

#ifdef SAPP_SAPP_GSM
#define DEFAULT_PROTOCOL         "sip"
#define SCCP_WELL_KNOWN_PORT_STR "2000"
#endif

#define SIP_PLATFORM_CONFIG_DATE_TEMPLATE_24HOUR  "M/D/Y"
#define SIP_PLATFORM_CONFIG_DATE_TEMPLATE_12HOUR  "M/D/YA"

#define DYNAMIC_DTMF_PAYLOAD_MIN 96
#define DYNAMIC_DTMF_PAYLOAD_MAX 127

// Includes 3 CCMS
#define MAX_CCMS 4

#define MAX_CODEC_ENTRIES 10
// updated MAX_LOAD_FILE_NAME to be in sync with XmlDefaultConfigParmObject
#define MAX_LOAD_FILE_NAME 65

/*********************************************************
 *
 *  Config Block Definition
 *    This structure holds all of the parsed configuration
 *    information obtained from the TFTP config file
 *
 *    To add new entries to the config table, please see
 *    the instructions in configmgr.h and prot_configmgr.h
 *
 *  note: IP addresses are internally stored in the
 *        Telecaster "Byte Reversed" order.
 *        Eg. 0xf8332ca1 = 161.44.51.248
 *
 ********************************************************/
typedef struct
{
    int         feature;
    int         index;
    int         maxnumcalls;
    int         busy_trigger;
    char        name[MAX_LINE_NAME_SIZE];
    char        authname[AUTH_NAME_SIZE];
    char        password[MAX_LINE_PASSWORD_SIZE];
    char        displayname[MAX_LINE_NAME_SIZE]; // Actually we allow upto 32 UTF-8 chars which typically needs 32*3 octets.
    char        contact[MAX_LINE_CONTACT_SIZE];
    int         autoanswer;
    char        autoanswer_mode[MAX_LINE_AUTO_ANS_MODE_SIZE];
    int         call_waiting;
    int         msg_waiting_lamp;
    int         msg_waiting_amwi;
    int         ring_setting_idle;
    int         ring_setting_active;
    char        proxy_address[MAX_IPADDR_STR_LEN];
    int         proxy_port;
    char        cfwdall[MAX_URL_LENGTH];
    char        speeddial_number[MAX_LINE_NAME_SIZE];
    char        retrieval_prefix[MAX_LINE_NAME_SIZE];
    char        messages_number[MAX_LINE_NAME_SIZE];
    int         fwd_caller_name_display;
    int         fwd_caller_number_display;
    int         fwd_redirected_number_display;
    int         fwd_dialed_number_display;
    int         feature_option_mask;
} line_cfg_t;

typedef struct
{
    char        address[MAX_IPADDR_STR_LEN];
    char        ipv6address[MAX_IPADDR_STR_LEN];
    int         sip_port;
    int         sec_level;
    int         is_valid;
} ccm_cfg_t;

typedef struct
{
    cpr_ip_addr_t   my_ip_addr;
    uint8_t     my_mac_addr[6];

    line_cfg_t  line[MAX_CONFIG_LINES];
    ccm_cfg_t   ccm[MAX_CCMS];
    int         proxy_register;
    int         sip_retx;              /* SIP retransmission count */
    int         sip_invite_retx;       /* SIP INVITE request retransmission count */
    int         timer_t1;              /* SIP T1 timer value */
    int         timer_t2;              /* SIP T2 timer value */
    int         timer_invite_expires;  /* SIP Expires timer value */
    int         timer_register_expires;
    /*
     * preferred codec is kept as key_table_entry structure. The
     * name field of the key is a primary indication whether the
     * parameter is configured or not. This is because the
     * zero value is a designated value for G711 and the -1 is
     * for no codec. The -1 is not natural value of uninitialized 
     * variable therefore keep the codec as name and value pair.
     * The missing of the name indiates there the parameter is not
     * configured.
     */ 
    key_table_entry_t preferred_codec;
    int         dtmf_db_level;
    DtmfOutOfBandTransport_t dtmf_outofband;
    int         dtmf_avt_payload;
    int         callerid_blocking;
    int         dnd_call_alert;
    int         dnd_reminder_timer;
    int         blf_alert_tone_idle;
    int         blf_alert_tone_busy;
    int         auto_pickup_enabled;
    int         call_hold_ringback;
    int         stutter_msg_waiting;
    int         call_stats;
    int         auto_answer;
    int         anonymous_call_block;
    int         nat_enable;
    char        nat_address[MAX_IPADDR_STR_LEN];
    int         voip_control_port;
    unsigned int media_port_start;
    unsigned int media_port_end;
    char        sync[MAX_SYNC_LEN];
    char        proxy_backup[MAX_IPADDR_STR_LEN];
    char        proxy_emergency[MAX_IPADDR_STR_LEN];
    int         proxy_backup_port;
    int         proxy_emergency_port;
    int         nat_received_processing;

    char        proxy_outbound[MAX_IPADDR_STR_LEN];
    int         proxy_outbound_port;
    char        reg_user_info[MAX_REG_USER_INFO_LEN];
    int         cnf_join_enable;
    int         remote_party_id;
    int         semi_xfer;
    char        cfwd_uri[MAX_URL_LENGTH];
    int         local_cfwd_enable;
    int         timer_register_delta;
    int         rfc_2543_hold;
    int         sip_max_forwards;
    int         conn_monitor_duration;
    char        call_pickup_uri[MAX_URL_LENGTH];
    char        call_pickup_list_uri[MAX_URL_LENGTH];
    char        call_pickup_group_uri[MAX_URL_LENGTH];
    char        meet_me_service_uri[MAX_URL_LENGTH];
    char        call_forward_uri[MAX_URL_LENGTH];
    char        abbreviated_dial_uri[MAX_URL_LENGTH];
    int         call_log_blf_enabled;
    int         remote_cc_enabled;
    int         timer_keepalive_expires;
    int         timer_subscribe_expires;
    int         timer_subscribe_delta;
    int         transport_layer_prot;
    int         kpml;
    int         enable_vad;
    int         autoanswer_idle_alt;
    int         autoanswer_timer;
    int         autoanswer_override;
    int         offhook_to_first_digit;
    int         call_waiting_period;
    int         ring_setting_busy_pol;
    int         dscp_for_call_control;
    int         speaker_enabled;
    int         xfr_onhook_enabled;
    int         retain_forward_information;
    int         rollover;
    int         join_across_lines;
    int         emcc_mode;
    int         visiting_em_port;
    char        visiting_em_ip[MAX_IPADDR_STR_LEN];
    int         ip_addr_mode;
    char        load_file[MAX_LOAD_FILE_NAME];
    int         inter_digit_timer;
    int         dscp_audio;
    int         dscp_video;
    char        deviceName[MAX_REG_USER_INFO_LEN];
    uint8_t     my_active_mac_addr[6];
    char        userAgent[MAX_REG_USER_INFO_LEN];
    char        modelNumber[MAX_REG_USER_INFO_LEN];
    int         srst_is_secure;
    char        join_dxfer_policy[MAX_JOIN_DXFER_POLICY_SIZE];
    char        external_number_mask[MAX_EXTERNAL_NUMBER_MASK_SIZE];
    char        media_ip_addr[MAX_IPADDR_STR_LEN];
    int         p2psip;
    int			roapproxy;
    int			roapclient;
    char        version[4];
} prot_cfg_t;

static prot_cfg_t prot_cfg_block;


/*********************************************************
 *  return (1);
 *  Config Table Keytable Structures
 *
 *  Some config table entries can only contain special  return (1);
 *  values.  Those entries must specify a key table
 *  that contains valid values for the entry.
 *
 *********************************************************/
/*
 * codec_table table is used for parsing configured preferred
 * codec, J-side caches string and it is converted to the enumerated
 * value during reset/restart. The "none" value is currently used
 * by CUCM' TFTP when there is no preferred codec configured and
 * therefore the "none" is included in the table.
 */
static const key_table_entry_t codec_table[] = {
    {"g711ulaw",         RTP_PCMU},
    {"g711alaw",         RTP_PCMA},
    {"g729a",            RTP_G729},
    {"L16",              RTP_L16},
#ifdef CISCOWB_SUPPORTED
    {"CiscoWb",          RTP_CISCOWB},
#endif
    {"g722",             RTP_G722},
    {"iLBC",             RTP_ILBC},
    {"iSAC",             RTP_ISAC},
    {"none",             RTP_NONE}, 
    {0,                  RTP_NONE}
};

static const key_table_entry_t dtmf_outofband_table[] = {
    {"none",             DTMF_OUTOFBAND_NONE},
    {"avt",              DTMF_OUTOFBAND_AVT},
    {"avt_always",       DTMF_OUTOFBAND_AVT_ALWAYS},
    {0,                  0}
};

static const key_table_entry_t user_info_table[] = {
    {"none",             0,},
    {"phone",            1,},
    {"ip",               2,},
    {0,                  0,}
};

/*********************************************************
 *!!!!!!!!!!!!!!!!!!!!!WARNING!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 *  SIP Protocol Config table Variable Name and Type Definitions
 *
 *    This table defines all the possible variables that can
 *    be set in the TFTP config file (if used).
 *    var:        Is the name as it is defined in the ascii
 *                configuration file.
 *    addr:       Is the address of where the parsed variable
 *                is stored into memory
 *    len:        Is the length of the item in kazoo memory.
 *                (Important for things like strings)
 *    parser:     Is a function pointer to a routine that is
 *                used to parse (convert) this particular
 *                variable from ascii form to internal form.
 *    print:      Is a function pointer to a routine that is
 *                used to print (convert) this particular
 *                variable from internal form to ascii form.
 *    keytable:   Defines the table of values that this entry
 *                can have.
 *
 *!!!!!!!!!!!!!!!!!!!!!WARNING!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * Before changing any these, please read the following:
 *
 * This table MUST be kept in sync with the configuration
 * ID enums located in the prot_configmgr.h
 * file.  There is a one-to-one correspondence between those
 * enums and this table.
 *
 * To add or remove config properties, please refer to the
 * file prot_configmgr.h.
 *
 *!!!!!!!!!!!!!!!!!!!!!WARNING!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *********************************************************/
      /*********************************************************
       *
       *  Platform-Specific Configuration Section
       *  (Common across all IP Phone protocols.)
       *
       ********************************************************/

/*     name                                  {addr & len}                    parser  print   */
/*     ------------------------              -----------------               ------  ------  */
var_t prot_cfg_table[CFGID_PROTOCOL_MAX+1] = {
 /* 0 */{"startMediaPort",CFGVAR(media_port_start),       PA_INT, PR_INT, 0},
        {"endMediaPort", CFGVAR(media_port_end),         PA_INT, PR_INT, 0},
        {"callerIdBlocking", CFGVAR(callerid_blocking),      PA_INT, PR_INT, 0},
        {"anonymousCallBlock", CFGVAR(anonymous_call_block),   PA_INT, PR_INT, 0},
        {"dndCallAlert", CFGVAR(dnd_call_alert),         PA_INT, PR_INT, 0},
        {"dndReminderTimer", CFGVAR(dnd_reminder_timer),     PA_INT, PR_INT, 0},
        {"preferredCode", CFGVAR(preferred_codec),        PA_KEYE, PR_KEYE, codec_table},
        {"dtmfOutofBand",CFGVAR(dtmf_outofband),         PA_KEY, PR_KEY, dtmf_outofband_table},
        {"dtmfAvtPayload", CFGVAR(dtmf_avt_payload),       PA_INT, PR_INT, 0},
        {"dtmfDbLevel", CFGVAR(dtmf_db_level),          PA_INT, PR_INT, 0},
        {"sipRetx", CFGVAR(sip_retx),               PA_INT, PR_INT, 0},
 /*11*/ {"sipInviteRetx", CFGVAR(sip_invite_retx),        PA_INT, PR_INT, 0},
        {"timerT1", CFGVAR(timer_t1),               PA_INT, PR_INT, 0},
        {"timerT2", CFGVAR(timer_t2),               PA_INT, PR_INT, 0},
        {"timerInviteExpires", CFGVAR(timer_invite_expires),   PA_INT, PR_INT, 0},
        {"timerRegisterExpires", CFGVAR(timer_register_expires), PA_INT, PR_INT, 0},
        {"registerWithProxy", CFGVAR(proxy_register),         PA_INT, PR_INT, 0},
        {"backupProxy", CFGVAR(proxy_backup),           PA_STR, PR_STR, 0},
        {"backupProxyPort", CFGVAR(proxy_backup_port),      PA_INT, PR_INT, 0},
        {"emergencyProxy", CFGVAR(proxy_emergency),        PA_STR, PR_STR, 0},
        {"emergencyProxyPort", CFGVAR(proxy_emergency_port),   PA_INT, PR_INT, 0},
        {"outboundProxy", CFGVAR(proxy_outbound),         PA_STR, PR_STR, 0},
        {"outboundProxyPort", CFGVAR(proxy_outbound_port),    PA_INT, PR_INT, 0},
        {"natReceivedProcessing", CFGVAR(nat_received_processing),PA_INT, PR_INT, 0},
        {"userInfo", CFGVAR(reg_user_info),          PA_KEY, PR_KEY, user_info_table},
        {"cnfJoinEnable", CFGVAR(cnf_join_enable),        PA_INT, PR_INT, 0},
        {"remotePartyID", CFGVAR(remote_party_id),        PA_INT, PR_INT, 0},
        {"semiAttendedTransfer", CFGVAR(semi_xfer),              PA_INT, PR_INT, 0},
        {"callHoldRingback", CFGVAR(call_hold_ringback),     PA_INT, PR_INT, 0},
        {"stutterMsgWaiting", CFGVAR(stutter_msg_waiting),    PA_INT, PR_INT, 0},
        {"callForwardUri", CFGVAR(cfwd_uri),               PA_STR, PR_STR, 0},
 /*31*/ {"callStats", CFGVAR(call_stats),             PA_INT, PR_INT, 0},
        {"autoAnswer", CFGVAR(auto_answer),            PA_INT, PR_INT, 0},
        {"localCfwdEnable", CFGVAR(local_cfwd_enable),      PA_INT, PR_INT, 0},
        {"timerRegisterDelta", CFGVAR(timer_register_delta),   PA_INT, PR_INT, 0},
        {"MaxRedirects", CFGVAR(sip_max_forwards),       PA_INT, PR_INT, 0},
        {"rfc2543Hold", CFGVAR(rfc_2543_hold),          PA_INT, PR_INT, 0},
        {"ccm1_address", CFGVAR(ccm[0].address),         PA_STR, PR_STR, 0},
        {"ccm2_address", CFGVAR(ccm[1].address),         PA_STR, PR_STR, 0},
        {"ccm3_address", CFGVAR(ccm[2].address),         PA_STR, PR_STR, 0},
        {"ccm1_ipv6address", CFGVAR(ccm[0].ipv6address),     PA_STR, PR_STR, 0},
        {"ccm2_ipv6address", CFGVAR(ccm[1].ipv6address),     PA_STR, PR_STR, 0},
        {"ccm3_ipv6address", CFGVAR(ccm[2].ipv6address),     PA_STR, PR_STR, 0},
        {"ccm1_sipPort", CFGVAR(ccm[0].sip_port),        PA_INT, PR_INT, 0},
        {"ccm2_sipPort", CFGVAR(ccm[1].sip_port),        PA_INT, PR_INT, 0},
        {"ccm3_sipPort", CFGVAR(ccm[2].sip_port),        PA_INT, PR_INT, 0},
        {"ccm1_securityLevel", CFGVAR(ccm[0].sec_level),       PA_INT, PR_INT, 0},
        {"ccm2_securityLevel", CFGVAR(ccm[1].sec_level),       PA_INT, PR_INT, 0},
        {"ccm3_securityLevel", CFGVAR(ccm[2].sec_level),       PA_INT, PR_INT, 0},
        {"ccm1_isValid", CFGVAR(ccm[0].is_valid),        PA_INT, PR_INT, 0},
 /*50*/ {"ccm2_isValid", CFGVAR(ccm[1].is_valid),        PA_INT, PR_INT, 0},
        {"ccm3_isValid", CFGVAR(ccm[2].is_valid),        PA_INT, PR_INT, 0},
        {"ccmTftp_ipAddr", CFGVAR(ccm[3].address),         PA_STR, PR_STR, 0},
        {"ccmTftp_port", CFGVAR(ccm[3].sip_port),        PA_INT, PR_INT, 0},
        {"ccmTftp_isValid", CFGVAR(ccm[3].is_valid),        PA_INT, PR_INT, 0},
        {"ccmTftp_securityLevel", CFGVAR(ccm[3].sec_level),       PA_INT, PR_INT, 0},
        {"ccmSrstIpAddr", CFGVAR(ccm[4].address),         PA_STR, PR_STR, 0},
        {"ccmSrst_sipPort", CFGVAR(ccm[4].sip_port),        PA_INT, PR_INT, 0},
        {"ccmSrst_isValid", CFGVAR(ccm[4].is_valid),        PA_INT, PR_INT, 0},
        {"ccmSrst_securityLevel", CFGVAR(ccm[4].sec_level),       PA_INT, PR_INT, 0},
        {"connectionMonitorDuration", CFGVAR(conn_monitor_duration),  PA_INT, PR_INT, 0},
        {"callPickupURI", CFGVAR(call_pickup_uri),        PA_STR, PR_STR, 0},
        {"callPickupListURI", CFGVAR(call_pickup_list_uri),   PA_STR, PR_STR, 0},
        {"callPickupGroupURI", CFGVAR(call_pickup_group_uri),  PA_STR, PR_STR, 0},
        {"meetMeServiceURI", CFGVAR(meet_me_service_uri),    PA_STR, PR_STR, 0},
        {"callForwardURI", CFGVAR(call_forward_uri),       PA_STR, PR_STR, 0},
        {"abbreviatedDialURI", CFGVAR(abbreviated_dial_uri),   PA_STR, PR_STR, 0},
        {"callLogBlfEnabled", CFGVAR(call_log_blf_enabled),   PA_INT, PR_INT, 0},
        {"remoteCcEnabled", CFGVAR(remote_cc_enabled),      PA_INT, PR_INT, 0},
        {"retainForwardInformation", CFGVAR(retain_forward_information),  PA_INT, PR_INT, 0},
 /*70*/ {"timerKeepaliveExpires", CFGVAR(timer_keepalive_expires),PA_INT, PR_INT, 0},
        {"timerSubscribeExpires", CFGVAR(timer_subscribe_expires),PA_INT, PR_INT, 0},
        {"timerSubscribeDelta", CFGVAR(timer_subscribe_delta),  PA_INT, PR_INT, 0},
        {"transportLayerProtocol", CFGVAR(transport_layer_prot),   PA_INT, PR_INT, 0},
        {"kpml", CFGVAR(kpml),                   PA_INT, PR_INT, 0},
        {"natEnable", CFGVAR(nat_enable),             PA_INT, PR_INT, 0},
        {"natAddress", CFGVAR(nat_address),            PA_STR, PR_STR, 0},
        {"voipControlPort", CFGVAR(voip_control_port),      PA_INT, PR_INT, 0},
        {"myIpAddr", CFGVAR(my_ip_addr),             PA_IP,  PR_IP, 0},
        {"myMacAddr", CFGVAR(my_mac_addr),            PA_STR, PR_MAC, 0},
        {"enableVad", CFGVAR(enable_vad),             PA_INT, PR_INT, 0},
        {"autoAnswerAltBehavior", CFGVAR(autoanswer_idle_alt),    PA_INT, PR_INT, 0},
        {"autoAnswerTimer", CFGVAR(autoanswer_timer),       PA_INT, PR_INT, 0},
        {"autoAnswerOverride", CFGVAR(autoanswer_override),    PA_INT, PR_INT, 0},
        {"offhookToFirstDigitTimer", CFGVAR(offhook_to_first_digit), PA_INT, PR_INT, 0},
        {"silentPeriodBetweenCallWaitingBursts", CFGVAR(call_waiting_period),    PA_INT, PR_INT, 0},
        {"ringSettingBusyStationPolicy", CFGVAR(ring_setting_busy_pol),  PA_INT, PR_INT, 0},
        {"DscpForCm2Dvce", CFGVAR(dscp_for_call_control),  PA_INT, PR_INT, 0},
        {"speakerEnabled", CFGVAR(speaker_enabled),        PA_INT, PR_INT, 0},
        {"transferOnhookEnable", CFGVAR(xfr_onhook_enabled),    PA_INT, PR_INT, 0},
 /*90*/ {"rollover", CFGVAR(rollover),               PA_INT, PR_INT, 0},
        {"loadFileName", CFGVAR(load_file), PA_STR, PR_STR, 0},
        {"blfAlertToneIdle", CFGVAR(blf_alert_tone_idle),PA_INT, PR_INT, 0},
        {"blfAlertToneBusy", CFGVAR(blf_alert_tone_busy),PA_INT, PR_INT, 0},
        {"autoPickupEnable", CFGVAR(auto_pickup_enabled),PA_INT, PR_INT, 0},
        {"joinAcrossLines", CFGVAR(join_across_lines),      PA_INT, PR_INT, 0},
 /*96*/ {"myActiveMacAddr", CFGVAR(my_active_mac_addr),            PA_STR, PR_MAC, 0},
 /*97*/ {"DscpAudio", CFGVAR(dscp_audio), PA_INT, PR_INT, 0},
        {"deviceName",                    CFGVAR(deviceName),         PA_STR, PR_STR, 0},
        {"userAgent",                     CFGVAR(userAgent),          PA_STR, PR_STR, 0},
        {"modelNumber",                   CFGVAR(modelNumber),        PA_STR, PR_STR, 0},
        {"DscpVideo", CFGVAR(dscp_video), PA_INT, PR_INT, 0},
        {"IPAddrMode", CFGVAR(ip_addr_mode),      PA_INT, PR_INT, 0},
        {"interDigitTimer", CFGVAR(inter_digit_timer),      PA_INT, PR_INT, 0},
        {"emccMode",            CFGVAR(emcc_mode),          PA_INT, PR_INT, 0},
        {"visitingEMPort",      CFGVAR(visiting_em_port),  PA_INT, PR_INT, 0},
        {"visitingEMIpAddress", CFGVAR(visiting_em_ip),    PA_STR, PR_STR, 0},
        {"isSRSTSecure", CFGVAR(srst_is_secure), PA_INT, PR_INT, 0},
/*108*/ {"joinDxferPolicy", CFGVAR(join_dxfer_policy), PA_STR, PR_STR, 0},
        {"externalNumberMask", CFGVAR(external_number_mask), PA_STR, PR_STR, 0},
        {"mediaIpAddr", CFGVAR(media_ip_addr),    PA_STR, PR_STR, 0},
        {"p2psip", CFGVAR(p2psip),       PA_INT, PR_INT, 0},
        {"roapproxy", CFGVAR(roapproxy),       PA_INT, PR_INT, 0},
        {"roapclient", CFGVAR(roapclient),       PA_INT, PR_INT, 0},
        {"version", CFGVAR(version),    PA_STR, PR_STR, 0},
        {0,                              0,      0,      0, 0, 0}
  };

#endif /* _PROT_CFGMGR_PRIVATE_H_ */
