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

#include "prerror.h"
#include "prlog.h"
#include <errno.h>

/*
 * Based on win32err.c
 * OS2TODO Stub everything for now to build. HCT
 */

/* forward declaration. */
void nss_MD_os2_map_default_error(PRInt32 err);

void
nss_MD_os2_map_opendir_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_closedir_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_readdir_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_delete_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

/* The error code for stat() is in errno. */
void
nss_MD_os2_map_stat_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_fstat_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_rename_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

/* The error code for access() is in errno. */
void
nss_MD_os2_map_access_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_mkdir_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_rmdir_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_read_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_transmitfile_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_write_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_lseek_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_fsync_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

/*
 * For both CloseHandle() and closesocket().
 */
void
nss_MD_os2_map_close_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_socket_error(PRInt32 err)
{
    //  PR_ASSERT(err != WSANOTINITIALISED);
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_recv_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_recvfrom_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_send_error(PRInt32 err)
{
    PRErrorCode prError;
    switch (err) {
        //     case WSAEMSGSIZE:  prError = PR_INVALID_ARGUMENT_ERROR; break;
        default:
            nss_MD_os2_map_default_error(err);
            return;
    }
    PR_SetError(prError, err);
}

void
nss_MD_os2_map_sendto_error(PRInt32 err)
{
    PRErrorCode prError;
    switch (err) {
        //    case WSAEMSGSIZE:     prError = PR_INVALID_ARGUMENT_ERROR; break;
        default:
            nss_MD_os2_map_default_error(err);
            return;
    }
    PR_SetError(prError, err);
}

void
nss_MD_os2_map_accept_error(PRInt32 err)
{
    PRErrorCode prError;
    switch (err) {
        //    case WSAEOPNOTSUPP: prError = PR_NOT_TCP_SOCKET_ERROR; break;
        //    case WSAEINVAL:   prError = PR_INVALID_STATE_ERROR; break;
        default:
            nss_MD_os2_map_default_error(err);
            return;
    }
    PR_SetError(prError, err);
}

void
nss_MD_os2_map_acceptex_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_connect_error(PRInt32 err)
{
    PRErrorCode prError;
    switch (err) {
        //    case WSAEWOULDBLOCK: prError = PR_IN_PROGRESS_ERROR; break;
        //    case WSAEINVAL:   prError = PR_ALREADY_INITIATED_ERROR; break;
        //    case WSAETIMEDOUT:    prError = PR_IO_TIMEOUT_ERROR; break;
        default:
            nss_MD_os2_map_default_error(err);
            return;
    }
    PR_SetError(prError, err);
}

void
nss_MD_os2_map_bind_error(PRInt32 err)
{
    PRErrorCode prError;
    switch (err) {
        //  case WSAEINVAL:     prError = PR_SOCKET_ADDRESS_IS_BOUND_ERROR; break;
        default:
            nss_MD_os2_map_default_error(err);
            return;
    }
    PR_SetError(prError, err);
}

void
nss_MD_os2_map_listen_error(PRInt32 err)
{
    PRErrorCode prError;
    switch (err) {
        //    case WSAEOPNOTSUPP: prError = PR_NOT_TCP_SOCKET_ERROR; break;
        //    case WSAEINVAL:   prError = PR_INVALID_STATE_ERROR; break;
        default:
            nss_MD_os2_map_default_error(err);
            return;
    }
    PR_SetError(prError, err);
}

void
nss_MD_os2_map_shutdown_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_getsockname_error(PRInt32 err)
{
    PRErrorCode prError;
    switch (err) {
        //    case WSAEINVAL:   prError = PR_INVALID_STATE_ERROR; break;
        default:
            nss_MD_os2_map_default_error(err);
            return;
    }
    PR_SetError(prError, err);
}

void
nss_MD_os2_map_getpeername_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_getsockopt_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_setsockopt_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_open_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_gethostname_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

/* Win32 select() only works on sockets.  So in this
** context, WSAENOTSOCK is equivalent to EBADF on Unix.
*/
void
nss_MD_os2_map_select_error(PRInt32 err)
{
    PRErrorCode prError;
    switch (err) {
        //    case WSAENOTSOCK: prError = PR_BAD_DESCRIPTOR_ERROR; break;
        default:
            nss_MD_os2_map_default_error(err);
            return;
    }
    PR_SetError(prError, err);
}

void
nss_MD_os2_map_lockf_error(PRInt32 err)
{
    nss_MD_os2_map_default_error(err);
}

void
nss_MD_os2_map_default_error(PRInt32 err)
{
    PRErrorCode prError;

    switch (err) {
//    case ENOENT:      prError = PR_FILE_NOT_FOUND_ERROR; break;
//    case ERROR_ACCESS_DENIED:     prError = PR_NO_ACCESS_RIGHTS_ERROR; break;
//    case ERROR_ALREADY_EXISTS:    prError = PR_FILE_EXISTS_ERROR; break;
//    case ERROR_DISK_CORRUPT:  prError = PR_IO_ERROR; break;
//    case ERROR_DISK_FULL:     prError = PR_NO_DEVICE_SPACE_ERROR; break;
//    case ERROR_DISK_OPERATION_FAILED: prError = PR_IO_ERROR; break;
//    case ERROR_DRIVE_LOCKED:  prError = PR_FILE_IS_LOCKED_ERROR; break;
//    case ERROR_FILENAME_EXCED_RANGE: prError = PR_NAME_TOO_LONG_ERROR; break;
//    case ERROR_FILE_CORRUPT:  prError = PR_IO_ERROR; break;
//    case ERROR_FILE_EXISTS:   prError = PR_FILE_EXISTS_ERROR; break;
//    case ERROR_FILE_INVALID:  prError = PR_BAD_DESCRIPTOR_ERROR; break;
#if ERROR_FILE_NOT_FOUND != ENOENT
//    case ERROR_FILE_NOT_FOUND:    prError = PR_FILE_NOT_FOUND_ERROR; break;
#endif
        default:
            prError = PR_UNKNOWN_ERROR;
            break;
    }
    PR_SetError(prError, err);
}
