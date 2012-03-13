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

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "cpr.h"
#include "phone_debug.h"
#include "cc_debug.h"
#include "phone.h"
#include "cpr_socket.h"
//#include "cc_config.h"
#include "prot_configmgr.h"
#include "debug.h"
/*--------------------------------------------------------------------------
 * Local definitions
 *--------------------------------------------------------------------------
 */

/* NTP related local data */
#define MAX_NTP_MONTH_STR_LEN     4
#define MAX_NTP_MONTH_ARRAY_SIZE  12
#define MAX_NTP_DATE_HDR_STR_LEN  128
#define MAX_NTP_TOKEN_BUF_LEN     16

/* The number of arguments (argc) used in the show command */
#define NUM_OF_SHOW_ARGUMENTS 2

static int last_month = 99;
static char last_month_str[MAX_NTP_MONTH_STR_LEN] = "";
static const char *month_ar[MAX_NTP_MONTH_ARRAY_SIZE] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

extern boolean ccsip_get_ccm_date(char *date_value);

/** The debug Bind table. Contains all the different debug categories used in
 * the core. Each row has the debug category, the name used in the RT/TNP
 * implementation (Jindo) and the variable that is used to start/stop this
 * debug.
 */
debugStruct_t debugBindTable[] = {
    {CC_DEBUG_CCAPP, "ccapp", &g_CCAppDebug},
    {CC_DEBUG_CONFIG_CACHE, "config-cache", &ConfigDebug},
    {CC_DEBUG_SIP_ADAPTER, "sip-adapter", &TNPDebug},
    {CC_DEBUG_CCAPI, "cc", &CCDebug},
    {CC_DEBUG_CC_MSG, "cc-msg", &CCDebugMsg},
    {CC_DEBUG_FIM, "fim", &FIMDebug},
    {CC_DEBUG_FSM, "fsm", &FSMDebugSM},
    {CC_DEBUG_AUTH, "auth", &AuthDebug},
    {CC_DEBUG_GSM, "gsm", &GSMDebug},
    {CC_DEBUG_LSM, "lsm", &LSMDebug},
    {CC_DEBUG_FSM_CAC, "fsm-cac", &g_cacDebug},
    {CC_DEBUG_DCSM, "dcsm", &g_dcsmDebug},
    {CC_DEBUG_SIP_TASK, "sip-task",      &SipDebugTask},
    {CC_DEBUG_SIP_STATE, "sip-state",     &SipDebugState},
    {CC_DEBUG_SIP_MSG, "sip-messages",  &SipDebugMessage},
    {CC_DEBUG_SIP_REG_STATE, "sip-reg-state", &SipDebugRegState},
    {CC_DEBUG_SIP_TRX, "sip-trx",       &SipDebugTrx},
    {CC_DEBUG_TIMERS, "timers",        &TMRDebug},
    {CC_DEBUG_CCDEFAULT, "ccdefault",     &g_DEFDebug},
    {CC_DEBUG_DIALPLAN, "dialplan", &DpintDebug},
    {CC_DEBUG_KPML, "kpml", &KpmlDebug},
    {CC_DEBUG_SIP_PRESENCE, "sip-presence",  &g_blfDebug},
    {CC_DEBUG_CONFIG_APP, "config-app", &g_configappDebug},
    {CC_DEBUG_CALL_EVENT, "call-event", &CCEVENTDebug},
    {CC_DEBUG_PLAT, "plat", &PLATDebug},
    {CC_DEBUG_NOTIFY, "cc-notify", NULL},
    {CC_DEBUG_CPR_MEMORY, "cpr-memory", NULL}, /* cpr-memory has a func callback*/
    {CC_DEBUG_MAX, "not-used", NULL} /* MUST BE THE LAST ELEMENT */
};

/* A list of the external functions that are used as callback functions in the
 * show command. These functions are invoked whenever a show command is called
 * either by the vendor or the plat code.
 */
extern cc_int32_t fsmcnf_show_cmd(cc_int32_t argc, const char *argv[]);
extern cc_int32_t fsmdef_show_cmd(cc_int32_t argc, const char *argv[]);
extern cc_int32_t fsmxfr_show_cmd(cc_int32_t argc, const char *argv[]);
extern cc_int32_t fsmb2bcnf_show_cmd(cc_int32_t argc, const char *argv[]);
extern cc_int32_t dcsm_show_cmd(cc_int32_t argc, const char *argv[]);
extern cc_int32_t fim_show_cmd(cc_int32_t argc, const char *argv[]);
extern cc_int32_t fsm_show_cmd(cc_int32_t argc, const char *argv[]);
extern cc_int32_t lsm_show_cmd(cc_int32_t argc, const char *argv[]);
extern cc_int32_t show_kpmlmap_cmd(cc_int32_t argc, const char *argv[]);
extern cc_int32_t show_config_cmd(cc_int32_t argc, const char *argv[]);
extern cc_int32_t show_subsmanager_stats(cc_int32_t argc, const char *argv[]);
extern cc_int32_t show_publish_stats(cc_int32_t argc, const char *argv[]);
extern cc_int32_t show_register_cmd(cc_int32_t argc, const char *argv[]);
extern cc_int32_t show_dialplan_cmd(cc_int32_t argc, const char *argv[]);
/* CPR MEMORY ARCHIVE DECLARATIONS. These are considered to be part of core */
extern int32_t cpr_show_memory(int32_t argc, const char *argv[]);
extern int32_t cpr_clear_memory (int32_t argc, const char *argv[]);
extern void debugCprMem(cc_debug_cpr_mem_options_e category, cc_debug_flag_e flag);
extern void debugClearCprMem(cc_debug_clear_cpr_options_e category);
void debugShowCprMem(cc_debug_show_cpr_options_e category);


/** The debug Show table. Contains all the different show categories used in
 * the core. Each row has the show category, the name used in the RT/TNP
 * implementation (Jindo), the callback function assoicated with the show
 * and a boolean. The boolean indicates if this show is tied with the "tech"
 * keyword. A TRUE means this show is also part of the "show tech-sip" command.
 */
debugShowStruct_t debugShowTable[] = {
    {CC_DEBUG_SHOW_FSMCNF, "fsmcnf", fsmcnf_show_cmd, FALSE},
    {CC_DEBUG_SHOW_FSMDEF, "fsmdef", fsmdef_show_cmd, FALSE},
    {CC_DEBUG_SHOW_FSMXFR, "fsmxfr", fsmxfr_show_cmd, FALSE},
    {CC_DEBUG_SHOW_FSMB2BCNF, "fsmb2bcnf", fsmb2bcnf_show_cmd, FALSE},
    {CC_DEBUG_SHOW_DCSM, "dcsm", dcsm_show_cmd, FALSE},
    {CC_DEBUG_SHOW_FIM, "fim", fim_show_cmd, FALSE},
    {CC_DEBUG_SHOW_FSM, "fsm", fsm_show_cmd, FALSE},
    {CC_DEBUG_SHOW_LSM, "lsm", lsm_show_cmd, FALSE},
    {CC_DEBUG_SHOW_KPML, "kpml", show_kpmlmap_cmd, FALSE},
    {CC_DEBUG_SHOW_CONFIG_CACHE, "config-cache", show_config_cmd, TRUE},
    {CC_DEBUG_SHOW_SUBS_STATS, "sip-subscription-statistics", show_subsmanager_stats, TRUE},
    {CC_DEBUG_SHOW_PUBLISH_STATS, "sip-publish-statistics", show_publish_stats, TRUE},
    {CC_DEBUG_SHOW_REGISTER, "register", show_register_cmd, TRUE},
    {CC_DEBUG_SHOW_DIALPLAN, "dialplan", show_dialplan_cmd, TRUE},
    {CC_DEBUG_SHOW_CPR_MEMORY, "cpr-memory", cpr_show_memory, FALSE},
    {CC_DEBUG_SHOW_MAX, "not-used", NULL, FALSE} /* MUST BE THE LAST ELEMENT */
};

/** The debug Clear table. Contains all the different clear categories used in
 * the core/cpr archives. Currently we have only CPR memory. Added table for
 * expansion purposes.
 */
debugClearStruct_t debugClearTable[] = {
    {CC_DEBUG_CLEAR_CPR_MEMORY, "cpr-memory", cpr_clear_memory},
    {CC_DEBUG_CLEAR_MAX, "not-used", NULL} /* MUST BE THE LAST ELEMENT */
};

/*--------------------------------------------------------------------------
 * Global data
 *--------------------------------------------------------------------------
 */


/*--------------------------------------------------------------------------
 * External data references
 * -------------------------------------------------------------------------
 */


/*--------------------------------------------------------------------------
 * External function prototypes
 *--------------------------------------------------------------------------
 */

extern void platform_set_time(int32_t gmt_time);
void SipNtpUpdateClockFromCCM(void);


/*--------------------------------------------------------------------------
 * Local scope function prototypes
 *--------------------------------------------------------------------------
 */


/*
 * This function finds month (0 to 11) from month name
 * listed above in month_ar[]. This is used to convert the
 * date header from CCM to derive time/date.
 */
static boolean
set_month_from_str (char *month_str)
{
    boolean ret_val = FALSE;
    const char * fname = "set_month_from_str";
    int i;

    if (month_str) {
        if (strncmp(month_str, last_month_str, 3) != 0) {
            for (i = 0; i < 12; i++) {
                if (strncmp(month_str, month_ar[i], 3) == 0) {
                    strncpy(last_month_str, month_str, 3);
                    last_month = i;
                    ret_val = TRUE;
                    break;
                }
            }
        } else {
            ret_val = TRUE;
        }
    } else {
        TNP_DEBUG(DEB_F_PREFIX "Input month_str is NULL!!!! \n", DEB_F_PREFIX_ARGS(PLAT_API, fname));
    }
    return (ret_val);
}

/**
 * This function is called from SIP stack when it receives
 * REGISTER response that contains a valid date header.
 * It reads the date header string and converts it into
 * local epoch; then sends it to platform to adjust the
 * date and time. If NTP server is used and in-sync then
 * the time derived from CCM is not used.
 * Date header format: "Wed, 22 Jun 2005 18:26:53 GMT"
 *
 * @param  none
 *
 * @return none
 */
void
SipNtpUpdateClockFromCCM (void)
{
    const char *fname = "SipNtpUpdateClockFromCCM";
    struct tm tm_s, *tm_tmp_ptr;
    time_t epoch = 0;
    time_t epoch_tmp = 0;
    time_t tz_diff_sec = 0;
    char   date_hdr_str[MAX_NTP_DATE_HDR_STR_LEN];
    char  *token[MAX_NTP_TOKEN_BUF_LEN];
    char  *curr_token;
    int    count = 0;
#ifndef _WIN32
    char  *last;
#endif

    if (ccsip_get_ccm_date(date_hdr_str) == TRUE) {

        /* windows does not support strtok_r() but it is needed
         * for CNU multi thread
         */
#ifndef _WIN32
        curr_token = (char *) strtok_r(date_hdr_str, " ,:\t\r\n", &last);
#else
        curr_token = (char *) strtok(date_hdr_str, " ,:\t\r\n");
#endif

        while (curr_token) {
            token[count++] = curr_token;
            if (count > 8) {
                break;
            }

            /* windows does not support strtok_r() but it is needed
             * for CNU multi thread
             */
#ifndef _WIN32
            curr_token = (char *) strtok_r(NULL, " ,:\t\r\n", &last);
#else
            curr_token = (char *) strtok(NULL, " ,:\t\r\n");
#endif

        }
        /* we must have exactly 8 tokens and last token must be "GMT" */
        if ((count == 8) && (strcmp(token[7], "GMT") == 0)) {
            if (set_month_from_str(token[2])) {
                tm_s.tm_mon = last_month;
                (void) sscanf(token[6], "%d", &tm_s.tm_sec);
                (void) sscanf(token[5], "%d", &tm_s.tm_min);
                (void) sscanf(token[4], "%d", &tm_s.tm_hour);
                (void) sscanf(token[1], "%d", &tm_s.tm_mday);
                (void) sscanf(token[3], "%d", &tm_s.tm_year);

                // subtract 1900 from the current year
                tm_s.tm_year = tm_s.tm_year - 1900;

                // no knowledge of DST
                tm_s.tm_isdst = -1;

                // convert tm to epoch. note that we are still GMT
                epoch = (time_t) mktime(&tm_s);
                if (epoch == (time_t) - 1) {
                    TNP_DEBUG(DEB_F_PREFIX "mktime() returned -1... Not Good\n", DEB_F_PREFIX_ARGS(PLAT_API, fname));
                } else {
                    // pass current (GMT) epoch to gmtime() so it will
                    // give us forward corrected (for timezone) tm data struct
                    tm_tmp_ptr = gmtime(&epoch);
                    tm_tmp_ptr->tm_isdst = -1;

                    // now use the forward corrected tm data struct to get
                    // epoch that will have number of sec added to it equal
                    // to the local timezone and GMT difference
                    epoch_tmp = (time_t) mktime(tm_tmp_ptr);
                    if (epoch == (time_t) - 1) {
                        TNP_DEBUG(DEB_F_PREFIX "mktime() returned -1... Not Good\n",
                                  DEB_F_PREFIX_ARGS(PLAT_API, fname));
                    } else {
                        // get the GMT and local tz sec difference
                        tz_diff_sec = epoch_tmp - epoch;
                        // subtract the difference from the original GMT epoch
                        epoch -= tz_diff_sec;

                        // call platform function to set time
                        platform_set_time((long) (epoch));
                    }
                }
            }
        }
    }
}


/*
 * MISC platform stubs
 * These stubs are mostly NOPs for TNP but they are referred from common code
 * and this avoids ifdefs in the code.
 *
 */
static uint16_t PHNState = STATE_CONNECTED;

/* ccsip_core.o */
uint16_t
PHNGetState (void)
{
    return (PHNState);
}

void
PHNChangeState (uint16_t state)
{
    PHNState = state;
}

/* ccsip_platform.o */
void
phone_reset (DeviceResetType resetType)
{
    return;
}


/* 
 * Methods below should be moved to plat as they are exported as an external API.
 * For now keeping all miscellaneous methods here.
 */
extern void config_get_value (int id, void *buffer, int length);

/*logger.c */

/*
 * Clear an entry or multiple entries in the
 * log table.  The passed in log message is used
 * for partial matching, in order to figure out
 * what logs need to be cleared.
 *
 * The TNP Status Message screen does not have any
 * facility to "clear" log messages.
 */
void
log_clear (int msg)
{
}


/**
 *
 * Indicates if the (preferred) network interface has changed (e.g., dock/undock)
 *
 * @param  none
 *
 *
 * @return true if the (preferred) network interface has changed since last query
 * @return false if the (preferred) network interface has not changed
 */
boolean plat_is_network_interface_changed (void)
{
    return(FALSE);
}


/**
 * give the platform IPV6 IP address.
 *
 * @param[in/out] ip_addr - pointer to the cpr_ip_addr_t. The
 *                          result IP address will be populated in this
 *                          structure.
 *
 * @return None.
 */
void
platform_get_ipv6_address (cpr_ip_addr_t *ip_addr)
{
    //config_get_value(CFGID_IP_ADDR_MODE, &ip_mode, sizeof(ip_mode));
    //Todo IPv6: Hack to get the IPV6 address
    //if (ip_mode == CPR_IP_MODE_IPV6 || ip_mode == CPR_IP_MODE_DUAL) {
    //}
    ip_addr->type = CPR_IP_ADDR_IPV6;
    ip_addr->u.ip6.addr.base8[15] = 0x65;
    ip_addr->u.ip6.addr.base8[14] = 0xfb;
    ip_addr->u.ip6.addr.base8[13] = 0xb1;
    ip_addr->u.ip6.addr.base8[12] = 0xfe;
    ip_addr->u.ip6.addr.base8[11] = 0xff;
    ip_addr->u.ip6.addr.base8[10] = 0x11;
    ip_addr->u.ip6.addr.base8[9] = 0x11;
    ip_addr->u.ip6.addr.base8[8] = 0x02;
    ip_addr->u.ip6.addr.base8[7] = 0x01;
    ip_addr->u.ip6.addr.base8[6] = 0x00;
    ip_addr->u.ip6.addr.base8[5] = 0x18;
    ip_addr->u.ip6.addr.base8[4] = 0x0c;
    ip_addr->u.ip6.addr.base8[3] = 0xb8;
    ip_addr->u.ip6.addr.base8[2] = 0x0d;
    ip_addr->u.ip6.addr.base8[1] = 0x01;
    ip_addr->u.ip6.addr.base8[0] = 0x20;

    return;
}

/**
 * give the mac address string
 *
 * @param addr - mac address string (OUTPUT)
 *
 * @return none
 */
void
platform_get_wired_mac_address (unsigned char *addr)
{
    config_get_value(CFGID_MY_MAC_ADDR, addr, 6);
    TNP_DEBUG(DEB_F_PREFIX"Wired MacAddr:from Get Val: %04x:%04x:%04x",
            DEB_F_PREFIX_ARGS(PLAT_API, "platform_get_wired_mac_address"),
              addr[0] * 256 + addr[1], addr[2] * 256 + addr[3],
              addr[4] * 256 + addr[5]);
}

/**
 * Get active mac address if required
 *
 * @param addr - mac address string (OUTPUT)
 *
 * @return none
 */
void
platform_get_active_mac_address (unsigned char *addr)
{
    config_get_value(CFGID_MY_ACTIVE_MAC_ADDR, addr, 6);
    TNP_DEBUG(DEB_F_PREFIX"ActiveMacAddr:from Get Val: %04x:%04x:%04x",
            DEB_F_PREFIX_ARGS(PLAT_API, "platform_get_mac_address"),
              addr[0] * 256 + addr[1], addr[2] * 256 + addr[3],
              addr[4] * 256 + addr[5]);
}

/**
 * give the platform IPV4 IP address.
 *
 * @param[in/out] ip_addr - pointer to the cpr_ip_addr_t. The
 *                          result IP address will be populated in this
 *                          structure.
 *
 * @return  None.
 */
void
platform_get_ipv4_address (cpr_ip_addr_t *ip_addr)
{
    config_get_value(CFGID_MY_IP_ADDR, ip_addr, sizeof(cpr_ip_addr_t));
    ip_addr->type = CPR_IP_ADDR_IPV4;

    return;
}

uint32_t
IPNameCk (char *name, char *addr_error)
{
    char *namePtr = name;
    char string[4] = { 0, 0, 0, 0 };
    int x = 0;
    int i = 0;
    uint32_t temp, ip_addr = 0;
    char ip_addr_out[MAX_IPADDR_STR_LEN];

    /* Check if valid IPv6 address */
    if (cpr_inet_pton(AF_INET6, name, ip_addr_out)) {
        *addr_error = FALSE;
        return TRUE;
    }
    *addr_error = TRUE;
    while (*namePtr != 0) {
        if ((*namePtr >= 0x30) && (*namePtr <= 0x39)) {
            if (x > 2)
                return (0);
            string[x++] = *namePtr++;
        } else {
            if (*namePtr == 0x2e) {
                if (i > 3)
                    return (0);
                namePtr++;
                x = 0;
                if ((temp = atoi(string)) > 255)
                    return (0);
                ip_addr |= temp << (24 - (i * 8));
                string[0] = 0;
                string[1] = 0;
                string[2] = 0;
                i++;
            } else
                return (0);     // not an IP address
        }
    }

    if (i == 3) {
        if ((temp = atoi(string)) > 255)
            return (0);
        ip_addr |= temp;
        *addr_error = FALSE;
        return (ntohl(ip_addr));
    } else {
        return 0;
    }
}

/**
 * Set/Unset a pSIPCC Debug Category
 *
 * The pSIPCC implements this function. The platform code i.e. vendor code
 * calls this function in order to set or unset a debug category
 *
 * @param[in] category - The Debug category
 * @param[in] flag - To enable or disable the debug
 *
 */
void debugSet (cc_debug_category_e category, cc_debug_flag_e flag, ...)
{
    int i = 0; //, sub_cat;
    va_list ap;
    int32_t data = -1;

    va_start(ap, flag);

    while (debugBindTable[i].category != CC_DEBUG_MAX) {
        if (debugBindTable[i].category == category) {
            if (debugBindTable[i].category == CC_DEBUG_CPR_MEMORY) {
                data = va_arg(ap, int32_t);
                if (data != -1) {
                    //sub_cat = atoi(data);
                    switch(data) {
                        case CC_DEBUG_CPR_MEM_TRACKING:
                        case CC_DEBUG_CPR_MEM_POISON:
                            debugCprMem(data, flag);
                            break;
                        default:
                            debugif_printf("Error: Unknown CPR debug sub-category passed in\n");
                    }
                } else {
                    debugif_printf("Error: CPR debug sub-category NOT passed in\n");
                }
            } else {
                *(debugBindTable[i].key) = flag;
            }
            va_end(ap);
            return;
        }
        i++;
    }
    debugif_printf("Error: Unknown debug category passed in\n");
    va_end(ap);
    return;
}

/**
 * Call a pSIPCC Show Category
 *
 * The pSIPCC implements this function. The platform code i.e. vendor code
 * calls this function in order to use a show command
 *
 * @param[in] category - The Show category
 *
 * @return 0 if succesfull
 */
int debugShow(cc_debug_show_options_e category, ...)
{
    const char *showArgc[NUM_OF_SHOW_ARGUMENTS];
    int i = 0, returnCode = 0;
    va_list ap;
    int32_t data = -1;

    va_start(ap, category);

    while (debugShowTable[i].category != CC_DEBUG_SHOW_MAX) {
        if (debugShowTable[i].category == category) {
            if (category == CC_DEBUG_SHOW_FSMDEF) {
                /* Special category for FSMDEF - Show "all" fsm's for vendor */
                showArgc[0] = "all";
                returnCode = debugShowTable[i].callbackFunc(NUM_OF_SHOW_ARGUMENTS-1, showArgc);
            } else if (category == CC_DEBUG_SHOW_CPR_MEMORY) {
                data = va_arg(ap, int32_t);
                if (data != -1) {
                    switch (data) {
                        case CC_DEBUG_SHOW_CPR_CONFIG:
                        case CC_DEBUG_SHOW_CPR_HEAP_GUARD:
                        case CC_DEBUG_SHOW_CPR_STATISTICS:
                        case CC_DEBUG_SHOW_CPR_TRACKING:
                            debugShowCprMem((cc_debug_show_cpr_options_e) data);
                            break;

                        default:
                            debugif_printf("Error: Unknown CPR show sub-category passed in\n");
                    }
                } else {
                            debugif_printf("Error: CPR show sub-category NOT passed in\n");
                }
            } else {
                showArgc[0] = "show";
                showArgc[1] = debugShowTable[i].showName;
                returnCode = debugShowTable[i].callbackFunc(NUM_OF_SHOW_ARGUMENTS, showArgc);
            }
            debugif_printf("\n<EOT>\n");
            va_end(ap);
            return returnCode;
        }
        i++;
    }
    debugif_printf("Error: Unknown show category passed in\n");

    va_end(ap);
    return -1;
}

/**
 * Call the show tech command
 *
 * The pSIPCC implements this function. The platform code i.e. vendor code
 * calls this function in order to do the equivalent of the "show tech-sip"
 *
 */
void debugShowTech()
{
    const char *showArgc[NUM_OF_SHOW_ARGUMENTS];
    int i = 0;

    showArgc[0] = "show";

    while (debugShowTable[i].category != CC_DEBUG_SHOW_MAX) {
        if (debugShowTable[i].showTech == TRUE) {
            showArgc[0] = debugShowTable[i].showName;
            debugShowTable[i].callbackFunc(NUM_OF_SHOW_ARGUMENTS, showArgc);
        }
        i++;
    }
    debugif_printf("\n<EOT>\n");
}

/**
 * Call a pSIPCC Clear Category
 *
 * The pSIPCC implements this function. Vendors need to
 * call this function in order to clear certain information stored in pSIPCC.
 *
 * @param[in] category - The Clear category
 * @param[in] ...     variable arg list
 *
 * @note - The variable parameter list actually consists of only one additional
 * formal argument. This is the second parameter specifying any sub category for
 * clearing information.
 * Currently only CC_DEBUG_CLEAR_CPR_MEMORY has sub-categories. The sub-categories are
 * defined in cc_debug_clear_cpr_options_e.
 * @return 0 if succesfull
 */
int debugClear(cc_debug_clear_options_e category, ...)
{
    int i = 0, returnCode = 0 ;
    va_list ap;
    int32_t data = -1;

    va_start(ap, category);

    /* Only the clear "cpr-memory" is present right now */
    if (debugClearTable[i].category == category) {
        data = va_arg(ap, int32_t);
        if (data != -1) {
            switch (data) {
                case CC_DEBUG_CLEAR_CPR_TRACKING:
                case CC_DEBUG_CLEAR_CPR_STATISTICS:
                    debugClearCprMem((cc_debug_clear_cpr_options_e) data);
                    break;
                default:
                    debugif_printf("Error: Unknown CPR clear sub-category passed in\n");
            }
            debugif_printf("\n<EOT>\n");
        } else {
            debugif_printf("Error: CPR clear sub-category NOT passed in\n");
        }
    } else {
        debugif_printf("Error: Unknown show category passed in\n");
        returnCode = -1;
    }

    va_end(ap);
    return returnCode;
}
