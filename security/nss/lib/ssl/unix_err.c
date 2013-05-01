/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file essentially replicates NSPR's source for the functions that
 * map system-specific error codes to NSPR error codes.  We would use 
 * NSPR's functions, instead of duplicating them, but they're private.
 * As long as SSL's server session cache code must do platform native I/O
 * to accomplish its job, and NSPR's error mapping functions remain private,
 * this code will continue to need to be replicated.
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id$ */

#if 0
#include "primpl.h"
#else
#define _PR_POLL_AVAILABLE 1
#include "prerror.h"
#endif

#if defined (__bsdi__) || defined(NTO) || defined(DARWIN) || defined(BEOS)
#undef _PR_POLL_AVAILABLE
#endif

#if defined(_PR_POLL_AVAILABLE)
#include <poll.h>
#endif
#include <errno.h>

/* forward declarations. */
void nss_MD_unix_map_default_error(int err);

void nss_MD_unix_map_opendir_error(int err)
{
    nss_MD_unix_map_default_error(err);
}

void nss_MD_unix_map_closedir_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case EINVAL:	prError = PR_BAD_DESCRIPTOR_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_readdir_error(int err)
{
    PRErrorCode prError;

    switch (err) {
    case ENOENT:	prError = PR_NO_MORE_FILES_ERROR; break;
#ifdef EOVERFLOW
    case EOVERFLOW:	prError = PR_IO_ERROR; break;
#endif
    case EINVAL:	prError = PR_IO_ERROR; break;
    case ENXIO:		prError = PR_IO_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_unlink_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case EPERM:		prError = PR_IS_DIRECTORY_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_stat_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case ETIMEDOUT:	prError = PR_REMOTE_FILE_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_fstat_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case ETIMEDOUT:	prError = PR_REMOTE_FILE_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_rename_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case EEXIST:	prError = PR_DIRECTORY_NOT_EMPTY_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_access_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case ETIMEDOUT:	prError = PR_REMOTE_FILE_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_mkdir_error(int err)
{
    nss_MD_unix_map_default_error(err);
}

void nss_MD_unix_map_rmdir_error(int err)
{
    PRErrorCode prError;

    switch (err) {
    case EEXIST:	prError = PR_DIRECTORY_NOT_EMPTY_ERROR; break;
    case EINVAL:	prError = PR_DIRECTORY_NOT_EMPTY_ERROR; break;
    case ETIMEDOUT:	prError = PR_REMOTE_FILE_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_read_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case EINVAL:	prError = PR_INVALID_METHOD_ERROR; break;
    case ENXIO:		prError = PR_INVALID_ARGUMENT_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_write_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case EINVAL:	prError = PR_INVALID_METHOD_ERROR; break;
    case ENXIO:		prError = PR_INVALID_METHOD_ERROR; break;
    case ETIMEDOUT:	prError = PR_REMOTE_FILE_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_lseek_error(int err)
{
    nss_MD_unix_map_default_error(err);
}

void nss_MD_unix_map_fsync_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case ETIMEDOUT:	prError = PR_REMOTE_FILE_ERROR; break;
    case EINVAL:	prError = PR_INVALID_METHOD_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_close_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case ETIMEDOUT:	prError = PR_REMOTE_FILE_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_socket_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case ENOMEM:	prError = PR_INSUFFICIENT_RESOURCES_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_socketavailable_error(int err)
{
    PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
}

void nss_MD_unix_map_recv_error(int err)
{
    nss_MD_unix_map_default_error(err);
}

void nss_MD_unix_map_recvfrom_error(int err)
{
    nss_MD_unix_map_default_error(err);
}

void nss_MD_unix_map_send_error(int err)
{
    nss_MD_unix_map_default_error(err);
}

void nss_MD_unix_map_sendto_error(int err)
{
    nss_MD_unix_map_default_error(err);
}

void nss_MD_unix_map_writev_error(int err)
{
    nss_MD_unix_map_default_error(err);
}

void nss_MD_unix_map_accept_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case ENODEV:	prError = PR_NOT_TCP_SOCKET_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_connect_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case EACCES:	prError = PR_ADDRESS_NOT_SUPPORTED_ERROR; break;
#if defined(UNIXWARE) || defined(SNI) || defined(NEC)
    /*
     * On some platforms, if we connect to a port on the local host 
     * (the loopback address) that no process is listening on, we get 
     * EIO instead of ECONNREFUSED.
     */
    case EIO:		prError = PR_CONNECT_REFUSED_ERROR; break;
#endif
    case ELOOP:		prError = PR_ADDRESS_NOT_SUPPORTED_ERROR; break;
    case ENOENT:	prError = PR_ADDRESS_NOT_SUPPORTED_ERROR; break;
    case ENXIO:		prError = PR_IO_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_bind_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case EINVAL:	prError = PR_SOCKET_ADDRESS_IS_BOUND_ERROR; break;
        /*
         * UNIX domain sockets are not supported in NSPR
         */
    case EIO:		prError = PR_ADDRESS_NOT_SUPPORTED_ERROR; break;
    case EISDIR:	prError = PR_ADDRESS_NOT_SUPPORTED_ERROR; break;
    case ELOOP:		prError = PR_ADDRESS_NOT_SUPPORTED_ERROR; break;
    case ENOENT:	prError = PR_ADDRESS_NOT_SUPPORTED_ERROR; break;
    case ENOTDIR:	prError = PR_ADDRESS_NOT_SUPPORTED_ERROR; break;
    case EROFS:		prError = PR_ADDRESS_NOT_SUPPORTED_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_listen_error(int err)
{
    nss_MD_unix_map_default_error(err);
}

void nss_MD_unix_map_shutdown_error(int err)
{
    nss_MD_unix_map_default_error(err);
}

void nss_MD_unix_map_socketpair_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case ENOMEM:	prError = PR_INSUFFICIENT_RESOURCES_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_getsockname_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case ENOMEM:	prError = PR_INSUFFICIENT_RESOURCES_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_getpeername_error(int err)
{
    PRErrorCode prError;

    switch (err) {
    case ENOMEM:	prError = PR_INSUFFICIENT_RESOURCES_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_getsockopt_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case EINVAL:	prError = PR_BUFFER_OVERFLOW_ERROR; break;
    case ENOMEM:	prError = PR_INSUFFICIENT_RESOURCES_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_setsockopt_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case EINVAL:	prError = PR_BUFFER_OVERFLOW_ERROR; break;
    case ENOMEM:	prError = PR_INSUFFICIENT_RESOURCES_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_open_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case EAGAIN:	prError = PR_INSUFFICIENT_RESOURCES_ERROR; break;
    case EBUSY:		prError = PR_IO_ERROR; break;
    case ENODEV:	prError = PR_FILE_NOT_FOUND_ERROR; break;
    case ENOMEM:	prError = PR_INSUFFICIENT_RESOURCES_ERROR; break;
    case ETIMEDOUT:	prError = PR_REMOTE_FILE_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_mmap_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case EAGAIN:	prError = PR_INSUFFICIENT_RESOURCES_ERROR; break;
    case EMFILE:	prError = PR_INSUFFICIENT_RESOURCES_ERROR; break;
    case ENODEV:	prError = PR_OPERATION_NOT_SUPPORTED_ERROR; break;
    case ENXIO:		prError = PR_INVALID_ARGUMENT_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_gethostname_error(int err)
{
    nss_MD_unix_map_default_error(err);
}

void nss_MD_unix_map_select_error(int err)
{
    nss_MD_unix_map_default_error(err);
}

#ifdef _PR_POLL_AVAILABLE
void nss_MD_unix_map_poll_error(int err)
{
    PRErrorCode prError;

    switch (err) {
    case EAGAIN:	prError = PR_INSUFFICIENT_RESOURCES_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_poll_revents_error(int err)
{
    if (err & POLLNVAL)
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, EBADF);
    else if (err & POLLHUP)
        PR_SetError(PR_CONNECT_RESET_ERROR, EPIPE);
    else if (err & POLLERR)
        PR_SetError(PR_IO_ERROR, EIO);
    else
        PR_SetError(PR_UNKNOWN_ERROR, err);
}
#endif /* _PR_POLL_AVAILABLE */


void nss_MD_unix_map_flock_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case EINVAL:	prError = PR_BAD_DESCRIPTOR_ERROR; break;
    case EWOULDBLOCK:	prError = PR_FILE_IS_LOCKED_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_unix_map_lockf_error(int err)
{
    PRErrorCode prError;
    switch (err) {
    case EACCES:	prError = PR_FILE_IS_LOCKED_ERROR; break;
    case EDEADLK:	prError = PR_INSUFFICIENT_RESOURCES_ERROR; break;
    default:		nss_MD_unix_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

#ifdef HPUX11
void nss_MD_hpux_map_sendfile_error(int err)
{
    nss_MD_unix_map_default_error(err);
}
#endif /* HPUX11 */


void nss_MD_unix_map_default_error(int err)
{
    PRErrorCode prError;
    switch (err ) {
    case EACCES:	prError = PR_NO_ACCESS_RIGHTS_ERROR; break;
    case EADDRINUSE:	prError = PR_ADDRESS_IN_USE_ERROR; break;
    case EADDRNOTAVAIL:	prError = PR_ADDRESS_NOT_AVAILABLE_ERROR; break;
    case EAFNOSUPPORT:	prError = PR_ADDRESS_NOT_SUPPORTED_ERROR; break;
    case EAGAIN:	prError = PR_WOULD_BLOCK_ERROR; break;
    /*
     * On QNX and Neutrino, EALREADY is defined as EBUSY.
     */
#if EALREADY != EBUSY
    case EALREADY:	prError = PR_ALREADY_INITIATED_ERROR; break;
#endif
    case EBADF:		prError = PR_BAD_DESCRIPTOR_ERROR; break;
#ifdef EBADMSG
    case EBADMSG:	prError = PR_IO_ERROR; break;
#endif
    case EBUSY:		prError = PR_FILESYSTEM_MOUNTED_ERROR; break;
    case ECONNREFUSED:	prError = PR_CONNECT_REFUSED_ERROR; break;
    case ECONNRESET:	prError = PR_CONNECT_RESET_ERROR; break;
    case EDEADLK:	prError = PR_DEADLOCK_ERROR; break;
#ifdef EDIRCORRUPTED
    case EDIRCORRUPTED:	prError = PR_DIRECTORY_CORRUPTED_ERROR; break;
#endif
#ifdef EDQUOT
    case EDQUOT:	prError = PR_NO_DEVICE_SPACE_ERROR; break;
#endif
    case EEXIST:	prError = PR_FILE_EXISTS_ERROR; break;
    case EFAULT:	prError = PR_ACCESS_FAULT_ERROR; break;
    case EFBIG:		prError = PR_FILE_TOO_BIG_ERROR; break;
    case EINPROGRESS:	prError = PR_IN_PROGRESS_ERROR; break;
    case EINTR:		prError = PR_PENDING_INTERRUPT_ERROR; break;
    case EINVAL:	prError = PR_INVALID_ARGUMENT_ERROR; break;
    case EIO:		prError = PR_IO_ERROR; break;
    case EISCONN:	prError = PR_IS_CONNECTED_ERROR; break;
    case EISDIR:	prError = PR_IS_DIRECTORY_ERROR; break;
    case ELOOP:		prError = PR_LOOP_ERROR; break;
    case EMFILE:	prError = PR_PROC_DESC_TABLE_FULL_ERROR; break;
    case EMLINK:	prError = PR_MAX_DIRECTORY_ENTRIES_ERROR; break;
    case EMSGSIZE:	prError = PR_INVALID_ARGUMENT_ERROR; break;
#ifdef EMULTIHOP
    case EMULTIHOP:	prError = PR_REMOTE_FILE_ERROR; break;
#endif
    case ENAMETOOLONG:	prError = PR_NAME_TOO_LONG_ERROR; break;
    case ENETUNREACH:	prError = PR_NETWORK_UNREACHABLE_ERROR; break;
    case ENFILE:	prError = PR_SYS_DESC_TABLE_FULL_ERROR; break;
#if !defined(SCO)
    case ENOBUFS:	prError = PR_INSUFFICIENT_RESOURCES_ERROR; break;
#endif
    case ENODEV:	prError = PR_FILE_NOT_FOUND_ERROR; break;
    case ENOENT:	prError = PR_FILE_NOT_FOUND_ERROR; break;
    case ENOLCK:	prError = PR_FILE_IS_LOCKED_ERROR; break;
#ifdef ENOLINK 
    case ENOLINK:	prError = PR_REMOTE_FILE_ERROR; break;
#endif
    case ENOMEM:	prError = PR_OUT_OF_MEMORY_ERROR; break;
    case ENOPROTOOPT:	prError = PR_INVALID_ARGUMENT_ERROR; break;
    case ENOSPC:	prError = PR_NO_DEVICE_SPACE_ERROR; break;
#ifdef ENOSR 
    case ENOSR:		prError = PR_INSUFFICIENT_RESOURCES_ERROR; break;
#endif
    case ENOTCONN:	prError = PR_NOT_CONNECTED_ERROR; break;
    case ENOTDIR:	prError = PR_NOT_DIRECTORY_ERROR; break;
    case ENOTSOCK:	prError = PR_NOT_SOCKET_ERROR; break;
    case ENXIO:		prError = PR_FILE_NOT_FOUND_ERROR; break;
    case EOPNOTSUPP:	prError = PR_NOT_TCP_SOCKET_ERROR; break;
#ifdef EOVERFLOW
    case EOVERFLOW:	prError = PR_BUFFER_OVERFLOW_ERROR; break;
#endif
    case EPERM:		prError = PR_NO_ACCESS_RIGHTS_ERROR; break;
    case EPIPE:		prError = PR_CONNECT_RESET_ERROR; break;
#ifdef EPROTO
    case EPROTO:	prError = PR_IO_ERROR; break;
#endif
    case EPROTONOSUPPORT: prError = PR_PROTOCOL_NOT_SUPPORTED_ERROR; break;
    case EPROTOTYPE:	prError = PR_ADDRESS_NOT_SUPPORTED_ERROR; break;
    case ERANGE:	prError = PR_INVALID_METHOD_ERROR; break;
    case EROFS:		prError = PR_READ_ONLY_FILESYSTEM_ERROR; break;
    case ESPIPE:	prError = PR_INVALID_METHOD_ERROR; break;
    case ETIMEDOUT:	prError = PR_IO_TIMEOUT_ERROR; break;
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:	prError = PR_WOULD_BLOCK_ERROR; break;
#endif
    case EXDEV:		prError = PR_NOT_SAME_DEVICE_ERROR; break;

    default:		prError = PR_UNKNOWN_ERROR; break;
    }
    PR_SetError(prError, err);
}
