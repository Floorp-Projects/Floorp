/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

void cpr_set_sockun_addr(cpr_sockaddr_un_t *addr, const char *name, pid_t pid);
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
