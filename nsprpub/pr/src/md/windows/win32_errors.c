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
 * GetLastError() and WSAGetLastError() return error codes in
 * non-overlapping ranges, so their error codes (ERROR_* and
 * WSAE*) can be mapped by the same function.  On the other hand,
 * errno and GetLastError() have overlapping ranges, so we need
 * to use a separate function to map errno.
 *
 * We do not check for WSAEINPROGRESS and WSAEINTR because we do not
 * use blocking Winsock 1.1 calls.
 *
 * Except for the 'socket' call, we do not check for WSAEINITIALISED.
 * It is assumed that if Winsock is not initialized, that fact will
 * be detected at the time we create new sockets.
 */

static void _MD_win32_map_default_errno(PRInt32 err)
{
    PRErrorCode prError;

    switch (err) {
        case EACCES:
            prError = PR_NO_ACCESS_RIGHTS_ERROR;
            break;
        case ENOENT:
            prError = PR_FILE_NOT_FOUND_ERROR;
            break;
        default:
            prError = PR_UNKNOWN_ERROR;
            break;
    }
    PR_SetError(prError, err);
}

static void _MD_win32_map_default_error(PRInt32 err)
{
    PRErrorCode prError;

    switch (err) {
        case ERROR_ACCESS_DENIED:
            prError = PR_NO_ACCESS_RIGHTS_ERROR;
            break;
        case ERROR_ALREADY_EXISTS:
            prError = PR_FILE_EXISTS_ERROR;
            break;
        case ERROR_DISK_CORRUPT:
            prError = PR_IO_ERROR; 
            break;
        case ERROR_DISK_FULL:
            prError = PR_NO_DEVICE_SPACE_ERROR;
            break;
        case ERROR_DISK_OPERATION_FAILED:
            prError = PR_IO_ERROR;
            break;
        case ERROR_DRIVE_LOCKED:
            prError = PR_FILE_IS_LOCKED_ERROR;
            break;
        case ERROR_FILENAME_EXCED_RANGE:
            prError = PR_NAME_TOO_LONG_ERROR;
            break;
        case ERROR_FILE_CORRUPT:
            prError = PR_IO_ERROR;
            break;
        case ERROR_FILE_EXISTS:
            prError = PR_FILE_EXISTS_ERROR;
            break;
        case ERROR_FILE_INVALID:
            prError = PR_BAD_DESCRIPTOR_ERROR;
            break;
        case ERROR_FILE_NOT_FOUND:
            prError = PR_FILE_NOT_FOUND_ERROR;
            break;
        case ERROR_HANDLE_DISK_FULL:
            prError = PR_NO_DEVICE_SPACE_ERROR;
            break;
        case ERROR_INVALID_ADDRESS:
            prError = PR_ACCESS_FAULT_ERROR;
            break;
        case ERROR_INVALID_HANDLE:
            prError = PR_BAD_DESCRIPTOR_ERROR;
            break;
        case ERROR_INVALID_NAME:
            prError = PR_INVALID_ARGUMENT_ERROR;
            break;
        case ERROR_INVALID_PARAMETER:
            prError = PR_INVALID_ARGUMENT_ERROR;
            break;
        case ERROR_INVALID_USER_BUFFER:
            prError = PR_INSUFFICIENT_RESOURCES_ERROR;
            break;
        case ERROR_LOCKED:
            prError = PR_FILE_IS_LOCKED_ERROR;
            break;
        case ERROR_NETNAME_DELETED:
            prError = PR_CONNECT_RESET_ERROR;
            break;
        case ERROR_NOACCESS:
            prError = PR_ACCESS_FAULT_ERROR;
            break;
        case ERROR_NOT_ENOUGH_MEMORY:
            prError = PR_INSUFFICIENT_RESOURCES_ERROR;
            break;
        case ERROR_NOT_ENOUGH_QUOTA:
            prError = PR_OUT_OF_MEMORY_ERROR;
            break;
        case ERROR_NOT_READY:
            prError = PR_IO_ERROR;
            break;
        case ERROR_NO_MORE_FILES:
            prError = PR_NO_MORE_FILES_ERROR;
            break;
        case ERROR_OPEN_FAILED:
            prError = PR_IO_ERROR;
            break;
        case ERROR_OPEN_FILES:
            prError = PR_IO_ERROR;
            break;
        case ERROR_OUTOFMEMORY:
            prError = PR_INSUFFICIENT_RESOURCES_ERROR;
            break;
        case ERROR_PATH_BUSY:
            prError = PR_IO_ERROR;
            break;
        case ERROR_PATH_NOT_FOUND:
            prError = PR_FILE_NOT_FOUND_ERROR;
            break;
        case ERROR_SEEK_ON_DEVICE:
            prError = PR_IO_ERROR;
            break;
        case ERROR_SHARING_VIOLATION:
            prError = PR_FILE_IS_BUSY_ERROR;
            break;
        case ERROR_STACK_OVERFLOW:
            prError = PR_ACCESS_FAULT_ERROR;
            break;
        case ERROR_TOO_MANY_OPEN_FILES:
            prError = PR_SYS_DESC_TABLE_FULL_ERROR;
            break;
        case ERROR_WRITE_PROTECT:
            prError = PR_NO_ACCESS_RIGHTS_ERROR;
            break;
        case WSAEACCES:
            prError = PR_NO_ACCESS_RIGHTS_ERROR;
            break;
        case WSAEADDRINUSE:
            prError = PR_ADDRESS_IN_USE_ERROR;
            break;
        case WSAEADDRNOTAVAIL:
            prError = PR_ADDRESS_NOT_AVAILABLE_ERROR;
            break;
        case WSAEAFNOSUPPORT:
            prError = PR_ADDRESS_NOT_SUPPORTED_ERROR;
            break;
        case WSAEALREADY:
            prError = PR_ALREADY_INITIATED_ERROR;
            break;
        case WSAEBADF:
            prError = PR_BAD_DESCRIPTOR_ERROR;
            break;
        case WSAECONNABORTED:
            prError = PR_CONNECT_ABORTED_ERROR;
            break;
        case WSAECONNREFUSED:
            prError = PR_CONNECT_REFUSED_ERROR;
            break;
        case WSAECONNRESET:
            prError = PR_CONNECT_RESET_ERROR;
            break;
        case WSAEDESTADDRREQ:
            prError = PR_INVALID_ARGUMENT_ERROR;
            break;
        case WSAEFAULT:
            prError = PR_ACCESS_FAULT_ERROR;
            break;
        case WSAEHOSTUNREACH:
            prError = PR_HOST_UNREACHABLE_ERROR;
            break;
        case WSAEINVAL:
            prError = PR_INVALID_ARGUMENT_ERROR;
            break;
        case WSAEISCONN:
            prError = PR_IS_CONNECTED_ERROR;
            break;
        case WSAEMFILE:
            prError = PR_PROC_DESC_TABLE_FULL_ERROR;
            break;
        case WSAEMSGSIZE:
            prError = PR_BUFFER_OVERFLOW_ERROR;
            break;
        case WSAENETDOWN:
            prError = PR_NETWORK_DOWN_ERROR;
            break;
        case WSAENETRESET:
            prError = PR_CONNECT_ABORTED_ERROR;
            break;
        case WSAENETUNREACH:
            prError = PR_NETWORK_UNREACHABLE_ERROR;
            break;
        case WSAENOBUFS:
            prError = PR_INSUFFICIENT_RESOURCES_ERROR;
            break;
        case WSAENOPROTOOPT:
            prError = PR_INVALID_ARGUMENT_ERROR;
            break;
        case WSAENOTCONN:
            prError = PR_NOT_CONNECTED_ERROR;
            break;
        case WSAENOTSOCK:
            prError = PR_NOT_SOCKET_ERROR;
            break;
        case WSAEOPNOTSUPP:
            prError = PR_OPERATION_NOT_SUPPORTED_ERROR;
            break;
        case WSAEPROTONOSUPPORT:
            prError = PR_PROTOCOL_NOT_SUPPORTED_ERROR;
            break;
        case WSAEPROTOTYPE:
            prError = PR_INVALID_ARGUMENT_ERROR;
            break;
        case WSAESHUTDOWN:
            prError = PR_SOCKET_SHUTDOWN_ERROR;
            break;
        case WSAESOCKTNOSUPPORT:
            prError = PR_INVALID_ARGUMENT_ERROR;
            break;
        case WSAETIMEDOUT:
            prError = PR_CONNECT_ABORTED_ERROR;
            break;
        case WSAEWOULDBLOCK:
            prError = PR_WOULD_BLOCK_ERROR;
            break;
        default:
            prError = PR_UNKNOWN_ERROR;
            break;
    }
    PR_SetError(prError, err);
}

void _MD_win32_map_opendir_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

void _MD_win32_map_closedir_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

void _MD_unix_readdir_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

void _MD_win32_map_delete_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

/* The error code for stat() is in errno. */
void _MD_win32_map_stat_error(PRInt32 err)
{
    _MD_win32_map_default_errno(err);
}

void _MD_win32_map_fstat_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

void _MD_win32_map_rename_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

/* The error code for access() is in errno. */
void _MD_win32_map_access_error(PRInt32 err)
{
    _MD_win32_map_default_errno(err);
}

void _MD_win32_map_mkdir_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

void _MD_win32_map_rmdir_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

void _MD_win32_map_read_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

void _MD_win32_map_transmitfile_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

void _MD_win32_map_write_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

void _MD_win32_map_lseek_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

void _MD_win32_map_fsync_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

/*
 * For both CloseHandle() and closesocket().
 */
void _MD_win32_map_close_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

void _MD_win32_map_socket_error(PRInt32 err)
{
    PR_ASSERT(err != WSANOTINITIALISED);
    _MD_win32_map_default_error(err);
}

void _MD_win32_map_recv_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

void _MD_win32_map_recvfrom_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

void _MD_win32_map_send_error(PRInt32 err)
{
    PRErrorCode prError;

    switch (err) {
        case WSAEMSGSIZE:
            prError = PR_INVALID_ARGUMENT_ERROR;
            break;
        default:
            _MD_win32_map_default_error(err);
            return;
    }
    PR_SetError(prError, err);
}

void _MD_win32_map_sendto_error(PRInt32 err)
{
    PRErrorCode prError;

    switch (err) {
        case WSAEMSGSIZE:
            prError = PR_INVALID_ARGUMENT_ERROR;
            break;
        default:
            _MD_win32_map_default_error(err);
            return;
    }
    PR_SetError(prError, err);
}

void _MD_win32_map_accept_error(PRInt32 err)
{
    PRErrorCode prError;

    switch (err) {
        case WSAEOPNOTSUPP:
            prError = PR_NOT_TCP_SOCKET_ERROR;
            break;
        case WSAEINVAL:
            prError = PR_INVALID_STATE_ERROR;
            break;
        default:
            _MD_win32_map_default_error(err);
            return;
    }
    PR_SetError(prError, err);
}

void _MD_win32_map_acceptex_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

void _MD_win32_map_connect_error(PRInt32 err)
{
    PRErrorCode prError;

    switch (err) {
        case WSAEWOULDBLOCK:
            prError = PR_IN_PROGRESS_ERROR;
            break;
        case WSAEINVAL:
            prError = PR_ALREADY_INITIATED_ERROR;
            break;
        case WSAETIMEDOUT:
            prError = PR_IO_TIMEOUT_ERROR;
            break;
        default:
            _MD_win32_map_default_error(err);
            return;
    }
    PR_SetError(prError, err);
}

void _MD_win32_map_bind_error(PRInt32 err)
{
    PRErrorCode prError;

    switch (err) {
        case WSAEINVAL:
            prError = PR_SOCKET_ADDRESS_IS_BOUND_ERROR;
            break;
        default:
            _MD_win32_map_default_error(err);
            return;
    }
    PR_SetError(prError, err);
}

void _MD_win32_map_listen_error(PRInt32 err)
{
    PRErrorCode prError;

    switch (err) {
        case WSAEOPNOTSUPP:
            prError = PR_NOT_TCP_SOCKET_ERROR;
            break;
        case WSAEINVAL:
            prError = PR_INVALID_STATE_ERROR;
            break;
        default:
            _MD_win32_map_default_error(err);
            return;
    }
    PR_SetError(prError, err);
}

void _MD_win32_map_shutdown_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

void _MD_win32_map_getsockname_error(PRInt32 err)
{
    PRErrorCode prError;

    switch (err) {
        case WSAEINVAL:
            prError = PR_INVALID_STATE_ERROR;
            break;
        default:
            _MD_win32_map_default_error(err);
            return;
    }
    PR_SetError(prError, err);
}

void _MD_win32_map_getpeername_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

void _MD_win32_map_getsockopt_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

void _MD_win32_map_setsockopt_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

void _MD_win32_map_open_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

void _MD_win32_map_gethostname_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}

/* Win32 select() only works on sockets.  So in this
** context, WSAENOTSOCK is equivalent to EBADF on Unix.  
*/
void _MD_win32_map_select_error(PRInt32 err)
{
    PRErrorCode prError;

    switch (err) {
        case WSAENOTSOCK:
            prError = PR_BAD_DESCRIPTOR_ERROR;
            break;
        default:
            _MD_win32_map_default_error(err);
            return;
    }
    PR_SetError(prError, err);
}

void _MD_win32_map_lockf_error(PRInt32 err)
{
    _MD_win32_map_default_error(err);
}
