/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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


