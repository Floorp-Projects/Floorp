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
#include "primpl.h"

void _MD_os2_map_opendir_error(PRInt32 err)
{
	switch (err) {
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
			PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
			break;
		case ERROR_ACCESS_DENIED:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
      case ERROR_INVALID_ADDRESS:
      case ERROR_INVALID_ACCESS:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
      case ERROR_INVALID_NAME:
      case ERROR_INVALID_PARAMETER:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
      case ERROR_TOO_MANY_OPEN_FILES:
		case ERROR_NOT_DOS_DISK:
		case ERROR_NOT_READY:
		case ERROR_OPEN_FAILED:
      case ERROR_PATH_BUSY:
      case ERROR_CANNOT_MAKE:
			PR_SetError(PR_IO_ERROR, err);
			break;
      case ERROR_DRIVE_LOCKED:
      case ERROR_DEVICE_IN_USE:
			PR_SetError(PR_FILE_IS_LOCKED_ERROR, err);
			break;
		case ERROR_FILENAME_EXCED_RANGE:
			PR_SetError(PR_NAME_TOO_LONG_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_SHARING_BUFFER_EXCEEDED:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_closedir_error(PRInt32 err)
{
	switch (err) {
      case ERROR_FILE_NOT_FOUND:
      case ERROR_ACCESS_DENIED:
		case ERROR_INVALID_HANDLE:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_readdir_error(PRInt32 err)
{

	switch (err) {
		case ERROR_NO_MORE_FILES:
			PR_SetError(PR_NO_MORE_FILES_ERROR, err);
			break;
		case ERROR_FILE_NOT_FOUND:
		case ERROR_INVALID_HANDLE:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ERROR_INVALID_ADDRESS:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_NOT_DOS_DISK:
		case ERROR_LOCK_VIOLATION:
		case ERROR_BROKEN_PIPE:
		case ERROR_NOT_READY:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
      case ERROR_MORE_DATA:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_delete_error(PRInt32 err)
{
	switch (err) {
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
			PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
			break;
		case ERROR_ACCESS_DENIED:
		case ERROR_WRITE_PROTECT:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case ERROR_INVALID_ADDRESS:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_DRIVE_LOCKED:
		case ERROR_LOCKED:
		case ERROR_SHARING_VIOLATION:
			PR_SetError(PR_FILE_IS_LOCKED_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

/* The error code for stat() is in errno. */
void _MD_os2_map_stat_error(PRInt32 err)
{
    switch (err) {
        case ENOENT:
            PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
            break;
        case EACCES:
            PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
            break;
        default:
            PR_SetError(PR_UNKNOWN_ERROR, err);
    }
}

void _MD_os2_map_fstat_error(PRInt32 err)
{
	switch (err) {
		case ERROR_ACCESS_DENIED:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case ERROR_INVALID_HANDLE:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ERROR_INVALID_ADDRESS:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_NOT_READY:
		case ERROR_PATH_BUSY:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_MORE_DATA:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case ERROR_DRIVE_LOCKED:
		case ERROR_LOCKED:
			PR_SetError(PR_FILE_IS_LOCKED_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_rename_error(PRInt32 err)
{
	switch (err) {
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
			PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
			break;
		case ERROR_ACCESS_DENIED:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case ERROR_INVALID_ADDRESS:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_INVALID_NAME:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		case ERROR_NOT_READY:
		case ERROR_PATH_BUSY:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case ERROR_DRIVE_LOCKED:
			PR_SetError(PR_FILE_IS_LOCKED_ERROR, err);
			break;
		case ERROR_FILENAME_EXCED_RANGE:
			PR_SetError(PR_NAME_TOO_LONG_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_MORE_DATA:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case ERROR_ALREADY_EXISTS:
		case ERROR_FILE_EXISTS:
			PR_SetError(PR_FILE_EXISTS_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

/* The error code for access() is in errno. */
void _MD_os2_map_access_error(PRInt32 err)
{
    switch (err) {
        case ENOENT:
            PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
            break;
        case EACCES:
            PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
            break;
        default:
            PR_SetError(PR_UNKNOWN_ERROR, err);
    }
}

void _MD_os2_map_mkdir_error(PRInt32 err)
{
	switch (err) {
		case ERROR_ALREADY_EXISTS:
		case ERROR_FILE_EXISTS:
			PR_SetError(PR_FILE_EXISTS_ERROR, err);
			break;
		case ERROR_FILE_NOT_FOUND:
			PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
			break;
		case ERROR_ACCESS_DENIED:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case ERROR_INVALID_ADDRESS:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_INVALID_NAME:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		case ERROR_NOT_READY:
		case ERROR_PATH_BUSY:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case ERROR_DRIVE_LOCKED:
			PR_SetError(PR_FILE_IS_LOCKED_ERROR, err);
			break;
		case ERROR_FILENAME_EXCED_RANGE:
			PR_SetError(PR_NAME_TOO_LONG_ERROR, err);
			break;
		case ERROR_TOO_MANY_OPEN_FILES:
			PR_SetError(PR_SYS_DESC_TABLE_FULL_ERROR, err);
			break;
		case ERROR_PATH_NOT_FOUND:
			PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_MORE_DATA:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case ERROR_DISK_FULL:
		case ERROR_HANDLE_DISK_FULL:
			PR_SetError(PR_NO_DEVICE_SPACE_ERROR, err);
			break;
		case ERROR_WRITE_PROTECT:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_rmdir_error(PRInt32 err)
{

	switch (err) {
		case ERROR_FILE_NOT_FOUND:
			PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
			break;
		case ERROR_ACCESS_DENIED:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case ERROR_INVALID_ADDRESS:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_INVALID_NAME:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		case ERROR_NOT_READY:
		case ERROR_PATH_BUSY:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case ERROR_DRIVE_LOCKED:
			PR_SetError(PR_FILE_IS_LOCKED_ERROR, err);
			break;
		case ERROR_FILENAME_EXCED_RANGE:
			PR_SetError(PR_NAME_TOO_LONG_ERROR, err);
			break;
		case ERROR_TOO_MANY_OPEN_FILES:
			PR_SetError(PR_SYS_DESC_TABLE_FULL_ERROR, err);
			break;
		case ERROR_PATH_NOT_FOUND:
			PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_MORE_DATA:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case ERROR_WRITE_PROTECT:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_read_error(PRInt32 err)
{
	switch (err) {
		case ERROR_ACCESS_DENIED:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case ERROR_INVALID_HANDLE:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ERROR_INVALID_ADDRESS:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_NOT_READY:
		case ERROR_PATH_BUSY:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_MORE_DATA:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case ERROR_DRIVE_LOCKED:
		case ERROR_LOCKED:
		case ERROR_SHARING_VIOLATION:
			PR_SetError(PR_FILE_IS_LOCKED_ERROR, err);
			break;
		case ERROR_NETNAME_DELETED:
			PR_SetError(PR_CONNECT_RESET_ERROR, err);
			break;
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break; 
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
#ifdef SOCEFAULT
		case SOCEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
#endif
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_transmitfile_error(PRInt32 err)
{
	switch (err) {
		case ERROR_ACCESS_DENIED:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case ERROR_INVALID_HANDLE:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ERROR_INVALID_ADDRESS:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_NOT_READY:
		case ERROR_PATH_BUSY:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_MORE_DATA:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case ERROR_DRIVE_LOCKED:
		case ERROR_LOCKED:
		case ERROR_SHARING_VIOLATION:
			PR_SetError(PR_FILE_IS_LOCKED_ERROR, err);
			break;
		case ERROR_FILENAME_EXCED_RANGE:
			PR_SetError(PR_NAME_TOO_LONG_ERROR, err);
			break;
		case ERROR_TOO_MANY_OPEN_FILES:
			PR_SetError(PR_SYS_DESC_TABLE_FULL_ERROR, err);
			break;
		case ERROR_PATH_NOT_FOUND:
			PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
			break;
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
#ifdef SOCEFAULT
		case SOCEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
#endif
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_write_error(PRInt32 err)
{
	switch (err) {
		case ERROR_ACCESS_DENIED:
		case ERROR_WRITE_PROTECT:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case ERROR_INVALID_HANDLE:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ERROR_INVALID_ADDRESS:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_NOT_READY:
		case ERROR_PATH_BUSY:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case ERROR_DRIVE_LOCKED:
		case ERROR_LOCKED:
		case ERROR_SHARING_VIOLATION:
			PR_SetError(PR_FILE_IS_LOCKED_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_MORE_DATA:
		case ERROR_DISK_FULL:
      case ERROR_HANDLE_DISK_FULL:
      case ENOSPC:
			PR_SetError(PR_NO_DEVICE_SPACE_ERROR, err);
			break;
		case ERROR_NETNAME_DELETED:
			PR_SetError(PR_CONNECT_RESET_ERROR, err);
			break;
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case EMSGSIZE:
		case EINVAL:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		case ENOBUFS:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case ECONNREFUSED:
			PR_SetError(PR_CONNECT_REFUSED_ERROR, err);
			break;
		case EISCONN:
			PR_SetError(PR_IS_CONNECTED_ERROR, err);
			break;
#ifdef SOCEFAULT
		case SOCEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
#endif
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_lseek_error(PRInt32 err)
{
	switch (err) {
		case ERROR_INVALID_HANDLE:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ERROR_SEEK_ON_DEVICE:
			PR_SetError(PR_IO_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_fsync_error(PRInt32 err)
{
	switch (err) {
		case ERROR_ACCESS_DENIED:
		case ERROR_WRITE_PROTECT:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case ERROR_INVALID_HANDLE:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_MORE_DATA:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case ERROR_DISK_FULL:
		case ERROR_HANDLE_DISK_FULL:
			PR_SetError(PR_NO_DEVICE_SPACE_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_close_error(PRInt32 err)
{
	switch (err) {
		case ERROR_INVALID_HANDLE:
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ERROR_NOT_READY:
		case ERROR_PATH_BUSY:
			PR_SetError(PR_IO_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_socket_error(PRInt32 err)
{
	switch (err) {
		case EPROTONOSUPPORT:
			PR_SetError(PR_PROTOCOL_NOT_SUPPORTED_ERROR, err);
			break;
		case EACCES:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_MORE_DATA:
		case ENOBUFS:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_recv_error(PRInt32 err)
{
	switch (err) {
		case EWOULDBLOCK:
			PR_SetError(PR_WOULD_BLOCK_ERROR, err);
			break;
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
#ifdef SOCEFAULT
		case SOCEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
#endif
		case ERROR_NETNAME_DELETED:
			PR_SetError(PR_CONNECT_RESET_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_recvfrom_error(PRInt32 err)
{
	switch (err) {
		case EWOULDBLOCK:
			PR_SetError(PR_WOULD_BLOCK_ERROR, err);
			break;
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
#ifdef SOCEFAULT
		case SOCEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
#endif
		case ERROR_NETNAME_DELETED:
			PR_SetError(PR_CONNECT_RESET_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_send_error(PRInt32 err)
{
	switch (err) {
		case EWOULDBLOCK:
			PR_SetError(PR_WOULD_BLOCK_ERROR, err);
			break;
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case EMSGSIZE:
		case EINVAL:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		case ENOBUFS:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case ECONNREFUSED:
			PR_SetError(PR_CONNECT_REFUSED_ERROR, err);
			break;
		case EISCONN:
			PR_SetError(PR_IS_CONNECTED_ERROR, err);
			break;
#ifdef SOCEFAULT
		case SOCEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
#endif
		case ERROR_NETNAME_DELETED:
			PR_SetError(PR_CONNECT_RESET_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_sendto_error(PRInt32 err)
{
	switch (err) {
		case EWOULDBLOCK:
			PR_SetError(PR_WOULD_BLOCK_ERROR, err);
			break;
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case EMSGSIZE:
		case EINVAL:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		case ENOBUFS:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case ECONNREFUSED:
			PR_SetError(PR_CONNECT_REFUSED_ERROR, err);
			break;
		case EISCONN:
			PR_SetError(PR_IS_CONNECTED_ERROR, err);
			break;
#ifdef SOCEFAULT
		case SOCEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
#endif
		case ERROR_NETNAME_DELETED:
			PR_SetError(PR_CONNECT_RESET_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_accept_error(PRInt32 err)
{
	switch (err) {
		case EWOULDBLOCK:
			PR_SetError(PR_WOULD_BLOCK_ERROR, err);
			break;
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case EOPNOTSUPP:
			PR_SetError(PR_NOT_TCP_SOCKET_ERROR, err);
			break;
#ifdef SOCEFAULT
		case SOCEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
#endif
		case EMFILE:
			PR_SetError(PR_PROC_DESC_TABLE_FULL_ERROR, err);
			break;
		case ENOBUFS:
			PR_SetError(PR_OUT_OF_MEMORY_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_acceptex_error(PRInt32 err)
{
	switch (err) {
		case ERROR_INVALID_HANDLE:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ERROR_INVALID_ADDRESS:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_MORE_DATA:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_connect_error(PRInt32 err)
{
	switch (err) {
       case EWOULDBLOCK:
			PR_SetError(PR_WOULD_BLOCK_ERROR, err);
			break;
        case EINPROGRESS:
			PR_SetError(PR_IN_PROGRESS_ERROR, err);
			break;
		case EALREADY:
		case EINVAL:
			PR_SetError(PR_ALREADY_INITIATED_ERROR, err);
			break;
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case EADDRNOTAVAIL:
			PR_SetError(PR_ADDRESS_NOT_AVAILABLE_ERROR, err);
			break;
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case EAFNOSUPPORT:
			PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, err);
			break;
		case ETIMEDOUT:
			PR_SetError(PR_IO_TIMEOUT_ERROR, err);
			break;
		case ECONNREFUSED:
			PR_SetError(PR_CONNECT_REFUSED_ERROR, err);
			break;
		case ENETUNREACH:
			PR_SetError(PR_NETWORK_UNREACHABLE_ERROR, err);
			break;
		case EADDRINUSE:
			PR_SetError(PR_ADDRESS_IN_USE_ERROR, err);
			break;
      case EISCONN:
         PR_SetError(PR_IS_CONNECTED_ERROR, err);
         break;
#ifdef SOCEFAULT
		case SOCEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
#endif
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_bind_error(PRInt32 err)
{
	switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
#ifdef SOCEFAULT
		case SOCEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
#endif
		case EADDRNOTAVAIL:
			PR_SetError(PR_ADDRESS_NOT_AVAILABLE_ERROR, err);
			break;
		case EADDRINUSE:
			PR_SetError(PR_ADDRESS_IN_USE_ERROR, err);
			break;
		case EACCES:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case EINVAL:
			PR_SetError(PR_SOCKET_ADDRESS_IS_BOUND_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_listen_error(PRInt32 err)
{
	switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case EOPNOTSUPP:
			PR_SetError(PR_NOT_TCP_SOCKET_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_shutdown_error(PRInt32 err)
{
	switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case ENOTCONN:
			PR_SetError(PR_NOT_CONNECTED_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_getsockname_error(PRInt32 err)
{
	switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
#ifdef SOCEFAULT
		case SOCEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
#endif
		case ENOBUFS:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_getpeername_error(PRInt32 err)
{

	switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case ENOTCONN:
			PR_SetError(PR_NOT_CONNECTED_ERROR, err);
			break;
#ifdef SOCEFAULT
		case SOCEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
#endif
		case ENOBUFS:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_getsockopt_error(PRInt32 err)
{
	switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case ENOPROTOOPT:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
#ifdef SOCEFAULT
		case SOCEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
#endif
		case EINVAL:
			PR_SetError(PR_BUFFER_OVERFLOW_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_setsockopt_error(PRInt32 err)
{
	switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case ENOPROTOOPT:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
#ifdef SOCEFAULT
		case SOCEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
#endif
		case EINVAL:
			PR_SetError(PR_BUFFER_OVERFLOW_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_open_error(PRInt32 err)
{
	switch (err) {
		case ERROR_ALREADY_EXISTS:
		case ERROR_FILE_EXISTS:
			PR_SetError(PR_FILE_EXISTS_ERROR, err);
			break;
		case ERROR_FILE_NOT_FOUND:
			PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
			break;
		case ERROR_ACCESS_DENIED:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case ERROR_INVALID_ADDRESS:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_INVALID_NAME:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		case ERROR_NOT_READY:
		case ERROR_OPEN_FAILED:
		case ERROR_PATH_BUSY:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case ERROR_DRIVE_LOCKED:
			PR_SetError(PR_FILE_IS_LOCKED_ERROR, err);
			break;
		case ERROR_FILENAME_EXCED_RANGE:
			PR_SetError(PR_NAME_TOO_LONG_ERROR, err);
			break;
		case ERROR_TOO_MANY_OPEN_FILES:
			PR_SetError(PR_SYS_DESC_TABLE_FULL_ERROR, err);
			break;
		case ERROR_PATH_NOT_FOUND:
			PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_MORE_DATA:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case ERROR_DISK_FULL:
		case ERROR_HANDLE_DISK_FULL:
			PR_SetError(PR_NO_DEVICE_SPACE_ERROR, err);
			break;
		case ERROR_WRITE_PROTECT:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_os2_map_gethostname_error(PRInt32 err)
{
    switch (err) {
#ifdef SOCEFAULT
		case SOCEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
#endif
		case ENETDOWN:
		case EINPROGRESS:
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
    }
}

void _MD_os2_map_select_error(PRInt32 err)
{
    PRErrorCode prerror;

    switch (err) {
        /*
         * OS/2 select() only works on sockets.  So in this
         * context, ENOTSOCK is equivalent to EBADF on Unix.
         */
        case ENOTSOCK:
            prerror = PR_BAD_DESCRIPTOR_ERROR;
            break;
        case EINVAL:
            prerror = PR_INVALID_ARGUMENT_ERROR;
            break;
#ifdef SOCEFAULT
        case SOCEFAULT:
            prerror = PR_ACCESS_FAULT_ERROR;
            break;
#endif
        default:
            prerror = PR_UNKNOWN_ERROR;
    }
    PR_SetError(prerror, err);
}

void _MD_os2_map_lockf_error(PRInt32 err)
{
    switch (err) {
		case ERROR_ACCESS_DENIED:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case ERROR_INVALID_HANDLE:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ERROR_INVALID_ADDRESS:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_DRIVE_LOCKED:
		case ERROR_LOCKED:
		case ERROR_SHARING_VIOLATION:
			PR_SetError(PR_FILE_IS_LOCKED_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_MORE_DATA:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
    }
}
