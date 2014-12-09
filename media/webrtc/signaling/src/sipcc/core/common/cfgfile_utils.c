/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_stdio.h"
#include "cpr_stdlib.h"
#include "cpr_string.h"
#include "cpr_socket.h"
#include "cpr_in.h"
#include <text_strings.h>
#include <cfgfile_utils.h>
#include <config.h>
#include <phone_debug.h>
#include "util_string.h"

#define IN6ADDRSZ   16
#define INT16SZ     2
#define	INADDRSZ	4
#define IS_DIGIT(ch)   ((ch >= '0') && (ch <= '9'))

/*
 * Parse ascii dotted ip address notation into binary representation
 * only parses and makes sure the address is in the form:
 *      digits.digits.digits.digits
 * Requires minimum of 1 digit per each section and digits cannot
 * exceed 255.  It does NOT attempt to validate if the end result
 * is a valid ip address or not (eg. 0.0.0.0) is accepted.
 * The parsed address is returned in the Telecaster "byte reversed"
 * order.  Eg. 0xf8332ca1 = 161.44.51.248
 */
int
str2ip (const char *str, cpr_ip_addr_t *cpr_addr)
{
    uint32_t ip_addr;
    unsigned int num;
    int dot_cnt;
    char ch;
    int digit_flag;
    uint32_t *addr = (uint32_t *)&(cpr_addr->u.ip4);

    dot_cnt = 0;
    num = 0;
    ip_addr = 0;
    digit_flag = 0;
    cpr_addr->type = CPR_IP_ADDR_INVALID;

    while (1) {
        ch = *str++;
        if (!ch)
            break;              /* end of string */
        /*
         * Check for digits 0 through 9
         */
        if (IS_DIGIT(ch)) {
            digit_flag = 1;
            num = num * 10 + (ch - '0');
            if (num > 255) {
                return (1);
            }
            continue;
        } else if (ch == ':') {
            //must be ipv6 address
            cpr_addr->type = CPR_IP_ADDR_IPV6;
            return(cpr_inet_pton(AF_INET6, str, addr));
        }

        /*
         * Check for DOT.  Must also have seen at least 1 digit prior
         */
        if ((ch == '.') && (digit_flag)) {
            dot_cnt++;
            ip_addr = ((ip_addr << 8) | num);
            num = 0;
            digit_flag = 0;
            continue;
        }

        /* if get here invalid dotted IP character or missing digit */
        return (1);
    }

    /*
     * Must have seen 3 dots exactly and at least 1 trailing digit
     */
    if ((dot_cnt != 3) || (!digit_flag)) {
        return (1);
    }

    ip_addr = ((ip_addr << 8) | num);

    ip_addr = ntohl(ip_addr);   /* convert to Telecaster format */
    cpr_addr->type = CPR_IP_ADDR_IPV4;
    *addr = ip_addr;
    return (0);
}


/*
 * Parse an IP address.
 * If the IP address value is set to "" or to "UNPROVISIONED" it
 * is set to its' default value.
 */
int
cfgfile_parse_ip (const var_t *entry, const char *value)
{
// RAC - Defaults will need to be handled on the Java Side.
//    if ((*value == NUL) || (cpr_strcasecmp(value, "UNPROVISIONED") == 0)) {
//        cfgfile_set_default(entry);
//        return (0);
//    } else {
    return (str2ip(value, (cpr_ip_addr_t *) entry->addr));
//    }
}

/*
 * Print (format) an IP address.
 * The IP address to be printed is in the Telecaster "byte reversed"
 * order.  Eg. 0xf8332ca1 = 248.51.44.161
 */
int
cfgfile_print_ip (const var_t *entry, char *buf, int len)
{
    // RT phones receive the IP address in this order: 0xf8332ca1 = 161.44.51.248
    cpr_ip_addr_t *cprIpAddrPtr = (cpr_ip_addr_t *)entry->addr;

    if (cprIpAddrPtr->type == CPR_IP_ADDR_IPV4) {
        sprint_ip(buf, cprIpAddrPtr->u.ip4);
        return 1;
    }

    return 0;
}

/*
 * Print (format) an IP address.
 * The IP address to be printed is in the non-Telecaster "byte reversed"
 * order - which is really network order.  Eg. 0xa12c33f8 = 161.44.51.248
 */
int
cfgfile_print_ip_ntohl (const var_t *entry, char *buf, int len)
{
    uint32_t ip;

    ip = *(uint32_t *) entry->addr;
    return (snprintf(buf, len, get_debug_string(DEBUG_IP_PRINT),
                     ((ip >> 24) & (0xff)), ((ip >> 16) & (0xff)),
                     ((ip >> 8) & (0xff)), ((ip >> 0) & (0xff))));
}

/*
 * parse (copy) an ascii string
 */
int
cfgfile_parse_str (const var_t *entry, const char *value)
{
    int str_len;

    /* fixme: this could use malloc, or offer a different */
    /* fixme: parser routine that does like parse_str_ptr */
    /* fixme: in that case, free the old string and */
    /* fixme: strdup the new string */

    str_len = strlen(value);
    if (str_len + 1 > entry->length) {
        CSFLogError("common", get_debug_string(DEBUG_PARSER_STRING_TOO_LARGE),
                entry->length, str_len);
        return (1);
    }

    /*
     * Copy string into config block
     */
    sstrncpy((char *)entry->addr, value, entry->length);
    return (0);


}

/*
 * Print (format) at string
 */
int
cfgfile_print_str (const var_t *entry, char *buf, int len)
{
    return (snprintf(buf, len, "%s", (char *)entry->addr));
}

/*
 * Parse an ascii integer into binary
 */
int
cfgfile_parse_int (const var_t *entry, const char *value)
{
    unsigned int num;
    char ch;

    num = 0;

    if (strcmp(value, "UNPROVISIONED") == 0) {
        num = 0;
    } else {
        while (1) {
            ch = *value++;
            if (!ch)
                break;          /* end of string */
            /*
             * Check for digits 0 through 9
             */
            if (IS_DIGIT(ch)) {
                num = num * 10 + (ch - '0');
                continue;
            }

            /* if get here invalid decimal character */
            return (1);
        }
    }
    switch (entry->length) {
    case 1:
        *(uint8_t *) entry->addr = (uint8_t) num;
        break;
    case 2:
        *(uint16_t *) entry->addr = (uint16_t) num;
        break;
    case 4:
        *(uint32_t *) entry->addr = num;
        break;
    default:
        *(unsigned int *) entry->addr = num;
        break;
    }

    return (0);
}

/*
 * print (format) an Integer
 */
int
cfgfile_print_int (const var_t *entry, char *buf, int len)
{
    unsigned int value;

    switch (entry->length) {
    case 1:
        value = *(uint8_t *) entry->addr;
        break;
    case 2:
        value = *(uint16_t *) entry->addr;
        break;
    case 4:
        value = *(uint32_t *) entry->addr;
        break;
    default:
        value = *(unsigned int *) entry->addr;
        break;
    }
    return (snprintf(buf, len, "%u", value));
}

/*
 * Parse a keytable.  A key table is a list of keywords.  For each
 * keyword there is an associated enum value (key value).
 * search the keyword table for a matching keyword, and if found
 * set the variable to the matching emum value.
 */
int
cfgfile_parse_key (const var_t *entry, const char *value)
{
    const key_table_entry_t *keytable;

    keytable = entry->key_table;

    if (keytable == NULL) {
        CSFLogError("common", "%s",
          get_debug_string(DEBUG_PARSER_NULL_KEY_TABLE));
        return (1);
    }

// RAC - This (If Needed) Will need to be moved to the Java Side.
//    /* check for nulled out keys and set to the default value */
//    if ((cpr_strcasecmp(value,"UNPROVISIONED") == 0) ||
//        (value[0] == 0)) {
//       CSFLogError("common", get_debug_string(DEBUG_PARSER_SET_DEFAULT),
//                  entry->name, entry->default_value);
//       cfgfile_set_default(entry);
//       return(0);
//    }

    while (keytable->name) {
        if (cpr_strcasecmp(value, keytable->name) == 0) {
            *(unsigned int *) entry->addr = keytable->value;
            return (0);
        }
        keytable++;
    }

    CSFLogError("common", get_debug_string(DEBUG_PARSER_UNKNOWN_KEY), value);
    return (1);
}

/*
 * print (format) a key value.  Search the table for the matching
 * enum type, then format as output the keyname associated with it.
 */
int
cfgfile_print_key (const var_t *entry, char *buf, int len)
{
    const key_table_entry_t *keytable;
    int value;

    keytable = entry->key_table;
    value = *(int *) entry->addr;

    while (keytable->name) {
        if (value == keytable->value) {
            return (snprintf(buf, len, "%s", keytable->name));
        }
        keytable++;
    }

    CSFLogError("common", get_debug_string(DEBUG_PARSER_UNKNOWN_KEY_ENUM),
                value);
    return (0);
}

/*
 * Sprintf an IP address in dotted notation.
 */
int
sprint_ip (char *buf, uint32_t ip)
{
    return (sprintf(buf, get_debug_string(DEBUG_IP_PRINT),
                    ((ip >> 0) & (0xff)), ((ip >> 8) & (0xff)),
                    ((ip >> 16) & (0xff)), ((ip >> 24) & (0xff))));
}

/*
 * print (format) a MAC address
 */
int
cfgfile_print_mac (const var_t *entry, char *buf, int len)
{
    return (snprintf(buf, len, get_debug_string(DEBUG_MAC_PRINT),
                     ((uint8_t *) entry->addr)[0] * 256 +
                     ((uint8_t *) entry->addr)[1],
                     ((uint8_t *) entry->addr)[2] * 256 +
                     ((uint8_t *) entry->addr)[3],
                     ((uint8_t *) entry->addr)[4] * 256 +
                     ((uint8_t *) entry->addr)[5]));
}

/**
 * Parse a keytable.  A key table is a list of keywords.  For each
 * keyword there is an associated enum value (key value).
 * search the keyword table for a matching keyword, and if found
 * save the entire key etnry into the table.
 *
 * @param[in] entry - pointer ot var_t.
 * @param[in] value - pointer to const. string of configuration value.
 *
 * @return 1 - failed to parsed the configuration.
 *         0 - succesfull parsed the configuration value.
 *
 * @pre    (entry != NULL)
 * @pre    (value != NULL)
 */
int
cfgfile_parse_key_entry (const var_t *entry, const char *value)
{
    const key_table_entry_t *keytable;

    keytable = entry->key_table;

    if (keytable == NULL) {
        CSFLogError("common", "%s",
                    get_debug_string(DEBUG_PARSER_NULL_KEY_TABLE));
        return (1);
    }

    while (keytable->name) {
        if (cpr_strcasecmp(value, keytable->name) == 0) {
            /* keep the entire entry */
            *(key_table_entry_t *)entry->addr = *keytable;
            return (0);
        }
        keytable++;
    }

    CSFLogError("common", get_debug_string(DEBUG_PARSER_UNKNOWN_KEY), value);
    return (1);
}

/**
 * print (format) a key value. Print the name of the key out.
 *
 * @param[in] entry - pointer ot var_t.
 * @param[in] value - pointer to const. string of configuration value.
 *
 * @return always return 0.
 *
 * @pre    (entry != NULL)
 * @pre    (value != NULL)
 */
int
cfgfile_print_key_entry (const var_t *entry, char *buf, int len)
{
    key_table_entry_t *key;

    key = (key_table_entry_t *) entry->addr;
    if (key->name != NULL) {
        return (snprintf(buf, len, "%s", key->name));
    } else {
        /* the entry is not even configured */
        return (0);
    }
}
