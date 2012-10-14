/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_ERRNO_H_
#define _CPR_ERRNO_H_

#include "cpr_types.h"

__BEGIN_DECLS

#if defined SIP_OS_LINUX
#include "../linux/cpr_linux_errno.h"
#elif defined SIP_OS_WINDOWS
#include "../win32/cpr_win_errno.h"
#elif defined SIP_OS_OSX
#include "../darwin/cpr_darwin_errno.h"
#endif

/** The enumerations for the various error types. pSIPCC uses these error values
 * for checking of the various error conditions and these error type abstractions
 * MUST be defined here.
 */
typedef enum
{
    /* First 34 entries are well-known, keep it that way */
    CPR_EPERM = 1,      /* Not super-user                    1 */
    CPR_ENOENT,         /* No such file or directory           */
    CPR_ESRCH,          /* No such process                     */
    CPR_EINTR,          /* interrupted system call             */
     //WSAEINTR
    CPR_EIO,            /* I/O error                           */
     //WSA_IO_INCOMPLETE
    CPR_ENXIO,          /* No such device or address           */
    CPR_E2BIG,          /* Arg list too long                   */
    CPR_ENOEXEC,        /* Exec format error                   */
    CPR_EBADF,          /* Bad file number                     */
    CPR_ECHILD,         /* No children                         */
    CPR_EAGAIN,         /* Resource temporarily unavailable    */
     //WSATRY_AGAIN
    CPR_EWOULDBLOCK = CPR_EAGAIN,
     //WSAEWOULDBLOCK
    CPR_ENOMEM,         /* Not enough core                     */
     //WSA_NOT_ENOUGH_MEMORY
    CPR_EACCES,         /* Permission denied                   */
     //WSAEACCES
    CPR_EFAULT,         /* Bad address                         */
     //WSAEFAULT
    CPR_ENOTBLK,        /* Block device required               */
    CPR_EBUSY,          /* Mount device busy                   */
    CPR_EEXIST,         /* File exists                         */
    CPR_EXDEV,          /* Cross-device link                   */
    CPR_ENODEV,         /* No such device                      */
    CPR_ENOTDIR,        /* Not a directory                     */
    CPR_EISDIR,         /* Is a directory                      */
    CPR_EINVAL,         /* Invalid argument                    */
     //WSAEINVAL
     //WSA_INVALID_PARAMETER
    CPR_ENFILE,         /* File table overflow                 */
    CPR_EMFILE,         /* Too many open files                 */
     //WSAEMFILE
    CPR_ENOTTY,         /* Inappropriate ioctl for device      */
    CPR_ETXTBSY,        /* Text file busy                      */
    CPR_EFBIG,          /* File too large                      */
    CPR_ENOSPC,         /* No space left on device             */
    CPR_ESPIPE,         /* Illegal seek                        */
    CPR_EROFS,          /* Read only file system               */
    CPR_EMLINK,         /* Too many links                      */
    CPR_EPIPE,          /* Broken pipe                         */
    CPR_EDOM,           /* Math arg out of domain of func      */
    CPR_ERANGE,         /* Math result not representable       */

    /*     Socket error conditions */
    CPR_ENOTSOCK,       /* Socket operation on non-socket   35 */
    CPR_EDESTADDRREQ,   /* Destination address required        */
    CPR_EMSGSIZE,       /* Message too long                    */
    CPR_EPROTOTYPE,     /* Protocol wrong type for socket      */
    CPR_ENOPROTOOPT,    /* Protocol not available              */
    CPR_EPROTONOSUPPORT,/* Protocol not supported              */
    CPR_ESOCKTNOSUPPORT,/* Socket type not supported           */
    CPR_EOPNOTSUPP,     /* Operation not supported on socket   */
    CPR_EPFNOSUPPORT,   /* Protocol family not supported       */
    CPR_EAFNOSUPPORT,   /* Address family not supp. by protocol*/
    CPR_EADDRINUSE,     /* Address already in use              */
    CPR_EADDRNOTAVAIL,  /* Can not assign requested address    */
    CPR_ENETDOWN,       /* Network is down                     */
    CPR_ENETUNREACH,    /* Network is unreachable              */
    CPR_ENETRESET,      /* Netwk dropped conn. because of reset*/
    CPR_ECONNABORTED,   /* Software caused connection abort    */
    CPR_ECONNRESET,     /* Connection reset by peer            */
    CPR_ENOBUFS,        /* No buffer space available           */
    CPR_EISCONN,        /* Socket is already connected         */
    CPR_ENOTCONN,       /* Socket is not connected             */
    CPR_ESHUTDOWN,      /* Can not send after socket shutdown  */
    CPR_ETOOMANYREFS,   /* Too many references: can not splice */
    CPR_ETIMEDOUT,      /* Connection timed out                */
    CPR_ECONNREFUSED,   /* Connection refused                  */
    CPR_EHOSTDOWN,      /* Host is down                        */
    CPR_EHOSTUNREACH,   /* No route to host                    */
    CPR_EALREADY,       /* Operation already in progress       */
    CPR_EINPROGRESS,    /* Operation now in progress           */

    /* The following error conditions are common among */
    /* CNU, Linux and Solaris environments             */

    CPR_ENOMSG,         /* No message of desired type       63 */
    CPR_EIDRM,          /* Identifier removed                  */
    CPR_ECHRNG,         /* Channel number out of range         */
    CPR_EL2NSYNC,       /* Level 2 not synchronized            */
    CPR_EL3HLT,         /* Level 3 halted                      */
    CPR_EL3RST,         /* Level 3 reset                       */
    CPR_ELNRNG,         /* Link number out of range            */
    CPR_EUNATCH,        /* Protocol driver not attached        */
    CPR_ENOCSI,         /* No CSI structure available          */
    CPR_EL2HLT,         /* Level 2 halted                      */
    CPR_ENOLCK,         /* No record locks available.          */
    CPR_EDEADLK,        /* Deadlock condition                  */
    CPR_ECANCELED,      /* Operation canceled                  */
    CPR_ENOTSUP,        /* Operation not supported             */
    CPR_EDQUOT,         /* Disc quota exceeded                 */

    /*    Convergent error conditions */
    CPR_EBADE,          /* invalid exchange                 78 */
    CPR_EBADR,          /* invalid request descriptor          */
    CPR_EXFULL,         /* exchange full                       */
    CPR_ENOANO,         /* no anode                            */
    CPR_EBADRQC,        /* invalid request code                */
    CPR_EBADSLT,        /* invalid slot                        */
    CPR_EDEADLOCK,      /* file locking deadlock error         */
    CPR_EBFONT,         /* bad font file fmt                   */

    /*    Stream error conditions */
    CPR_ENOSTR,         /* Device not a stream              86 */
    CPR_ENODATA,        /* no data (for no delay io)           */
    CPR_ETIME,          /* timer expired                       */
    CPR_ENOSR,          /* out of streams resources            */
    CPR_ENONET,         /* Machine is not on the network       */
    CPR_ENOPKG,         /* Package not installed               */
    CPR_EREMOTE,        /* The object is remote                */
    CPR_ENOLINK,        /* the link has been severed           */
    CPR_EADV,           /* advertise error                     */
    CPR_ESRMNT,         /* srmount error                       */
    CPR_ECOMM,          /* Communication error on send         */
    CPR_EPROTO,         /* Protocol error                      */
    CPR_EMULTIHOP,      /* multihop attempted                  */
    CPR_EBADMSG,        /* trying to read unreadable message   */
    CPR_ENAMETOOLONG,   /* path name is too long               */
    CPR_EOVERFLOW,      /* value too large to be stored        */
    CPR_ENOTUNIQ,       /* given log. name not unique          */
    CPR_EBADFD,         /* f.d. invalid for this operation     */
    CPR_EREMCHG,        /* Remote address changed              */

    /*    Shared library error conditions */
    CPR_ELIBACC,        /* Can't access a needed shared library*/
    CPR_ELIBBAD,        /* Accessing a corrupted shared library*/
    CPR_ELIBSCN,        /* .lib section in a.out corrupted.    */
    CPR_ELIBMAX,        /* Attempting to link in too many libs */
    CPR_ELIBEXEC,       /* Attempting to exec a shared library */
    CPR_EILSEQ,         /* Illegal byte sequence               */
    CPR_ENOSYS,         /* Unsupported file system operation   */
    CPR_ELOOP,          /* Symbolic link loop                  */
    CPR_ERESTART,       /* Restartable system call             */
    CPR_ESTRPIPE,       /* If pipe, don't sleep in stream head */
    CPR_ENOTEMPTY,      /* directory not empty                 */
    CPR_EUSERS,         /* Too many users (for UFS)            */

    CPR_ESTALE,         /* Stale NFS file handle               */

    /* The following error conditions not common among OSs */
    CPR_ECLOSED,        /* CNU specific, connection closed by host
                           (may need to re-map to something else,
                            still TBD) */
    CPR_EINIT,

   CPR_UNKNOWN_ERR, /* 120 */
   CPR_ERRNO_MAX
} cpr_errno_e;

__END_DECLS

#endif
