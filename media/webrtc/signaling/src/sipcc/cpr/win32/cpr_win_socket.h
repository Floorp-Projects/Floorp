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

#ifndef _CPR_WIN_SOCKET_H_
#define _CPR_WIN_SOCKET_H_

#include "cpr_types.h"

/*
 * winsock2.h is already included from cpr_win_types.h.
 */

/*
 * RAMC BEGIN- Copied from ios socket/socket.h
 */
#define SOL_TCP                 0x02
#define SOL_IP                  0x03
#define SOL_UDP                 0x04

/*
 * sockaddr_storage:
 * Common superset of at least AF_INET, AF_INET6 and AF_LINK sockaddr
 * structures. Has sufficient size and alignment for those sockaddrs.
 */
typedef uint16_t        sa_family_t;



/*
 * nokelly:
 * Created this type to get things to compile, but I know that this is not 
 * necessarily going to work.
 */
#ifndef AF_LOCAL
#define AF_LOCAL 1
#endif
typedef struct
{
    sa_family_t	    sun_family;
    int8_t	    sun_path[108];
} cpr_sockaddr_un_t;

#define cpr_sun_len(a) sizeof(a)

void cpr_set_sockun_addr(cpr_sockaddr_un_t *addr, const char *name);
/* End of nokelly's adds */


//extern const cpr_in6_addr_t in6addr_any;

/*
 * Define SOCKET_ERROR
 */
/*
 * Define INVALID_SOCKET
 */
#define INVALID_SOCKET  (SOCKET)(~0)
#define SOCKET_ERROR            (-1)

typedef unsigned int    u_int;
typedef u_int  SOCKET;
#define Socket SOCKET
typedef int cpr_socklen_t;
typedef SOCKET cpr_socket_t;
typedef unsigned long u_long;
typedef unsigned short u_short; 

#define SUPPORT_CONNECT_CONST const

#ifdef CPR_USE_SOCKETPAIR
#undef CPR_USE_SOCKETPAIR
#endif

#define MAX_RETRY_FOR_EAGAIN 10
#endif 
