/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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
#include <errno.h>
#include <windows.h>

void _MD_win32_map_opendir_error(PRInt32 err)
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
		case ERROR_DISK_CORRUPT:
		case ERROR_DISK_OPERATION_FAILED:
		case ERROR_FILE_CORRUPT:
		case ERROR_NOT_READY:
		case ERROR_OPEN_FAILED:
		case ERROR_OPEN_FILES:
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
		case ERROR_OUTOFMEMORY:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_win32_map_closedir_error(PRInt32 err)
{
	switch (err) {
		case ERROR_FILE_INVALID:
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
		case ERROR_FILE_INVALID:
		case ERROR_INVALID_HANDLE:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ERROR_INVALID_ADDRESS:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_DISK_CORRUPT:
		case ERROR_DISK_OPERATION_FAILED:
		case ERROR_FILE_CORRUPT:
		case ERROR_NOT_READY:
		case ERROR_PATH_BUSY:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_OUTOFMEMORY:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_win32_map_delete_error(PRInt32 err)
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
void _MD_win32_map_stat_error(PRInt32 err)
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

void _MD_win32_map_fstat_error(PRInt32 err)
{
	switch (err) {
		case ERROR_ACCESS_DENIED:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case ERROR_FILE_INVALID:
		case ERROR_INVALID_HANDLE:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ERROR_INVALID_ADDRESS:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_DISK_CORRUPT:
		case ERROR_DISK_OPERATION_FAILED:
		case ERROR_FILE_CORRUPT:
		case ERROR_NOT_READY:
		case ERROR_PATH_BUSY:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_OUTOFMEMORY:
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

void _MD_win32_map_rename_error(PRInt32 err)
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
		case ERROR_DISK_CORRUPT:
		case ERROR_DISK_OPERATION_FAILED:
		case ERROR_FILE_CORRUPT:
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
		case ERROR_OUTOFMEMORY:
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
void _MD_win32_map_access_error(PRInt32 err)
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

void _MD_win32_map_mkdir_error(PRInt32 err)
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
		case ERROR_DISK_CORRUPT:
		case ERROR_DISK_OPERATION_FAILED:
		case ERROR_FILE_CORRUPT:
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
		case ERROR_OUTOFMEMORY:
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

void _MD_win32_map_rmdir_error(PRInt32 err)
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
		case ERROR_DISK_CORRUPT:
		case ERROR_DISK_OPERATION_FAILED:
		case ERROR_FILE_CORRUPT:
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
		case ERROR_OUTOFMEMORY:
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

void _MD_win32_map_read_error(PRInt32 err)
{
	switch (err) {
		case ERROR_ACCESS_DENIED:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case ERROR_FILE_INVALID:
		case ERROR_INVALID_HANDLE:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ERROR_INVALID_ADDRESS:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_DISK_CORRUPT:
		case ERROR_DISK_OPERATION_FAILED:
		case ERROR_FILE_CORRUPT:
		case ERROR_NOT_READY:
		case ERROR_PATH_BUSY:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_OUTOFMEMORY:
		case ERROR_INVALID_USER_BUFFER:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_QUOTA:
			PR_SetError(PR_OUT_OF_MEMORY_ERROR, err);
			break;
		case ERROR_DRIVE_LOCKED:
		case ERROR_LOCKED:
		case ERROR_SHARING_VIOLATION:
			PR_SetError(PR_FILE_IS_LOCKED_ERROR, err);
			break;
		case ERROR_NETNAME_DELETED:
			PR_SetError(PR_CONNECT_RESET_ERROR, err);
			break;
		case WSAEBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case WSAENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case WSAEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_win32_map_transmitfile_error(PRInt32 err)
{
	switch (err) {
		case ERROR_ACCESS_DENIED:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case ERROR_FILE_INVALID:
		case ERROR_INVALID_HANDLE:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ERROR_INVALID_ADDRESS:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_DISK_CORRUPT:
		case ERROR_DISK_OPERATION_FAILED:
		case ERROR_FILE_CORRUPT:
		case ERROR_NOT_READY:
		case ERROR_PATH_BUSY:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_OUTOFMEMORY:
		case ERROR_INVALID_USER_BUFFER:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_QUOTA:
			PR_SetError(PR_OUT_OF_MEMORY_ERROR, err);
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
		case WSAEBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case WSAENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case WSAEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_win32_map_write_error(PRInt32 err)
{
	switch (err) {
		case ERROR_ACCESS_DENIED:
		case ERROR_WRITE_PROTECT:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case ERROR_FILE_INVALID:
		case ERROR_INVALID_HANDLE:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ERROR_INVALID_ADDRESS:
		case ERROR_STACK_OVERFLOW:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_DISK_CORRUPT:
		case ERROR_DISK_OPERATION_FAILED:
		case ERROR_FILE_CORRUPT:
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
		case ERROR_OUTOFMEMORY:
		case ERROR_INVALID_USER_BUFFER:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_QUOTA:
			PR_SetError(PR_OUT_OF_MEMORY_ERROR, err);
			break;
		case ERROR_DISK_FULL:
		case ERROR_HANDLE_DISK_FULL:
			PR_SetError(PR_NO_DEVICE_SPACE_ERROR, err);
			break;
		case ERROR_NETNAME_DELETED:
			PR_SetError(PR_CONNECT_RESET_ERROR, err);
			break;
		case WSAEBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case WSAENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case WSAEMSGSIZE:
		case WSAEINVAL:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		case WSAENOBUFS:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case WSAECONNREFUSED:
			PR_SetError(PR_CONNECT_REFUSED_ERROR, err);
			break;
		case WSAEISCONN:
			PR_SetError(PR_IS_CONNECTED_ERROR, err);
			break;
		case WSAEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_win32_map_lseek_error(PRInt32 err)
{
	switch (err) {
		case ERROR_FILE_INVALID:
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

void _MD_win32_map_fsync_error(PRInt32 err)
{
	switch (err) {
		case ERROR_ACCESS_DENIED:
		case ERROR_WRITE_PROTECT:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case ERROR_FILE_INVALID:
		case ERROR_INVALID_HANDLE:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_OUTOFMEMORY:
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

void _MD_win32_map_close_error(PRInt32 err)
{
	switch (err) {
		case ERROR_FILE_INVALID:
		case ERROR_INVALID_HANDLE:
		case WSAEBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ERROR_DISK_OPERATION_FAILED:
		case ERROR_NOT_READY:
		case ERROR_PATH_BUSY:
			PR_SetError(PR_IO_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_win32_map_socket_error(PRInt32 err)
{
	switch (err) {
		case WSAEPROTONOSUPPORT:
			PR_SetError(PR_PROTOCOL_NOT_SUPPORTED_ERROR, err);
			break;
		case WSAEACCES:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_OUTOFMEMORY:
		case WSAENOBUFS:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_win32_map_recv_error(PRInt32 err)
{
	switch (err) {
		case WSAEWOULDBLOCK:
			PR_SetError(PR_WOULD_BLOCK_ERROR, err);
			break;
		case WSAEBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case WSAENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case WSAEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_NETNAME_DELETED:
			PR_SetError(PR_CONNECT_RESET_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_win32_map_recvfrom_error(PRInt32 err)
{
	switch (err) {
		case WSAEWOULDBLOCK:
			PR_SetError(PR_WOULD_BLOCK_ERROR, err);
			break;
		case WSAEBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case WSAENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case WSAEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_NETNAME_DELETED:
			PR_SetError(PR_CONNECT_RESET_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_win32_map_send_error(PRInt32 err)
{
	switch (err) {
		case WSAEWOULDBLOCK:
			PR_SetError(PR_WOULD_BLOCK_ERROR, err);
			break;
		case WSAEBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case WSAENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case WSAEMSGSIZE:
		case WSAEINVAL:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		case WSAENOBUFS:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case WSAECONNREFUSED:
			PR_SetError(PR_CONNECT_REFUSED_ERROR, err);
			break;
		case WSAEISCONN:
			PR_SetError(PR_IS_CONNECTED_ERROR, err);
			break;
		case WSAEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_NETNAME_DELETED:
			PR_SetError(PR_CONNECT_RESET_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_win32_map_sendto_error(PRInt32 err)
{
	switch (err) {
		case WSAEWOULDBLOCK:
			PR_SetError(PR_WOULD_BLOCK_ERROR, err);
			break;
		case WSAEBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case WSAENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case WSAEMSGSIZE:
		case WSAEINVAL:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		case WSAENOBUFS:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case WSAECONNREFUSED:
			PR_SetError(PR_CONNECT_REFUSED_ERROR, err);
			break;
		case WSAEISCONN:
			PR_SetError(PR_IS_CONNECTED_ERROR, err);
			break;
		case WSAEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_NETNAME_DELETED:
			PR_SetError(PR_CONNECT_RESET_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_win32_map_accept_error(PRInt32 err)
{
	switch (err) {
		case WSAEWOULDBLOCK:
			PR_SetError(PR_WOULD_BLOCK_ERROR, err);
			break;
		case WSAEBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case WSAENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case WSAEOPNOTSUPP:
			PR_SetError(PR_NOT_TCP_SOCKET_ERROR, err);
			break;
		case WSAEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case WSAEMFILE:
			PR_SetError(PR_PROC_DESC_TABLE_FULL_ERROR, err);
			break;
		case WSAENOBUFS:
			PR_SetError(PR_OUT_OF_MEMORY_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_win32_map_acceptex_error(PRInt32 err)
{
	switch (err) {
		case ERROR_FILE_INVALID:
		case ERROR_INVALID_HANDLE:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ERROR_INVALID_ADDRESS:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_OUTOFMEMORY:
		case ERROR_INVALID_USER_BUFFER:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_win32_map_connect_error(PRInt32 err)
{
	switch (err) {
		case WSAEWOULDBLOCK:
			PR_SetError(PR_IN_PROGRESS_ERROR, err);
			break;
		case WSAEALREADY:
		case WSAEINVAL:
			PR_SetError(PR_ALREADY_INITIATED_ERROR, err);
			break;
		case WSAEBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case WSAEADDRNOTAVAIL:
			PR_SetError(PR_ADDRESS_NOT_AVAILABLE_ERROR, err);
			break;
		case WSAENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case WSAEAFNOSUPPORT:
			PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, err);
			break;
		case WSAEISCONN:
			PR_SetError(PR_IS_CONNECTED_ERROR, err);
			break;
		case WSAETIMEDOUT:
			PR_SetError(PR_IO_TIMEOUT_ERROR, err);
			break;
		case WSAECONNREFUSED:
			PR_SetError(PR_CONNECT_REFUSED_ERROR, err);
			break;
		case WSAENETUNREACH:
			PR_SetError(PR_NETWORK_UNREACHABLE_ERROR, err);
			break;
		case WSAEADDRINUSE:
			PR_SetError(PR_ADDRESS_IN_USE_ERROR, err);
			break;
		case WSAEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_win32_map_bind_error(PRInt32 err)
{
	switch (err) {
		case WSAEBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case WSAENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case WSAEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case WSAEADDRNOTAVAIL:
			PR_SetError(PR_ADDRESS_NOT_AVAILABLE_ERROR, err);
			break;
		case WSAEADDRINUSE:
			PR_SetError(PR_ADDRESS_IN_USE_ERROR, err);
			break;
		case WSAEACCES:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case WSAEINVAL:
			PR_SetError(PR_SOCKET_ADDRESS_IS_BOUND_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_win32_map_listen_error(PRInt32 err)
{
	switch (err) {
		case WSAEBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case WSAENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case WSAEOPNOTSUPP:
			PR_SetError(PR_NOT_TCP_SOCKET_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_win32_map_shutdown_error(PRInt32 err)
{
	switch (err) {
		case WSAEBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case WSAENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case WSAENOTCONN:
			PR_SetError(PR_NOT_CONNECTED_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_win32_map_getsockname_error(PRInt32 err)
{
	switch (err) {
		case WSAEBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case WSAENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case WSAEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case WSAENOBUFS:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_win32_map_getpeername_error(PRInt32 err)
{

	switch (err) {
		case WSAEBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case WSAENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case WSAENOTCONN:
			PR_SetError(PR_NOT_CONNECTED_ERROR, err);
			break;
		case WSAEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case WSAENOBUFS:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_win32_map_getsockopt_error(PRInt32 err)
{
	switch (err) {
		case WSAEBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case WSAENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case WSAENOPROTOOPT:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		case WSAEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case WSAEINVAL:
			PR_SetError(PR_BUFFER_OVERFLOW_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_win32_map_setsockopt_error(PRInt32 err)
{
	switch (err) {
		case WSAEBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case WSAENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case WSAENOPROTOOPT:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		case WSAEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case WSAEINVAL:
			PR_SetError(PR_BUFFER_OVERFLOW_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_win32_map_open_error(PRInt32 err)
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
		case ERROR_DISK_CORRUPT:
		case ERROR_DISK_OPERATION_FAILED:
		case ERROR_FILE_CORRUPT:
		case ERROR_NOT_READY:
		case ERROR_OPEN_FAILED:
		case ERROR_OPEN_FILES:
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
		case ERROR_OUTOFMEMORY:
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

void _MD_win32_map_gethostname_error(PRInt32 err)
{
    switch (err) {
		case WSAEFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case WSAENETDOWN:
		case WSAEINPROGRESS:
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
    }
}

void _MD_win32_map_select_error(PRInt32 err)
{
    PRErrorCode prerror;

    switch (err) {
        /*
         * Win32 select() only works on sockets.  So in this
         * context, WSAENOTSOCK is equivalent to EBADF on Unix.
         */
        case WSAENOTSOCK:
            prerror = PR_BAD_DESCRIPTOR_ERROR;
            break;
        case WSAEINVAL:
            prerror = PR_INVALID_ARGUMENT_ERROR;
            break;
        case WSAEFAULT:
            prerror = PR_ACCESS_FAULT_ERROR;
            break;
        default:
            prerror = PR_UNKNOWN_ERROR;
    }
    PR_SetError(prerror, err);
}

void _MD_win32_map_lockf_error(PRInt32 err)
{
    switch (err) {
		case ERROR_ACCESS_DENIED:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case ERROR_FILE_INVALID:
		case ERROR_INVALID_HANDLE:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ERROR_INVALID_ADDRESS:
		case ERROR_STACK_OVERFLOW:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ERROR_DRIVE_LOCKED:
		case ERROR_LOCKED:
		case ERROR_SHARING_VIOLATION:
			PR_SetError(PR_FILE_IS_LOCKED_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_OUTOFMEMORY:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case ERROR_NOT_ENOUGH_QUOTA:
			PR_SetError(PR_OUT_OF_MEMORY_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
    }
}
