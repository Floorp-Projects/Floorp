/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

/*
 * !!! NOTE !!!
 *
 * The strings of type string_t are actually very special blocks
 * of memory that have a "hidden" header block immediately preceding
 * the pointer. You MUST use the functions in string_lib.c to
 * create, manipulate, destroy, copy, or otherwise work with these
 * strings.
 */

typedef const char *string_t;

__END_DECLS

#endif
