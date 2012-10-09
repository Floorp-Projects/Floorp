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

#include "cpr_errno.h"
#include <errno.h>

/**
 * @addtogroup OSAPIs The CPR OS Abstractions
 * @brief Misc OS API Abstractions in CPR
 *
 * @{
 */
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
    CPR_EDEADLK, /*10*/
    CPR_ENOMEM,
    CPR_EACCES,
    CPR_EFAULT,
    CPR_ENOTBLK,
    CPR_EBUSY,
    CPR_EEXIST,
    CPR_EXDEV,
    CPR_ENODEV,
    CPR_ENOTDIR,
    CPR_EISDIR,/*20*/
    CPR_EINVAL,
    CPR_ENFILE,
    CPR_EMFILE,
    CPR_ENOTTY,
    CPR_ETXTBSY,
    CPR_EFBIG,
    CPR_ENOSPC,
    CPR_ESPIPE,
    CPR_EROFS,
    CPR_EMLINK,/*30*/
    CPR_EPIPE,
    CPR_EDOM,
    CPR_ERANGE,
    CPR_EAGAIN,
    CPR_EINPROGRESS,
    CPR_EALREADY,
    CPR_ENOTSOCK,
    CPR_EDESTADDRREQ,
    CPR_EMSGSIZE,
    CPR_EPROTOTYPE,/*40*/
    CPR_ENOPROTOOPT,
    CPR_EPROTONOSUPPORT,
    CPR_ESOCKTNOSUPPORT,
    CPR_ENOTSUP,
    CPR_EPFNOSUPPORT,
    CPR_EAFNOSUPPORT,
    CPR_EADDRINUSE,
    CPR_EADDRNOTAVAIL,
    CPR_ENETDOWN,
    CPR_ENETUNREACH,/*50*/
    CPR_ENETRESET,
    CPR_ECONNABORTED,
    CPR_ECONNRESET,
    CPR_ENOBUFS,
    CPR_EISCONN,
    CPR_ENOTCONN,
    CPR_ESHUTDOWN,
    CPR_ETOOMANYREFS,
    CPR_ETIMEDOUT,
    CPR_ECONNREFUSED,/*60*/
    CPR_ELOOP,
    CPR_ENAMETOOLONG,
    CPR_EHOSTDOWN,
    CPR_EHOSTUNREACH,
    CPR_ENOTEMPTY,
    CPR_UNKNOWN_ERR,            /* empty 66 */
    CPR_EUSERS,
    CPR_EDQUOT,
    CPR_ESTALE,
    CPR_EREMOTE,/*70*/
    CPR_UNKNOWN_ERR,            /* empty 71 */
    CPR_UNKNOWN_ERR,            /* empty 72 */
    CPR_UNKNOWN_ERR,            /* empty 73 */
    CPR_UNKNOWN_ERR,            /* empty 74 */
    CPR_UNKNOWN_ERR,            /* empty 75 */
    CPR_ENOLCK,
    CPR_ENOSYS,
    CPR_UNKNOWN_ERR,            /* empty 78 */
    CPR_UNKNOWN_ERR,            /* empty 79 */
    CPR_UNKNOWN_ERR,/*80*/
    CPR_UNKNOWN_ERR,            /* empty 81 */
    CPR_UNKNOWN_ERR,            /* empty 82 */
    CPR_EOVERFLOW,
    CPR_UNKNOWN_ERR,            /* empty 84 */
    CPR_UNKNOWN_ERR,            /* empty 85 */
    CPR_UNKNOWN_ERR,            /* empty 86 */
    CPR_UNKNOWN_ERR,            /* empty 87 */
    CPR_ECANCELED,
    CPR_EIDRM,
    CPR_ENOMSG,/*90*/
    CPR_EILSEQ,
    CPR_UNKNOWN_ERR,            /* empty 92 */
    CPR_EBADMSG,
    CPR_EMULTIHOP,
    CPR_ENODATA,
    CPR_ENOLINK,
    CPR_ENOSR,
    CPR_ENOSTR,
    CPR_EPROTO,
    CPR_ETIME,/*100*/
    CPR_EOPNOTSUPP,
    CPR_UNKNOWN_ERR
}; // Soya: the errno_table should be consistent with <errno.h>

/**
 *
 * @brief Translates to "cpr_errno" Macro
 *
 * pSIPCC uses the cpr_errno macro to print the errno 
 * for error conditions. This function is used to map the standard 
 * errno to standard CPR errors
 *
 * @return The CPR error number
 *
 */
int16_t
cprTranslateErrno (void)
{
    int16_t e = (int16_t) errno;

    /*
     * Verify against MIN and MAX errno numbers
     */
    if ( e < 1 || e > (int16_t) (sizeof(errno_table)/sizeof(errno_table[0])) )
	{
        return CPR_UNKNOWN_ERR;
    }
    return (int16_t) errno_table[e - 1];
}

