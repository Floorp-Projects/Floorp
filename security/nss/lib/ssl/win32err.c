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

#include "prerror.h"
#include "prlog.h"
#include <errno.h>
#include <windows.h>

/*
 * On Win32, we map three kinds of error codes:
 * - GetLastError(): for Win32 functions
 * - WSAGetLastError(): for Winsock functions
 * - errno: for standard C library functions
 *
 * We do not check for WSAEINPROGRESS and WSAEINTR because we do not
 * use blocking Winsock 1.1 calls.
 *
 * Except for the 'socket' call, we do not check for WSAEINITIALISED.
 * It is assumed that if Winsock is not initialized, that fact will
 * be detected at the time we create new sockets.
 */

/* forward declaration. */
void nss_MD_win32_map_default_error(PRInt32 err);

void nss_MD_win32_map_opendir_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_closedir_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_readdir_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_delete_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

/* The error code for stat() is in errno. */
void nss_MD_win32_map_stat_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_fstat_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_rename_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

/* The error code for access() is in errno. */
void nss_MD_win32_map_access_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_mkdir_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_rmdir_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_read_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_transmitfile_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_write_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_lseek_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_fsync_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

/*
 * For both CloseHandle() and closesocket().
 */
void nss_MD_win32_map_close_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_socket_error(PRInt32 err)
{
    PR_ASSERT(err != WSANOTINITIALISED);
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_recv_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_recvfrom_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_send_error(PRInt32 err)
{
    PRErrorCode prError;
    switch (err) {
    case WSAEMSGSIZE: 	prError = PR_INVALID_ARGUMENT_ERROR; break;
    default:		nss_MD_win32_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_win32_map_sendto_error(PRInt32 err)
{
    PRErrorCode prError;
    switch (err) {
    case WSAEMSGSIZE: 	prError = PR_INVALID_ARGUMENT_ERROR; break;
    default:		nss_MD_win32_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_win32_map_accept_error(PRInt32 err)
{
    PRErrorCode prError;
    switch (err) {
    case WSAEOPNOTSUPP: prError = PR_NOT_TCP_SOCKET_ERROR; break;
    case WSAEINVAL: 	prError = PR_INVALID_STATE_ERROR; break;
    default:		nss_MD_win32_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_win32_map_acceptex_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_connect_error(PRInt32 err)
{
    PRErrorCode prError;
    switch (err) {
    case WSAEWOULDBLOCK: prError = PR_IN_PROGRESS_ERROR; break;
    case WSAEINVAL: 	prError = PR_ALREADY_INITIATED_ERROR; break;
    case WSAETIMEDOUT: 	prError = PR_IO_TIMEOUT_ERROR; break;
    default:		nss_MD_win32_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_win32_map_bind_error(PRInt32 err)
{
    PRErrorCode prError;
    switch (err) {
    case WSAEINVAL: 	prError = PR_SOCKET_ADDRESS_IS_BOUND_ERROR; break;
    default:		nss_MD_win32_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_win32_map_listen_error(PRInt32 err)
{
    PRErrorCode prError;
    switch (err) {
    case WSAEOPNOTSUPP: prError = PR_NOT_TCP_SOCKET_ERROR; break;
    case WSAEINVAL: 	prError = PR_INVALID_STATE_ERROR; break;
    default:		nss_MD_win32_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_win32_map_shutdown_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_getsockname_error(PRInt32 err)
{
    PRErrorCode prError;
    switch (err) {
    case WSAEINVAL: 	prError = PR_INVALID_STATE_ERROR; break;
    default:		nss_MD_win32_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_win32_map_getpeername_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_getsockopt_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_setsockopt_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_open_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

void nss_MD_win32_map_gethostname_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}

/* Win32 select() only works on sockets.  So in this
** context, WSAENOTSOCK is equivalent to EBADF on Unix.  
*/
void nss_MD_win32_map_select_error(PRInt32 err)
{
    PRErrorCode prError;
    switch (err) {
    case WSAENOTSOCK:	prError = PR_BAD_DESCRIPTOR_ERROR; break;
    default:		nss_MD_win32_map_default_error(err); return;
    }
    PR_SetError(prError, err);
}

void nss_MD_win32_map_lockf_error(PRInt32 err)
{
    nss_MD_win32_map_default_error(err);
}



void nss_MD_win32_map_default_error(PRInt32 err)
{
    PRErrorCode prError;

    switch (err) {
    case EACCES: 		prError = PR_NO_ACCESS_RIGHTS_ERROR; break;
    case ENOENT: 		prError = PR_FILE_NOT_FOUND_ERROR; break;
    case ERROR_ACCESS_DENIED: 	prError = PR_NO_ACCESS_RIGHTS_ERROR; break;
    case ERROR_ALREADY_EXISTS: 	prError = PR_FILE_EXISTS_ERROR; break;
    case ERROR_DISK_CORRUPT: 	prError = PR_IO_ERROR; break;
    case ERROR_DISK_FULL: 	prError = PR_NO_DEVICE_SPACE_ERROR; break;
    case ERROR_DISK_OPERATION_FAILED: prError = PR_IO_ERROR; break;
    case ERROR_DRIVE_LOCKED: 	prError = PR_FILE_IS_LOCKED_ERROR; break;
    case ERROR_FILENAME_EXCED_RANGE: prError = PR_NAME_TOO_LONG_ERROR; break;
    case ERROR_FILE_CORRUPT: 	prError = PR_IO_ERROR; break;
    case ERROR_FILE_EXISTS: 	prError = PR_FILE_EXISTS_ERROR; break;
    case ERROR_FILE_INVALID: 	prError = PR_BAD_DESCRIPTOR_ERROR; break;
#if ERROR_FILE_NOT_FOUND != ENOENT
    case ERROR_FILE_NOT_FOUND: 	prError = PR_FILE_NOT_FOUND_ERROR; break;
#endif
    case ERROR_HANDLE_DISK_FULL: prError = PR_NO_DEVICE_SPACE_ERROR; break;
    case ERROR_INVALID_ADDRESS: prError = PR_ACCESS_FAULT_ERROR; break;
    case ERROR_INVALID_HANDLE: 	prError = PR_BAD_DESCRIPTOR_ERROR; break;
    case ERROR_INVALID_NAME: 	prError = PR_INVALID_ARGUMENT_ERROR; break;
    case ERROR_INVALID_PARAMETER: prError = PR_INVALID_ARGUMENT_ERROR; break;
    case ERROR_INVALID_USER_BUFFER: prError = PR_INSUFFICIENT_RESOURCES_ERROR; break;
    case ERROR_LOCKED:	 	prError = PR_FILE_IS_LOCKED_ERROR; break;
    case ERROR_NETNAME_DELETED: prError = PR_CONNECT_RESET_ERROR; break;
    case ERROR_NOACCESS: 	prError = PR_ACCESS_FAULT_ERROR; break;
    case ERROR_NOT_ENOUGH_MEMORY: prError = PR_INSUFFICIENT_RESOURCES_ERROR; break;
    case ERROR_NOT_ENOUGH_QUOTA: prError = PR_OUT_OF_MEMORY_ERROR; break;
    case ERROR_NOT_READY: 	prError = PR_IO_ERROR; break;
    case ERROR_NO_MORE_FILES: 	prError = PR_NO_MORE_FILES_ERROR; break;
    case ERROR_OPEN_FAILED: 	prError = PR_IO_ERROR; break;
    case ERROR_OPEN_FILES: 	prError = PR_IO_ERROR; break;
    case ERROR_OUTOFMEMORY: 	prError = PR_INSUFFICIENT_RESOURCES_ERROR; break;
    case ERROR_PATH_BUSY: 	prError = PR_IO_ERROR; break;
    case ERROR_PATH_NOT_FOUND: 	prError = PR_FILE_NOT_FOUND_ERROR; break;
    case ERROR_SEEK_ON_DEVICE: 	prError = PR_IO_ERROR; break;
    case ERROR_SHARING_VIOLATION: prError = PR_FILE_IS_BUSY_ERROR; break;
    case ERROR_STACK_OVERFLOW: 	prError = PR_ACCESS_FAULT_ERROR; break;
    case ERROR_TOO_MANY_OPEN_FILES: prError = PR_SYS_DESC_TABLE_FULL_ERROR; break;
    case ERROR_WRITE_PROTECT: 	prError = PR_NO_ACCESS_RIGHTS_ERROR; break;
    case WSAEACCES: 		prError = PR_NO_ACCESS_RIGHTS_ERROR; break;
    case WSAEADDRINUSE: 	prError = PR_ADDRESS_IN_USE_ERROR; break;
    case WSAEADDRNOTAVAIL: 	prError = PR_ADDRESS_NOT_AVAILABLE_ERROR; break;
    case WSAEAFNOSUPPORT: 	prError = PR_ADDRESS_NOT_SUPPORTED_ERROR; break;
    case WSAEALREADY: 		prError = PR_ALREADY_INITIATED_ERROR; break;
    case WSAEBADF: 		prError = PR_BAD_DESCRIPTOR_ERROR; break;
    case WSAECONNABORTED: 	prError = PR_CONNECT_ABORTED_ERROR; break;
    case WSAECONNREFUSED: 	prError = PR_CONNECT_REFUSED_ERROR; break;
    case WSAECONNRESET: 	prError = PR_CONNECT_RESET_ERROR; break;
    case WSAEDESTADDRREQ: 	prError = PR_INVALID_ARGUMENT_ERROR; break;
    case WSAEFAULT: 		prError = PR_ACCESS_FAULT_ERROR; break;
    case WSAEHOSTUNREACH: 	prError = PR_HOST_UNREACHABLE_ERROR; break;
    case WSAEINVAL: 		prError = PR_INVALID_ARGUMENT_ERROR; break;
    case WSAEISCONN: 		prError = PR_IS_CONNECTED_ERROR; break;
    case WSAEMFILE: 		prError = PR_PROC_DESC_TABLE_FULL_ERROR; break;
    case WSAEMSGSIZE: 		prError = PR_BUFFER_OVERFLOW_ERROR; break;
    case WSAENETDOWN: 		prError = PR_NETWORK_DOWN_ERROR; break;
    case WSAENETRESET: 		prError = PR_CONNECT_ABORTED_ERROR; break;
    case WSAENETUNREACH: 	prError = PR_NETWORK_UNREACHABLE_ERROR; break;
    case WSAENOBUFS: 		prError = PR_INSUFFICIENT_RESOURCES_ERROR; break;
    case WSAENOPROTOOPT: 	prError = PR_INVALID_ARGUMENT_ERROR; break;
    case WSAENOTCONN: 		prError = PR_NOT_CONNECTED_ERROR; break;
    case WSAENOTSOCK: 		prError = PR_NOT_SOCKET_ERROR; break;
    case WSAEOPNOTSUPP: 	prError = PR_OPERATION_NOT_SUPPORTED_ERROR; break;
    case WSAEPROTONOSUPPORT: 	prError = PR_PROTOCOL_NOT_SUPPORTED_ERROR; break;
    case WSAEPROTOTYPE: 	prError = PR_INVALID_ARGUMENT_ERROR; break;
    case WSAESHUTDOWN: 		prError = PR_SOCKET_SHUTDOWN_ERROR; break;
    case WSAESOCKTNOSUPPORT: 	prError = PR_INVALID_ARGUMENT_ERROR; break;
    case WSAETIMEDOUT: 		prError = PR_CONNECT_ABORTED_ERROR; break;
    case WSAEWOULDBLOCK: 	prError = PR_WOULD_BLOCK_ERROR; break;
    default: 			prError = PR_UNKNOWN_ERROR; break;
    }
    PR_SetError(prError, err);
}

