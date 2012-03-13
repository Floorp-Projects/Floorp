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

#ifndef _CPR_WIN_STDLIB_H_
#define _CPR_WIN_STDLIB_H_

//#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include "string.h"
#include "errno.h"

/*
 * Windows must use direct OS calls in order to debug
 * memory leaks. However since the phone expects malloced
 * memory to be zeroed out cpr_malloc is mapped directly
 * to calloc in cpr_stdlib.h
 */
#define CPR_USE_DIRECT_OS_CALL
#define CPR_USE_CALLOC_FOR_MALLOC

/*
 * Definitions for errno
 */
#define ENOTBLK        15 /* Block device required                */
#define ETXTBSY        26 /* Text file busy                       */
#define ENOMSG         35 /* No message of desired type           */
#define ECHRNG         37 /* Channel number out of range          */
#define EIDRM          43 /* Identifier removed                   */
#define EL2NSYNC       44 /* Level 2 not synchronized             */
#define EL3HLT         45 /* Level 3 halted                       */
#define EL3RST         46 /* Level 3 reset                        */
#define ELNRNG         47 /* Link number out of range             */
#define EUNATCH        48 /* Protocol driver not attached         */
#define ENOCSI         49 /* No CSI structure available           */
#define EL2HLT         50 /* Level 2 halted                       */
#define ECANCELED      51 /* Operation canceled                   */
#define ENOTSUP        52 /* Operation not supported              */
#define EBADE          53 /* Invalid exchange                     */
#define EBADR          54 /* Invalid request descriptor           */
#define EXFULL         55 /* Exchange full                        */
#define ENOANO         56 /* No anode                             */
#define EBADRQC        57 /* Invalid request code                 */
#define EBADSLT        58 /* Invalid slot                         */
#define EBFONT         59 /* Bad font file fmt                    */
#define EBADMSG        60 /* Trying to read unreadable message    */
#define EOVERFLOW      61 /* Value too large for defined data type*/
#define ENOSTR         62 /* Device not a stream                  */
#define ENODATA        63 /* No data (for no delay io)            */
#define ETIME          64 /* Timer expired                        */
#define ENOSR          65 /* Out of streams resources             */
#define ENONET         66 /* Machine is not on the network        */
#define ENOPKG         67 /* Package not installed                */
#define EREMOTE        68 /* The object is remote                 */
#define ENOLINK        69 /* The link has been severed            */
#define EADV           70 /* Advertise error                      */
#define ESRMNT         71 /* Srmount error                        */
#define ECOMM          72 /* Communication error on send          */
#define EPROTO         73 /* Protocol error                       */
#define EREMCHG        74 /* Remote address changed               */
#define EBADFD         75 /* fd invalid for this operation        */
#define ENOTUNIQ       76 /* given log. name not unique           */
#define EMULTIHOP      77 /* multihop attempted                   */
#define ELIBACC        78 /* Can't access a needed shared lib.    */
#define ELIBBAD        79 /* Accessing a corrupted shared lib.    */
#define ELIBSCN        80 /* .lib section in a.out corrupted.     */
#define ELIBMAX        81 /* Attempting to link in too many libs. */
#define ELIBEXEC       82 /* Attempting to exec a shared library. */
#define ELOOP          83 /* Symbolic link loop                   */
#define ERESTART       84 /* Restartable system call              */
#define ESTRPIPE       85 /* if FIFO, don't sleep in stream head  */
#define EUSERS         86 /* Too many users (for UFS)             */
#define ENOTSOCK       87 /* Socket operation on non-socket       */
#define EDESTADDRREQ   88 /* Destination address required         */
#define EMSGSIZE       89 /* Message too long                     */
#define EPROTOTYPE     90 /* Protocol wrong type for socket       */
#define ENOPROTOOPT    91 /* Protocol not available               */
#define EPROTONOSUPPORT 100     /* Protocol not supported */
#define ESOCKTNOSUPPORT 101     /* Socket type not supported */
#define EOPNOTSUPP      102     /* Operation not supported on socket */
#define EPFNOSUPPORT    103     /* Protocol family not supported */
#define EAFNOSUPPORT    104     /* Address family not supported by */
                                /* protocol family */
#define EADDRINUSE      105     /* Address already in use */
#define EADDRNOTAVAIL   106     /* Can't assign requested address */
#define ENETDOWN        107     /* Network is down */
#define ENETUNREACH     108     /* Network is unreachable */
#define ENETRESET       109     /* Network dropped connection because */
                                /* of reset */
#define ECONNABORTED    110     /* Software caused connection abort */
#define ECONNRESET      111     /* Connection reset by peer */
#define ENOBUFS         112     /* No buffer space available */
#define EISCONN         113     /* Socket is already connected */
#define ENOTCONN        114     /* Socket is not connected */
#define ESHUTDOWN       115     /* Can't send after socket shutdown */
#define ETOOMANYREFS    116     /* Too many references: can't splice */
#define ETIMEDOUT       117     /* Connection timed out */
#define ECONNREFUSED    118     /* Connection refused */
#define EHOSTDOWN       119     /* Host is down */
#define EHOSTUNREACH    120     /* No route to host */
#define EWOULDBLOCK     EAGAIN
#define EALREADY        121     /* operation already in progress */
#define EINPROGRESS     122     /* operation now in progress */
#define EDQUOT          123     /* Disc quota exceeded   */
#define ESTALE          124     /* Stale NFS file handle */

#endif

