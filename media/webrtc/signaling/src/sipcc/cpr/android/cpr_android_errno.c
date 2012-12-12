/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_errno.h"
#include <errno.h>

/**
 * @addtogroup OSAPIs The CPR OS Abstractions
 * @brief Misc OS API Abstractions in CPR
 *
 * @{
 */
static int8_t errno_table[] =
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
    CPR_EAGAIN, /*10*/
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
    CPR_ENOMSG,
    CPR_EIDRM,
    CPR_ECHRNG,
    CPR_EL2NSYNC,
    CPR_EL3HLT,
    CPR_EL3RST,
    CPR_ELNRNG,/*40*/
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
    CPR_UNKNOWN_ERR,            /* empty 58 */
    CPR_UNKNOWN_ERR,            /* empty 59 */
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
    CPR_UNKNOWN_ERR,            /* empty 72 */
    CPR_UNKNOWN_ERR,            /* empty 73 */
    CPR_EMULTIHOP,
    CPR_UNKNOWN_ERR,            /* empty 75 */
    CPR_UNKNOWN_ERR,            /* empty 76 */
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
    CPR_ECLOSED,
    CPR_UNKNOWN_ERR,            /* empty 136 */
    CPR_UNKNOWN_ERR,            /* empty 137 */
    CPR_UNKNOWN_ERR,            /* empty 138 */
    CPR_UNKNOWN_ERR,            /* empty 139 */
    CPR_UNKNOWN_ERR,            /* empty 140 */
    CPR_UNKNOWN_ERR,            /* empty 141 */
    CPR_UNKNOWN_ERR,            /* empty 142 */
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
    if ((e < 1) || (e > 151)) {
        return CPR_UNKNOWN_ERR;
    } else if (e >= 120) {
        e = e - 20;
    } else if (e >= 100) {
        /*
         * In the gap from 100 to 119
         */
        return CPR_UNKNOWN_ERR;
    }
    return (int16_t) errno_table[e - 1];
}

/**
  * @}
  */
