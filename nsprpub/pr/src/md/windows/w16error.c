/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
** Note: A single error mapping function is provided.
**
*/ 
#include "prerror.h"
#include <errno.h>
#include <winsock.h>


void _PR_MD_map_error( int err )
{

    switch ( err )
    {
        case  ENOENT:   /* No such file or directory */
			PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
            break;
        case  E2BIG:    /* Argument list too big */
            PR_SetError( PR_INVALID_ARGUMENT_ERROR, err );
            break;
        case  ENOEXEC:  /* Exec format error */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  EBADF:    /* Bad file number */
            PR_SetError( PR_BAD_DESCRIPTOR_ERROR, err );
            break;
        case  ENOMEM:   /* Not enough Memory */
            PR_SetError( PR_OUT_OF_MEMORY_ERROR, err );
            break;
        case  EACCES:   /* Permission denied */
            PR_SetError( PR_NO_ACCESS_RIGHTS_ERROR, err );
            break;
        case  EEXIST:   /* File exists */
        
 /* RESTART here */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  EXDEV:    /* Cross device link */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  EINVAL:   /* Invalid argument */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  ENFILE:   /* File table overflow */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  EMFILE:   /* Too many open files */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  ENOSPC:   /* No space left on device */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        /* math errors */
        case  EDOM:     /* Argument too large */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  ERANGE:   /* Result too large */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        /* file locking error */
        case  EDEADLK:      /* Resource deadlock would occur */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  EINTR:    /* Interrupt */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  ECHILD:   /* Child does not exist */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        /* POSIX errors */
        case  EAGAIN:   /* Resource unavailable, try again */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  EBUSY:    /* Device or Resource is busy */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  EFBIG:    /* File too large */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  EIO:      /* I/O error */
            PR_SetError( PR_IO_ERROR, err );
            break;
        case  EISDIR:   /* Is a directory */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  ENOTDIR:  /* Not a directory */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  EMLINK:   /* Too many links */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  ENOTBLK:  /* Block device required */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  ENOTTY:   /* Not a character device */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  ENXIO:    /* No such device or address */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  EPERM:    /* Not owner */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  EPIPE:    /* Broken pipe */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  EROFS:    /* Read only file system */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  ESPIPE:   /* Illegal seek */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  ESRCH:    /* No such process */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  ETXTBSY:  /* Text file busy */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  EFAULT:   /* Bad address */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  ENAMETOOLONG: /* Name too long */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  ENODEV:   /* No such device */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  ENOLCK:   /* No locks available on system */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  ENOSYS:   /* Unknown system call */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
        case  ENOTEMPTY:    /* Directory not empty */
        /* Normative Addendum error */
        case  EILSEQ:   /* Multibyte/widw character encoding error */
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
            
        /* WinSock errors */
        case WSAEACCES:
            PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
            break;
        case WSAEADDRINUSE:
            PR_SetError(PR_ADDRESS_IN_USE_ERROR, err);
            break;
        case WSAEADDRNOTAVAIL:
            PR_SetError(PR_ADDRESS_NOT_AVAILABLE_ERROR, err);
            break;
        case WSAEAFNOSUPPORT:
            PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, err);
            break;
        case WSAEBADF:
            PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
            break;
        case WSAECONNREFUSED:
            PR_SetError(PR_CONNECT_REFUSED_ERROR, err);
            break;
        case WSAEFAULT:
            PR_SetError(PR_ACCESS_FAULT_ERROR, err);
            break;
        case WSAEINVAL:
            PR_SetError(PR_BUFFER_OVERFLOW_ERROR, err);
            break;
        case WSAEISCONN:
            PR_SetError(PR_IS_CONNECTED_ERROR, err);
            break;
        case WSAEMFILE:
            PR_SetError(PR_PROC_DESC_TABLE_FULL_ERROR, err);
            break;
        case WSAENETDOWN:
        case WSAENETUNREACH:
            PR_SetError(PR_NETWORK_UNREACHABLE_ERROR, err);
            break;
        case WSAENOBUFS:
            PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
            break;
        case WSAENOPROTOOPT:
        case WSAEMSGSIZE:
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
            break;
        case WSAENOTCONN:
            PR_SetError(PR_NOT_CONNECTED_ERROR, err);
            break;
        case WSAENOTSOCK:
            PR_SetError(PR_NOT_SOCKET_ERROR, err);
            break;
        case WSAEOPNOTSUPP:
            PR_SetError(PR_NOT_TCP_SOCKET_ERROR, err);
            break;
        case WSAEPROTONOSUPPORT:
            PR_SetError(PR_PROTOCOL_NOT_SUPPORTED_ERROR, err);
            break;
        case WSAETIMEDOUT:
            PR_SetError(PR_IO_TIMEOUT_ERROR, err);
            break;
        case WSAEINTR:
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, err );
            break;
        case WSASYSNOTREADY:
        case WSAVERNOTSUPPORTED:
            PR_SetError(PR_PROTOCOL_NOT_SUPPORTED_ERROR, err);
            break;
		case WSAEWOULDBLOCK:
			PR_SetError(PR_WOULD_BLOCK_ERROR, err);
			break;
            
        default:
            PR_SetError( PR_UNKNOWN_ERROR, err );
            break;
    }
    return;
} /* end _MD_map_win16_error() */


