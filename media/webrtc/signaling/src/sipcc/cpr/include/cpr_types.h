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

#ifndef _CPR_TYPES_H_
#define _CPR_TYPES_H_

#if defined SIP_OS_LINUX
#include "../linux/cpr_linux_types.h"
#elif defined SIP_OS_WINDOWS
#include "../win32/cpr_win_types.h"
#elif defined SIP_OS_OSX
#include "../darwin/cpr_darwin_types.h"
#endif

__BEGIN_DECLS

/*
 * CPR Return Codes
 */
typedef enum
{
    CPR_SUCCESS,
    CPR_FAILURE
} cpr_status_e;
typedef cpr_status_e cprRC_t;

/*
 * IPv4 address structure
 */
typedef uint32_t cpr_in_addr_t;

struct in_addr_s
{
#ifdef s_addr
    /* can occur with Windows winsock.h */
    union {
        struct {
            unsigned char s_b1, s_b2, sb_b3, s_b4;
        } S_un_b;
        cpr_in_addr_t S_addr;
    } S_un;
#else
    cpr_in_addr_t s_addr;
#endif
};

/*
 * IPv6 address structure
 */
typedef struct
{
    union
    {
        uint8_t  base8[16];
        uint16_t base16[8];
        uint32_t base32[4];
    } addr;
} cpr_in6_addr_t;

#ifndef s6_addr
#define s6_addr   addr.base8
#endif
#ifndef s6_addr16
#define s6_addr16 addr.base16
#endif
#ifndef s6_addr32
#define s6_addr32 addr.base32
#endif

typedef enum
{
     CPR_IP_ADDR_INVALID=0,
     CPR_IP_ADDR_IPV4,
     CPR_IP_ADDR_IPV6
} cpr_ip_type;

typedef enum
{
     CPR_IP_MODE_IPV4 = 0,
     CPR_IP_MODE_IPV6,
     CPR_IP_MODE_DUAL
}
cpr_ip_mode_e;
/*
 * IP address structure
 */
typedef struct
{
    cpr_ip_type type;
    union
    {
        cpr_in_addr_t  ip4;
        cpr_in6_addr_t ip6;
    } u;
} cpr_ip_addr_t;

extern const cpr_ip_addr_t ip_addr_invalid;

#define MAX_IPADDR_STR_LEN 48


#define CPR_IP_ADDR_INIT(a) a.type = CPR_IP_ADDR_INVALID;

typedef const char *string_t;

__END_DECLS

#endif
