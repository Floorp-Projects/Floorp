/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_stdio.h"
#include "cpr_string.h"
#include "cpr_in.h"
#include "util_string.h"
#include "task.h"
#include "upgrade.h"
#include "ccsip_task.h"
#include "config.h"
#include "ccsip_core.h"
#include "prot_configmgr.h"
#include "prot_cfgmgr_private.h"
#include "sip_common_transport.h"
#include "phone_debug.h"
#include "regmgrapi.h"
#include "rtp_defs.h"
#include "vcm.h"
#include "plat_api.h"

#define MAX_TOS_VALUE            5
#define MIN_VOIP_PORT_RANGE   1024
#define MAX_VOIP_PORT_RANGE  65535
#define MAX_AUTO_ANSWER_7960    63
#define MIN_KEEPALIVE_EXPIRES 120
#define MAX_KEEPALIVE_EXPIRES 7200

extern void platform_get_ipv4_address(cpr_ip_addr_t *ip_addr);
extern void platform_get_ipv6_address(cpr_ip_addr_t *ip_addr);
extern boolean Is794x;

static cpr_ip_addr_t redirected_nat_ipaddr = {0,{0}};
static void config_set_current_codec_table(int codec_mask,
                                           rtp_ptype *codec_table);

/*********************************************************
 *
 *  Network Configuration Settings
 *  Look at making these generic and moving them to the
 *  network library...
 *
 *********************************************************/

static void initCfgTblEntry(int index, const char * name, void *addr, int length,
                            parse_func_t parse, print_func_t print,
                            const key_table_entry_t *key)
{
  var_t *table;
  table = &prot_cfg_table[index];

  table->name = name;
  table->addr = addr;
  table->length = length;
  table->parse_func = parse;
  table->print_func = print;
  table->key_table = key;

}

/* Function: protCfgTblInit()
 *
 *  Description: Initializes line specific params in prot_cfg_table
 *
 *  Parameters:
 *
 *  Returns:
 */
void protCfgTblInit()
{
int i;
  memset(&prot_cfg_block, 0, sizeof(prot_cfg_block));
  for (i=0; i< MAX_CONFIG_LINES; i++) {
    initCfgTblEntry(CFGID_LINE_INDEX+i, "Index", CFGVAR(line[i].index), PA_INT, PR_INT, 0);
    initCfgTblEntry(CFGID_LINE_FEATURE+i, "Feat", CFGVAR(line[i].feature), PA_INT, PR_INT, 0);
    initCfgTblEntry(CFGID_PROXY_ADDRESS+i, "ProxyAddr", CFGVAR(line[i].proxy_address), PA_STR, PR_STR, 0);
    initCfgTblEntry(CFGID_PROXY_PORT+i, "ProxyPort", CFGVAR(line[i].proxy_port), PA_INT, PR_INT, 0);
    initCfgTblEntry(CFGID_LINE_CALL_WAITING+i, "CWait", CFGVAR(line[i].call_waiting), PA_INT, PR_INT, 0);
    initCfgTblEntry(CFGID_LINE_AUTOANSWER_ENABLED+i, "AAns", CFGVAR(line[i].autoanswer), PA_INT, PR_INT, 0);
    initCfgTblEntry(CFGID_LINE_AUTOANSWER_MODE+i, "AAnsMode", CFGVAR(line[i].autoanswer_mode), PA_STR, PR_STR, 0);
    initCfgTblEntry(CFGID_LINE_MSG_WAITING_LAMP+i, "MWILamp", CFGVAR(line[i].msg_waiting_lamp), PA_INT, PR_INT, 0);
    initCfgTblEntry(CFGID_LINE_MESSAGE_WAITING_AMWI+i, "AMWI", CFGVAR(line[i].msg_waiting_amwi), PA_INT, PR_INT, 0);
    initCfgTblEntry(CFGID_LINE_RING_SETTING_IDLE+i, "RingIdle", CFGVAR(line[i].ring_setting_idle), PA_INT, PR_INT, 0);
    initCfgTblEntry(CFGID_LINE_RING_SETTING_ACTIVE+i, "RingActive", CFGVAR(line[i].ring_setting_active), PA_INT, PR_INT, 0);
    initCfgTblEntry(CFGID_LINE_NAME+i, "Name", CFGVAR(line[i].name), PA_STR, PR_STR, 0);
    initCfgTblEntry(CFGID_LINE_AUTHNAME+i, "AuthName", CFGVAR(line[i].authname), PA_STR, PR_STR, 0);
    initCfgTblEntry(CFGID_LINE_PASSWORD+i, "Passwd", CFGVAR(line[i].password), PA_STR, PR_STR, 0);
    initCfgTblEntry(CFGID_LINE_DISPLAYNAME+i, "DisplayName", CFGVAR(line[i].displayname), PA_STR, PR_STR, 0);
    initCfgTblEntry(CFGID_LINE_CONTACT+i, "Contact", CFGVAR(line[i].contact), PA_STR, PR_STR, 0);
    initCfgTblEntry(CFGID_LINE_CFWDALL+i, "CfwdAll", CFGVAR(line[i].cfwdall), PA_STR, PR_STR, 0);
    initCfgTblEntry(CFGID_LINE_SPEEDDIAL_NUMBER+i, "speedDialNumber", CFGVAR(line[i].speeddial_number), PA_STR, PR_STR, 0);
    initCfgTblEntry(CFGID_LINE_RETRIEVAL_PREFIX+i, "retrievalPrefix", CFGVAR(line[i].retrieval_prefix), PA_STR, PR_STR, 0);
    initCfgTblEntry(CFGID_LINE_MESSAGES_NUMBER+i, "messagesNumber", CFGVAR(line[i].messages_number), PA_STR, PR_STR, 0);
    initCfgTblEntry(CFGID_LINE_FWD_CALLER_NAME_DIPLAY+i, "callerName", CFGVAR(line[i].fwd_caller_name_display), PA_INT, PR_INT, 0);
    initCfgTblEntry(CFGID_LINE_FWD_CALLER_NUMBER_DIPLAY+i, "callerName", CFGVAR(line[i].fwd_caller_number_display), PA_INT, PR_INT, 0);
    initCfgTblEntry(CFGID_LINE_FWD_REDIRECTED_NUMBER_DIPLAY+i, "redirectedNumber", CFGVAR(line[i].fwd_redirected_number_display), PA_INT, PR_INT, 0);
    initCfgTblEntry(CFGID_LINE_FWD_DIALED_NUMBER_DIPLAY+i, "dialedNumber", CFGVAR(line[i].fwd_dialed_number_display), PA_INT, PR_INT, 0);
    initCfgTblEntry(CFGID_LINE_FEATURE_OPTION_MASK+i, "featureOptionMask", CFGVAR(line[i].feature_option_mask), PA_INT, PR_INT, 0);
  }

  initCfgTblEntry(CFGID_PROTOCOL_MAX, 0, 0, 0, 0, 0, 0);
}

/*
 * sip_config_get_net_device_ipaddr()
 *
 * Get the device IP address.
 * Note: the IP Address is returned in the non-Telecaster
 *       SIP format, which is not byte reversed.
 *       Eg. 0xac2c33f8 = 161.44.51.248
 */
void
sip_config_get_net_device_ipaddr (cpr_ip_addr_t *ip_addr)
{
    cpr_ip_addr_t ip_addr1 = {0,{0}};

    platform_get_ipv4_address(&ip_addr1);
    util_ntohl(ip_addr, &ip_addr1);
}

/*
 * sip_config_get_net_device_ipaddr()
 *
 * Get the device IP address.
 * Note: the IP Address is returned in the non-Telecaster
 *       SIP format, which is not byte reversed.
 *
 */
void
sip_config_get_net_ipv6_device_ipaddr (cpr_ip_addr_t *ip_addr)
{
    cpr_ip_addr_t ip_addr1 = {0,{0}};

    platform_get_ipv6_address(&ip_addr1);
    util_ntohl(ip_addr, &ip_addr1);
}


/*
 * sip_config_get_nat_ipaddr()
 *
 * Get the nat IP address.
 * Note: the IP Address is returned in the non-Telecaster
 *       SIP format, which is not byte reversed.
 *       Eg. 0xac2c33f8 = 161.44.51.248
 */
void
sip_config_get_nat_ipaddr (cpr_ip_addr_t *ip_addr)
{
    cpr_ip_addr_t IPAddress;
    char address[MAX_IPADDR_STR_LEN];
    int dnsErrorCode = 1;

    if (redirected_nat_ipaddr.type == CPR_IP_ADDR_INVALID) {
        config_get_string(CFGID_NAT_ADDRESS, address, sizeof(address));
        if ((cpr_strcasecmp(address, UNPROVISIONED) != 0) && (address[0] != 0)) {
            dnsErrorCode = dnsGetHostByName(address, &IPAddress, 100, 1);
        }

        if (dnsErrorCode == 0) {
            util_ntohl(ip_addr, &IPAddress);
            return ;
        } else {
            /*
             * If the NAT address is not provisioned or
             * unavailable, return the local address instead.
             */
            sip_config_get_net_device_ipaddr(ip_addr);
            return;
        }
    } else {
        *ip_addr = redirected_nat_ipaddr;
        return ;
    }

}

/*
 * sip_config_set_nat_ipaddr()
 *
 * Set the device NAT IP address.
 * Note: the IP Address is returned in the non-Telecaster
 *       SIP format, which is not byte reversed.
 *       Eg. 0xac2c33f8 = 161.44.51.248
 */
void
sip_config_set_nat_ipaddr (cpr_ip_addr_t *ip_address)
{
    redirected_nat_ipaddr = *ip_address;
}

/*********************************************************
 *
 *  SIP Configuration Settings
 *  These should probably be turned into generic config
 *  table "gets/sets" or should be moved to a SIP platform
 *  file.  Maybe ccsip_platform_ui.c since that's where we
 *  have the other SIP Platform code.  In the long run, we
 *  should rename ccsip_platform_ui.c to ccsip_platform.c
 *
 *********************************************************/

/*
 * sip_config_get_line_from_button()
 * Some cases CCM sends down line number instead of button number
 * that has to be mapped to correct button number. This function is
 * called to get actual line number for a given button number
 *
 * Returns: actual line number from given button number
 *
 */
line_t
sip_config_get_line_from_button (line_t button)
{
    line_t     max_lines_allowed;
    uint32_t   line = 0;
    line_t     button_no = 0;

    if (Is794x) {
        max_lines_allowed = MAX_REG_LINES_794X;
    } else {
        max_lines_allowed = MAX_REG_LINES;
    }

    if ((button < 1) || (button > max_lines_allowed)) {
        return (button);
    }

    config_get_line_value(CFGID_LINE_INDEX, &line,
                          sizeof(line), button);

    /* Look for the line number through the configuration
     * <line button="4" lineIndex="3>. If the inddex value is not
     * found then use old way of searching for button. The dolby
     * release of CCM adds index to configuration but older
     * ccm does not support that.
     */

    if (line > 0) {

        return((line_t)line);
    }

    /* Try old way of calculating the line number
     */

    line = 0;
    button_no = 0;

    for (button_no = 1; button_no <= button; button_no++) {

        if (sip_config_check_line(button_no) == FALSE) {
            continue;
        }

        line++;
    }

    return ((line_t)line);
}


/*
 * sip_config_get_button_from_line()
 *
 * Some cases CCM sends down line number instead of button number
 * that has to be mapped to correct button number.
 *
 * Parameters: line - the line instance
 *
 * Returns: line - actual button number
 *
 */
line_t
sip_config_get_button_from_line (line_t line)
{
    line_t     max_lines_allowed;
    line_t     button = 0;
    uint32_t   line_no = 0;

    if (Is794x) {
        max_lines_allowed = MAX_REG_LINES_794X;
    } else {
        max_lines_allowed = MAX_REG_LINES;
    }

    if ((line < 1) || (line > max_lines_allowed)) {
        return (line);
    }

    /* Look for the button number through the configuration
     * <line button="4" lineIndex="3>. If the inddex value is not
     * found then use old way of searching for button. The dolby
     * release of CCM adds index to configuration but older
     * ccm does not support that.
     */

    for (button = 1; button <= max_lines_allowed; button++) {

        config_get_line_value(CFGID_LINE_INDEX, &line_no, sizeof(line_no), button);

        if ((line_t)line_no == line) {
            return(button);
        }
    }

    button = 0;
    line_no = 0;

    /* Nothing has found so far, try old way of calculating the
     * button number
     */
    do {

        if (sip_config_check_line(button) == FALSE) {
            button++;
            continue;
        }

        button++;
        line_no++;

    } while (((line_t)line_no < line) &&
            button <= max_lines_allowed);


    /* Above loop not able to find the correct button number
     * so return value 0
     */
    if (button > max_lines_allowed) {
        return(0);
    }

    return (button - 1);
}


/*
 * sip_config_check_line()
 *
 * Check to see if the indicated line is configured as a DN line
 *
 * Parameters: line - the line instance
 *
 * Returns: TRUE if the indicated line is Valid
 *          FALSE if the indicated line is Invalid
 *
 */
boolean
sip_config_check_line (line_t line)
{
    const char fname[] = "sip_config_check_line";
    char     temp[MAX_LINE_NAME_SIZE];
    uint32_t line_feature;
    line_t   max_lines_allowed;

    if (Is794x) {
        max_lines_allowed = MAX_REG_LINES_794X;
    } else {
        max_lines_allowed = MAX_REG_LINES;
    }

    if ((line < 1) || (line > max_lines_allowed)) {
        if (line != 0) {
            PLAT_ERROR(PLAT_COMMON_F_PREFIX"Invalid Line: %d", fname, line);
        }
        return FALSE;
    }

    config_get_line_string(CFGID_LINE_NAME, temp, line, sizeof(temp));
    if (temp[0] == '\0') {
        return FALSE;
    }

    config_get_line_value(CFGID_LINE_FEATURE, &line_feature,
                          sizeof(line_feature), line);

    if (line_feature != cfgLineFeatureDN) {
        return FALSE;
    }

    return TRUE;
}
/*
 * sip_config_local_line_get()
 *
 * Get the Line setting.
 * Note: The UI has serious problems if there are gaps in the
 * line names.  Therefore, lines must be sequential.  In
 * other words, if lines 1, 2, and 4 have names, this routine
 * will return that two lines are active.  Once Line three is
 * found to be unprovisioned, line 4 will be ignored.
 */
line_t
sip_config_local_line_get (void)
{
    if (Is794x) {
        return (MAX_REG_LINES_794X);
    }
    return (MAX_REG_LINES);
}
/*
 * sip_config_get_keepalive_expires()
 *
 * Returns the keepalive expires configured.
 * The minimum allowed value is returned if
 * configured value is less than the minimum
 * allowed value.If the configured value is
 * greater than the maximum allowed then the
 * maximum allowed value is returned.
 *
 */
int
sip_config_get_keepalive_expires()
{
    int keepalive_interval = 0;

    config_get_value(CFGID_TIMER_KEEPALIVE_EXPIRES, &keepalive_interval,
                         sizeof(keepalive_interval));

    if (keepalive_interval < MIN_KEEPALIVE_EXPIRES) {
        keepalive_interval = MIN_KEEPALIVE_EXPIRES;
        TNP_DEBUG(DEB_F_PREFIX"Keepalive interval less than minimum acceptable.Resetting it to %d",
            DEB_F_PREFIX_ARGS(SIP_KA, "sip_config_get_keepalive_expires"),
            keepalive_interval);
    } else if (keepalive_interval > MAX_KEEPALIVE_EXPIRES) {
        keepalive_interval = MAX_KEEPALIVE_EXPIRES;
        TNP_DEBUG(DEB_F_PREFIX"Keepalive interval more than maximum acceptable.Resetting it to %d",
            DEB_F_PREFIX_ARGS(SIP_KA, "sip_config_get_keepalive_expires"),
            keepalive_interval);
    }

    return keepalive_interval;
}
/*
 * sip_config_get_display_name()
 *
 * Get the display name
 */
void
sip_config_get_display_name (line_t line, char *buffer, int buffer_len)
{

    config_get_line_string(CFGID_LINE_DISPLAYNAME, buffer, line, buffer_len);

    if ((strcmp(buffer, UNPROVISIONED) == 0) || (buffer[0] == '\0')) {
        config_get_line_string(CFGID_LINE_NAME, buffer, line, buffer_len);
    }
}

/**
 * Returns the configured value of preferred codec. The codec may
 * or may not be available by the platform.
 *
 * @param[in] none.
 *
 * @return rtp_ptype of the codec.
 */
rtp_ptype
sip_config_preferred_codec (void)
{
    key_table_entry_t cfg_preferred_codec;

    config_get_value(CFGID_PREFERRED_CODEC, &cfg_preferred_codec,
                     sizeof(cfg_preferred_codec));
    if ((cfg_preferred_codec.name != NULL) &&
        (cfg_preferred_codec.name[0] != '\0')) {
        /* The configuration has preferred codec configured */
        return (cfg_preferred_codec.value);
    }
    /* No preferred codec configured */
    return (RTP_NONE);
}

/**
 * sip_config_local_supported_codecs_get()
 * Get the locally supported codec list. The returned list
 * of codecs will be in the ordered of preference. If there is
 * preferred condec configured and it is available, the
 * preferred codec will be put on the first entry of the
 * returned list.
 *
 * @param[in,out] aSupportedCodecs - pointer to arrary fo the
 *                                   rtp_ptype to store the result of
 *                                   currenlty available codecs.
 * @param[in] supportedCodecsLen   - indicates the number of entry
 *                                   of the aSupportedCodecs.
 *
 * @return number of current codecs available.
 *
 * @pre  (aSupportedCodecs != NULL)
 * @pre  (supportedCodecsLen != 0)
 */
uint16_t
sip_config_local_supported_codecs_get (rtp_ptype aSupportedCodecs[],
                                       uint16_t supportedCodecsLen)
{
    rtp_ptype current_codec_table[MAX_CODEC_ENTRIES+1];
    rtp_ptype *codec;
    rtp_ptype pref_codec;
    uint16_t count = 0;
    int codec_mask;
    boolean preferred_codec_available = FALSE;

    codec_mask = vcmGetAudioCodecList(VCM_DSP_FULLDUPLEX);

    if (!codec_mask) {
        codec_mask = VCM_CODEC_RESOURCE_G711 | VCM_CODEC_RESOURCE_OPUS;
    }

    /*
     * convert the current available codec into the enumerated
     * preferred list.
     */
    current_codec_table[0] = RTP_NONE;
    current_codec_table[MAX_CODEC_ENTRIES] = RTP_NONE;
    config_set_current_codec_table(codec_mask, &current_codec_table[0]);

    /*
     * Get the configured preferred codec. If one is configured,
     * check it to see if currently it can be supported by the
     * platform. If it is configured and is availble to support,
     * put the preferred codec in the first one of the list.
     */
    pref_codec = sip_config_preferred_codec();
    if (pref_codec != RTP_NONE) {
        /*
         * There is a configured preferred codec, check to see if
         * the codec is currently avaible or not.
         */
        codec = &current_codec_table[0];
        while (*codec != RTP_NONE) {
            if (pref_codec == *codec) {
                preferred_codec_available = TRUE;
                break;
            }
            codec++;
        }
    }

    if (preferred_codec_available) {
        /*
         * The preferred codec is configured and the platform
         * currently can support the preferred codec, put it in
         * the first entry.
         */
        aSupportedCodecs[count] = pref_codec;
        count++;
    } else {
        /*
         * Must init or comparison will be made to uninitialized memory.
         * Do not increment count here since we are not adding RTP_NONE
         * as a supported codec. We are only initializing memory to a
         * known value.
         */
        aSupportedCodecs[count] = RTP_NONE;
    }

    codec = &current_codec_table[0];
    while (*codec != RTP_NONE) {
        if (count < supportedCodecsLen) {
            if (*codec != aSupportedCodecs[0]) {
                aSupportedCodecs[count] = *codec;
                count++;
            }
        }
        codec++;
    }
    return count;
}

uint32_t
config_get_video_max_fs(const rtp_ptype codec)
{
  uint32_t max_fs;

  if(vcmGetVideoMaxFs(codec, (int32_t *) &max_fs) == 0) {
    return max_fs;
  }
  return 0;
}

uint32_t
config_get_video_max_fr(const rtp_ptype codec)
{
  uint32_t max_fr;

  if(vcmGetVideoMaxFr(codec, (int32_t *) &max_fr) == 0) {
    return max_fr;
  }
  return 0;
}

/*
 * sip_config_local_supported_codecs_get()
 *
 * Get the locally supported codec list.
 */
uint16_t
sip_config_video_supported_codecs_get (rtp_ptype aSupportedCodecs[],
                          uint16_t supportedCodecsLen, boolean isOffer)
{
    uint16_t count = 0;
    int codec_mask;

    if ( isOffer ) {
        codec_mask = vcmGetVideoCodecList(VCM_DSP_FULLDUPLEX);
    } else {
        /* we are trying to match the answer then we
           already have the rx stream open */
        //codec_mask = vcmGetVideoCodecList(DSP_ENCODEONLY);
        codec_mask = vcmGetVideoCodecList(VCM_DSP_IGNORE);
    }
#ifdef WEBRTC_GONK
    if ( codec_mask & VCM_CODEC_RESOURCE_H264) {
      if (vcmGetVideoMaxSupportedPacketizationMode() == 1) {
        aSupportedCodecs[count] = RTP_H264_P1;
        count++;
      }
      aSupportedCodecs[count] = RTP_H264_P0;
      count++;
    }
    if ( codec_mask & VCM_CODEC_RESOURCE_VP8) {
      aSupportedCodecs[count] = RTP_VP8;
      count++;
    }
#else
    // Apply video codecs with VP8 first on non gonk
    if ( codec_mask & VCM_CODEC_RESOURCE_VP8) {
      aSupportedCodecs[count] = RTP_VP8;
      count++;
    }
    if ( codec_mask & VCM_CODEC_RESOURCE_H264) {
      if (vcmGetVideoMaxSupportedPacketizationMode() == 1) {
        aSupportedCodecs[count] = RTP_H264_P1;
        count++;
      }
      aSupportedCodecs[count] = RTP_H264_P0;
      count++;
    }
#endif
    if ( codec_mask & VCM_CODEC_RESOURCE_H263) {
      aSupportedCodecs[count] = RTP_H263;
      count++;
    }

    return count;
}

/**
 * The function fills in the given codec array based on the
 * platform bit mask of codecs.  Note, that the enumerated list
 * produced is also in the preferred order.
 *
 * @param[in] codec_mask - platform bit mask corresponding to the
 *                         codecs.
 * @param[in/out] codecs - pointer to array of for storing the
 *                         output of the enumerated codec based on
 *                         bit set in the codec_mask.
 *
 * @return  None.
 *
 * @pre     (codec_table != NULL)
 * @pre     storge of codec_table must be last enough to holds
 *          supported codec in the bit mask.
 */
static void
config_set_current_codec_table (int codec_mask, rtp_ptype *codecs)
{
    int idx = 0;

    if (codec_mask & VCM_CODEC_RESOURCE_OPUS) {
        codecs[idx] = RTP_OPUS;
        idx++;
    }

    if (codec_mask & VCM_CODEC_RESOURCE_G711) {
        codecs[idx] = RTP_PCMU;
        idx++;
        codecs[idx] = RTP_PCMA;
        idx++;
    }

    if (codec_mask & VCM_CODEC_RESOURCE_G729A) {
        codecs[idx] = RTP_G729;
        idx++;
    }

    if (codec_mask & VCM_CODEC_RESOURCE_LINEAR) {
        codecs[idx] = RTP_L16;
        idx++;
    }

    if (codec_mask & VCM_CODEC_RESOURCE_G722) {
        codecs[idx] = RTP_G722;
        idx++;
    }

    if (codec_mask & VCM_CODEC_RESOURCE_iLBC) {
        codecs[idx] = RTP_ILBC;
        idx++;
    }

    if (codec_mask & VCM_CODEC_RESOURCE_iSAC) {
        codecs[idx] = RTP_ISAC;
        idx++;
    }

    codecs[idx] = RTP_NONE;

    return;
}

/*
 * sip_config_local_dtmf_dblevels_get()
 *
 * Get the DTMF DB levels
 */
uint32_t
sip_config_local_dtmf_dblevels_get (void)
{
    int value;

    config_get_value(CFGID_DTMF_DB_LEVEL, &value, sizeof(value));
    switch (value) {
    case 0:
        return 0;     // Mute
    case 1:
        return 2900;  // 6 dB down
    case 2:
        return 4096;  // 3 dB down
    case 3:
        return 5786;  // Nominal amplitude
                      // (-8.83 dBm0 to network, -11.83 dBm0 local)
    case 4:
        return 8173;  // 3 dB up
    case 5:
        return 11544; // 6 dB up
    default:
        return 5786;  // Nominal amplitude
    }
}

/*
 *  sip_config_get_line_by_called_number
 *
 *  Return the line by the given called_number
 */
line_t sip_config_get_line_by_called_number (line_t start_line, const char *called_number)
{
    int             i;
    line_t          max_lines;
    line_t          line = 0;
    char            line_name[MAX_LINE_NAME_SIZE];
    char            contact[MAX_LINE_CONTACT_SIZE];
    char           *name;

    max_lines = sip_config_local_line_get();

    /*
     * Check the called number for the E.164 "+"
     * and ignore it if present.
     */
    if (called_number[0] == '+') {
        called_number++;
    }

    for (i = start_line; i <= max_lines; i++) {
        if (sip_config_check_line((line_t)i)) {
            config_get_line_string(CFGID_LINE_NAME, line_name, i,
                                   sizeof(line_name));
            /*
             * Check the configured line name for the E.164 "+"
             * and ignore it if present.
             */
            name = &line_name[0];
            if (line_name[0] == '+') {
                name++;
            }

            if (cpr_strcasecmp(called_number, name) == 0) {
                line = (line_t)i;
                break;
            }
        }
    }

    // If line not found - check with contact list
    if (line == 0) {
        for (i = start_line; i <= max_lines; i++) {
            if (sip_config_check_line((line_t)i)) {
                config_get_line_string(CFGID_LINE_CONTACT, contact, i,
                                       sizeof(contact));
                if (cpr_strcasecmp(called_number, contact) == 0) {
                    line = (line_t)i;
                    break;
                }
            }
        }
    }

    return (line);
}

/*********************************************************
 *
 *  SIP Config API
 *  The routines below with the "prot" prefix are called
 *  by the config system.  The calls that start with "sip"
 *  are helper functions for the SIP implementation of the
 *  "prot" API.
 *
 *********************************************************/

/*
 * sip_minimum_config_check()
 *
 * Return indication if the SIP minimum configuration
 * requirements have been met.
 * Returns 0 if minimum config is met
 * Returns non-zero if minimum config has not been met
 *   (eg. missing at least 1 required parameter)
 */
int
sip_minimum_config_check (void)
{
    char str_val[MAX_IPADDR_STR_LEN];
    char line_name[MAX_LINE_NAME_SIZE];
    int value;

    /*
     * Make sure that line 1 is configured
     */
    config_get_line_string(CFGID_LINE_NAME, line_name, 1, sizeof(line_name));
    if ((strcmp(line_name, UNPROVISIONED) == 0) || (line_name[0] == '\0')) {
        return -1;
    }

    config_get_line_string(CFGID_PROXY_ADDRESS, str_val, 1, MAX_IPADDR_STR_LEN);
    if ((strcmp(str_val, UNPROVISIONED) == 0) || (str_val[0] == '\0')) {
        return -1;
    }

    config_get_line_value(CFGID_PROXY_PORT, &value, sizeof(value), 1);
    if (value == 0) {
        return -1;
    }

    return 0;
}

/*
 * prot_config_change_notify()
 * Let the SIP stack know that a config change has occurred.
 *
 */
int
prot_config_change_notify (int notify_type)
{
    if (SIPTaskProcessConfigChangeNotify(notify_type) < 0) {
//CPR TODO: need reference for
        CCSIP_DEBUG_ERROR(PLAT_COMMON_F_PREFIX"SIPTaskProcessConfigChangeNotify() "
                          "returned error.\n", "prot_config_change_notify");
    }

    return (TRUE);
}

/*
 * prot_config_check_line_name()
 * Makes sure that there are no spaces in the SIP Line Names
 *
 * Returns: TRUE if the Name is Valid
 *          FALSE if the Name is Invalid
 *
 */
boolean
prot_config_check_line_name (char *line_name)
{
    while ((*line_name != ' ') && (*line_name != NUL)) {
        line_name++;
    }

    if (*line_name == ' ') {
        return (FALSE);
    }
    return (TRUE);
}

/*
 * prot_sanity_check_config_settings()
 *
 , Louis* Checks the sanity of the protocol config block values
 * and sets them to defaults if they are incorrect.
 */
int
prot_sanity_check_config_settings (void)
{
    int retval = 0;
    return retval;
}


/*
 * prot_shutdown()
 *
 * Shut down the protocol stack.
 */

void
prot_shutdown (void)
{
    sip_shutdown();
}
