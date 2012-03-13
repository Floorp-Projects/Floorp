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

#include "cpr_types.h"
#include "cpr_stdio.h"
#include "cpr_stdlib.h"
#include "cpr_socket.h"
#include "cpr_in.h"
#include "util_string.h"
/*
 *  Conver IP address into dotted format or colon format.
 *
 *  @param addr_str : string to hold IP
 *         addr     : pointer to the IP address
 *
 *  @return  none
 *  
 *  @pre     none
 *
 */

void
ipaddr2dotted (char *addr_str, cpr_ip_addr_t *addr)
{
    if (addr_str)
    {
        switch (addr->type) {
        case CPR_IP_ADDR_IPV4 :
        sprintf(addr_str, "%u.%u.%u.%u",
                (addr->u.ip4 >> 24) & 0xFF,
                (addr->u.ip4 >> 16) & 0xFF,
                (addr->u.ip4 >> 8)  & 0xFF,
                (addr->u.ip4)       & 0xFF);
            break;
        case CPR_IP_ADDR_IPV6:
        sprintf(addr_str, "[%x:%x:%x:%x:%x:%x:%x:%x]",
                (addr->u.ip6.addr.base16[7]),
                (addr->u.ip6.addr.base16[6]),
                (addr->u.ip6.addr.base16[5]),
                (addr->u.ip6.addr.base16[4]),
                (addr->u.ip6.addr.base16[3]),
                (addr->u.ip6.addr.base16[2]),
                (addr->u.ip6.addr.base16[1]),
                (addr->u.ip6.addr.base16[0]));

            break;
        default:
            break;
        }


    }
}

/*
 *  Convert IP address to appropriate format dotted,or with colon
 *
 *  @param addr_str ip address string
 *
 *  @return  address
 *  
 *  @pre      none
 *
 */

uint32_t
dotted2ipaddr (const char *addr_str)
{
    uint32_t    address = 0;
    const char *p = NULL;
    char        section_str[3];
    int         sections[4];
    int         section;
    int         i;

    /* Check args */
    if ((!addr_str) || (addr_str[0] == '\0'))
    {
        return (uint32_t) -1;
    }

    /* Init */
    for (i = 0; i < 4; i++)
    {
        sections[i] = 0;
    }
    p = addr_str;

    /* Remove leading blanks */
    while ((*p == ' ') || (*p == '\t') || (*p == '\n') || (*p == '\r'))
    {
        p++;
    }

    for (section = 0; section < 4; section++)
    {
        i = 0;
        section_str[0] = '\0';
        section_str[1] = '\0';
        section_str[2] = '\0';
        while ((*p != '.') && (i < 3)) {
            section_str[i] = *p;
            i++;
            p++;
        }
        sections[section] = atoi(section_str);
        address = address | (sections[section]<<((3-section)*8));
        p++;
    }

    return address;
}

/*
 * ntohl function wrapper for both IPv4 and Ipv6
 *
 *  @param ip_addr_in, ip_addr_out : IP addresses
 *
 *  @return  none
 *  
 *  @pre none
 *
 */

void util_ntohl (cpr_ip_addr_t *ip_addr_out, cpr_ip_addr_t *ip_addr_in)
{
    int i,j;
    unsigned char tmp;

    ip_addr_out->type = ip_addr_in->type;

    if (ip_addr_in->type == CPR_IP_ADDR_IPV4) {

        ip_addr_out->u.ip4 = ntohl(ip_addr_in->u.ip4);

    } else {
        //to do IPv6: Add ntohl functionality for ipv6.

        if (ip_addr_out == ip_addr_in) {
            for (i=0, j=15; i<8; i++, j--) {
                tmp  = ip_addr_out->u.ip6.addr.base8[j];
                ip_addr_out->u.ip6.addr.base8[j] = ip_addr_in->u.ip6.addr.base8[i];
                ip_addr_in->u.ip6.addr.base8[i] = tmp;
            }
        } else {
            for (i=0, j=15; i<16; i++, j--) {
                ip_addr_out->u.ip6.addr.base8[j] = ip_addr_in->u.ip6.addr.base8[i];
            }
        }
    }
}

/*
 *  Check if the IP address passed is valid. For IPv4& IPv6 check if value 0 if so
 *   return FALSE. If the type is CPR_IP_ADDR_INVALID or any other non defined type
 *   return FALSE.
 *   
 *
 *  @param ip_addr  IP address
 *
 *  @return  boolean False if not a valid IP, or else true
 *  
 *  @pre     none
 *
 */

boolean util_check_if_ip_valid (cpr_ip_addr_t *ip_addr)
{
    if (ip_addr->type == CPR_IP_ADDR_INVALID) {
        return(FALSE);
    }
    if (ip_addr->type == CPR_IP_ADDR_IPV4 && 
        ip_addr->u.ip4 == 0) {

        return(FALSE);
    }

    if ((ip_addr->type == CPR_IP_ADDR_IPV6) && 
        (ip_addr->u.ip6.addr.base16[7] == 0) &&
        (ip_addr->u.ip6.addr.base16[6] == 0) &&
        (ip_addr->u.ip6.addr.base16[5] == 0) &&
        (ip_addr->u.ip6.addr.base16[4] == 0) &&
        (ip_addr->u.ip6.addr.base16[3] == 0) &&
        (ip_addr->u.ip6.addr.base16[2] == 0) &&
        (ip_addr->u.ip6.addr.base16[1] == 0) &&
        (ip_addr->u.ip6.addr.base16[0] == 0)) {

        return(FALSE);
    }

    if ((ip_addr->type != CPR_IP_ADDR_INVALID) && 
        (ip_addr->type != CPR_IP_ADDR_IPV4) &&
        (ip_addr->type != CPR_IP_ADDR_IPV6)) {

        return(FALSE);
    }

    return(TRUE);
}

/*
 *  Compare 2 IP addresses
 *
 *  @param ip_addr1 & ip_addr2 2 ip addresses to compare
 *
 *  @return  true if that matches or else false
 *  
 *  @pre     (ip_addr1 != NULL) && (ip_addr2 != NULL)
 *
 */

boolean util_compare_ip (cpr_ip_addr_t *ip_addr1, cpr_ip_addr_t *ip_addr2)
{
    if (ip_addr1->type != ip_addr2->type) {

        return(FALSE);
    }

    if (ip_addr1->type == CPR_IP_ADDR_IPV4 &&
        ip_addr2->type == CPR_IP_ADDR_IPV4) {

        return((boolean) (ip_addr1->u.ip4 == ip_addr2->u.ip4));

    } else if (ip_addr1->type == CPR_IP_ADDR_IPV6 &&
            ip_addr2->type == CPR_IP_ADDR_IPV6) {

        return((boolean)memcmp((void *)&(ip_addr1->u.ip6.addr.base8), 
                    (void *)&(ip_addr2->u.ip6.addr.base8), 16));
    }

    return(FALSE);
}

/*
 *  Extract ip address information from socket storage structure
 *
 *  @param ip_addr  ip address to be filled.
 *          cpr_sockaddr_storage storage structure pointer
 * 
 *  @return  none
 *  
 *  @pre     (ip_addr != NULL)
 *
 */
void util_extract_ip (cpr_ip_addr_t *ip_addr, 
                        cpr_sockaddr_storage *from)
{
    switch (from->ss_family) {
    case AF_INET6:
        ip_addr->type = CPR_IP_ADDR_IPV6;
        ip_addr->u.ip6 = ((cpr_sockaddr_in6_t *)from)->sin6_addr;   
        break;
    case AF_INET:
        ip_addr->type = CPR_IP_ADDR_IPV4;
        ip_addr->u.ip4 = ((cpr_sockaddr_in_t *)from)->sin_addr.s_addr;
        break;
    default:
        break;
    }
}

/*
 *  Get port number from storage structure.
 *
 *  @param sock_storage
 *
 *  @return  port number
 *  
 *  @pre     (sock_storage != NULL)
 *
 */

uint16_t util_get_port (cpr_sockaddr_storage *sock_storage)
{
    switch (sock_storage->ss_family) {
    case AF_INET6:
        return(((cpr_sockaddr_in6_t *)sock_storage)->sin6_port);
    case AF_INET:
        return(((cpr_sockaddr_in_t *)sock_storage)->sin_port);
    default:
        break;
    }
    return(0);
}

void util_get_ip_using_mode (cpr_ip_addr_t *ip_addr, cpr_ip_mode_e ip_mode,
                             uint32_t  ip4, char *ip6)
{
    *ip_addr = ip_addr_invalid;

    switch (ip_mode) {
    case CPR_IP_MODE_IPV4:
        ip_addr->type = CPR_IP_ADDR_IPV4;
        ip_addr->u.ip4 = ip4;
        break;
    case CPR_IP_MODE_IPV6:
        ip_addr->type = CPR_IP_ADDR_IPV6;
        if (ip6 != NULL) {
            memcpy((void *)&(ip_addr->u.ip6.addr.base8[16]), (void *)ip6, 16);
        }
        break;
    case CPR_IP_MODE_DUAL:
    default:
        break;
    }

}

unsigned long
gmt_string_to_seconds (char *gmt_string, unsigned long *seconds)
{
    char *token;

    // Do a preliminary check and see if the string
    // passed in is a completely numeric string
    // If it is, then we will return just that number
    // and return a 1 for the error code, meaning
    // we parsed a
    *seconds = strtoul(gmt_string, &token, 10);
    if ((token == NULL) || (token[0] == '\0')) {
        return 1;
    }
    *seconds = 0;
    return 0;
}

long
diff_current_time (unsigned long t1, unsigned long *difference)
{
    *difference = 1;
    return 0;
}

boolean 
is_empty_str (char *str)
{
    if (str == NULL) {
        return TRUE;
    }
    if (strncmp(str, EMPTY_STR, EMPTY_STR_LEN) == 0) {
        return TRUE;
    }
    return FALSE;
}

void 
init_empty_str (char *str)
{
    strcpy(str,EMPTY_STR);
}
