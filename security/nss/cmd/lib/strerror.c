/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#include "secutil.h"

#if defined(XP_UNIX)
#include <unistd.h>
#endif

#if defined(XP_WIN)
#include <errno.h>
#include <winsock.h>
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif

#include <stdio.h>
#include <string.h>

static char *(mcom_include_merrors_i_strings)(int16 i) ;
static char *(mcom_include_secerr_i_strings)(int16 i) ;
static char *(mcom_include_sslerr_i_strings)(int16 i) ;
static char *(mcom_include_xp_error_i_strings)(int16 i) ;
static char *(mcom_include_xp_msg_i_strings)(int16 i) ;


#ifdef XP_WIN

#define EWOULDBLOCK             WSAEWOULDBLOCK
#define EINPROGRESS             WSAEINPROGRESS
#define EALREADY                WSAEALREADY
#define ENOTSOCK                WSAENOTSOCK
#define EDESTADDRREQ            WSAEDESTADDRREQ
#define EMSGSIZE                WSAEMSGSIZE
#define EPROTOTYPE              WSAEPROTOTYPE
#define ENOPROTOOPT             WSAENOPROTOOPT
#define EPROTONOSUPPORT         WSAEPROTONOSUPPORT
#define ESOCKTNOSUPPORT         WSAESOCKTNOSUPPORT
#define EOPNOTSUPP              WSAEOPNOTSUPP
#define EPFNOSUPPORT            WSAEPFNOSUPPORT
#define EAFNOSUPPORT            WSAEAFNOSUPPORT
#define EADDRINUSE              WSAEADDRINUSE
#define EADDRNOTAVAIL           WSAEADDRNOTAVAIL
#define ENETDOWN                WSAENETDOWN
#define ENETUNREACH             WSAENETUNREACH
#define ENETRESET               WSAENETRESET
#define ECONNABORTED            WSAECONNABORTED
#define ECONNRESET              WSAECONNRESET
#define ENOBUFS                 WSAENOBUFS
#define EISCONN                 WSAEISCONN
#define ENOTCONN                WSAENOTCONN
#define ESHUTDOWN               WSAESHUTDOWN
#define ETOOMANYREFS            WSAETOOMANYREFS
#define ETIMEDOUT               WSAETIMEDOUT
#define ECONNREFUSED            WSAECONNREFUSED
#define ELOOP                   WSAELOOP
#define EHOSTDOWN               WSAEHOSTDOWN
#define EHOSTUNREACH            WSAEHOSTUNREACH
#define EPROCLIM                WSAEPROCLIM
#define EUSERS                  WSAEUSERS
#define EDQUOT                  WSAEDQUOT
#define ESTALE                  WSAESTALE
#define EREMOTE                 WSAEREMOTE

#endif

#undef XP_WIN
#undef XP_UNIX
#undef RESOURCE_STR_X
#undef RESOURCE_STR

#define XP_UNIX 1
#define RESOURCE_STR 1

#include "allxpstr.h"

const char *
SECU_Strerror(int error)
{
    char * errString;
    int16  err          = error + RES_OFFSET;

    errString = mcom_include_merrors_i_strings(err) ;
    if (!errString)
	errString =  mcom_include_secerr_i_strings(err) ;
    if (!errString)
	errString =  mcom_include_sslerr_i_strings(err) ;
    if (!errString)
	errString =  mcom_include_xp_error_i_strings(err) ;
    if (!errString)
	errString =  mcom_include_xp_msg_i_strings(err) ;
    if (!errString)
	errString =  "ERROR NOT FOUND! \n";

/*    printf( "%s\n", errString); */

    return (const char *)errString;
}

