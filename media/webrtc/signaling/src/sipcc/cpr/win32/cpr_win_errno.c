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

#include <errno.h>
#include <winsock2.h>
#include "cpr_errno.h"

int8_t errno_table[] =
{
    CPR_EPERM,
    CPR_ENOENT,
    CPR_ESRCH,
    CPR_EINTR,
    CPR_EIO,
    CPR_ENXIO,
    CPR_E2BIG,
    CPR_ENOEXEC,
    CPR_EBADF,
    CPR_ECHILD,
    CPR_EAGAIN,
    CPR_ENOMEM,
    CPR_EACCES,
    CPR_EFAULT,
    CPR_ENOTBLK,
    CPR_EBUSY,
    CPR_EEXIST,
    CPR_EXDEV,
    CPR_ENODEV,
    CPR_ENOTDIR,
    CPR_EISDIR,
    CPR_EINVAL,
    CPR_ENFILE,
    CPR_EMFILE,
    CPR_ENOTTY,
    CPR_ETXTBSY,
    CPR_EFBIG,
    CPR_ENOSPC,
    CPR_ESPIPE,
    CPR_EROFS,
    CPR_EMLINK,
    CPR_EPIPE,
    CPR_EDOM,
    CPR_ERANGE,
    CPR_ENOMSG,
    CPR_EIDRM,
    CPR_ECHRNG,
    CPR_EL2NSYNC,
    CPR_EL3HLT,
    CPR_EL3RST,
    CPR_ELNRNG,
    CPR_EUNATCH,
    CPR_ENOCSI,
    CPR_EL2HLT,
    CPR_EDEADLK,
    CPR_ENOLCK,
    CPR_ECANCELED,
    CPR_ENOTSUP,
    CPR_EDQUOT,
    CPR_EBADE,
    CPR_EBADR,
    CPR_EXFULL,
    CPR_ENOANO,
    CPR_EBADRQC,
    CPR_EBADSLT,
    CPR_EDEADLOCK,
    CPR_EBFONT,
    CPR_UNKNOWN_ERR, /* empty 58 */
    CPR_UNKNOWN_ERR, /* empty 59 */
    CPR_ENOSTR,
    CPR_ENODATA,
    CPR_ETIME,
    CPR_ENOSR,
    CPR_ENONET,
    CPR_ENOPKG,
    CPR_EREMOTE,
    CPR_ENOLINK,
    CPR_EADV,
    CPR_ESRMNT,
    CPR_ECOMM,
    CPR_EPROTO,
    CPR_UNKNOWN_ERR, /* empty 72 */
    CPR_UNKNOWN_ERR, /* empty 73 */
    CPR_EMULTIHOP,
    CPR_UNKNOWN_ERR, /* empty 75 */
    CPR_UNKNOWN_ERR, /* empty 76 */
    CPR_EBADMSG,
    CPR_ENAMETOOLONG,
    CPR_EOVERFLOW,
    CPR_ENOTUNIQ,
    CPR_EBADFD,
    CPR_EREMCHG,
    CPR_ELIBACC,
    CPR_ELIBBAD,
    CPR_ELIBSCN,
    CPR_ELIBMAX,
    CPR_ELIBEXEC,
    CPR_EILSEQ,
    CPR_ENOSYS,
    CPR_ELOOP,
    CPR_ERESTART,
    CPR_ESTRPIPE,
    CPR_ENOTEMPTY,
    CPR_EUSERS,
    CPR_ENOTSOCK,
    CPR_EDESTADDRREQ,
    CPR_EMSGSIZE,
    CPR_EPROTOTYPE,
    CPR_ENOPROTOOPT,
    /* errno index goes from 99 to 120 */
    CPR_EPROTONOSUPPORT,
    CPR_ESOCKTNOSUPPORT,
    CPR_EOPNOTSUPP,
    CPR_EPFNOSUPPORT,
    CPR_EAFNOSUPPORT,
    CPR_EADDRINUSE,
    CPR_EADDRNOTAVAIL,
    CPR_ENETDOWN,
    CPR_ENETUNREACH,
    CPR_ENETRESET,
    CPR_ECONNABORTED,
    CPR_ECONNRESET,
    CPR_ENOBUFS,
    CPR_EISCONN,
    CPR_ENOTCONN,
    CPR_UNKNOWN_ERR, /* empty 135 */
    CPR_UNKNOWN_ERR, /* empty 136 */
    CPR_UNKNOWN_ERR, /* empty 137 */
    CPR_UNKNOWN_ERR, /* empty 138 */
    CPR_UNKNOWN_ERR, /* empty 139 */
    CPR_UNKNOWN_ERR, /* empty 140 */
    CPR_UNKNOWN_ERR, /* empty 141 */
    CPR_UNKNOWN_ERR, /* empty 142 */
    CPR_ESHUTDOWN,
    CPR_ETOOMANYREFS,
    CPR_ETIMEDOUT,
    CPR_ECONNREFUSED,
    CPR_EHOSTDOWN,
    CPR_EHOSTUNREACH,
    CPR_EALREADY,
    CPR_EINPROGRESS,
    CPR_ESTALE
};

int16_t
cprTranslateErrno (void)
{
    int err = WSAGetLastError();
    switch (err)
    {
    case WSAEWOULDBLOCK:
    	err = CPR_EWOULDBLOCK;
    	break;
    case WSAEINPROGRESS:
    	err = CPR_EINPROGRESS;
    	break;
    case WSAEALREADY:
    	err = CPR_EALREADY;
    	break;
    case WSAEISCONN:
    	err = CPR_EISCONN;
    	break;
    case WSAENOTCONN:
    	err = CPR_ENOTCONN;
    	break;
    case WSAECONNREFUSED:
    	err = CPR_ECONNREFUSED;
    	break;
    }
    return err;
}

