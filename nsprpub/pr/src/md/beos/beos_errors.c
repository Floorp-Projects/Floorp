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

#include "prtypes.h"
#include "md/_unix_errors.h"
#include "prerror.h"
#include <errno.h>

void _MD_unix_map_opendir_error(int err)
{
	switch (err) {
		case ENOTDIR:
			PR_SetError(PR_NOT_DIRECTORY_ERROR, err);
			break;
		case EACCES:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case EMFILE:
			PR_SetError(PR_PROC_DESC_TABLE_FULL_ERROR, err);
			break;
		case ENFILE:
			PR_SetError(PR_SYS_DESC_TABLE_FULL_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ELOOP:
			PR_SetError(PR_LOOP_ERROR, err);
			break;
		case ENAMETOOLONG:
			PR_SetError(PR_NAME_TOO_LONG_ERROR, err);
			break;
		case ENOENT:
			PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_closedir_error(int err)
{
	switch (err) {
		case EINVAL:
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_readdir_error(int err)
{

	switch (err) {
		case 0:
		case ENOENT:
			PR_SetError(PR_NO_MORE_FILES_ERROR, err);
			break;
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
#ifdef IRIX
#ifdef IRIX5_3
#else
		case EDIRCORRUPTED:
			PR_SetError(PR_DIRECTORY_CORRUPTED_ERROR, err);
			break;
#endif
#endif
#ifdef EOVERFLOW
		case EOVERFLOW:
			PR_SetError(PR_IO_ERROR, err);
			break;
#endif
		case EINVAL:
			PR_SetError(PR_IO_ERROR, err);
			break;
#ifdef EBADMSG
		case EBADMSG:
			PR_SetError(PR_IO_ERROR, err);
			break;
#endif
		case EDEADLK:
			PR_SetError(PR_DEADLOCK_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case EINTR:
			PR_SetError(PR_PENDING_INTERRUPT_ERROR, err);
			break;
		case EIO:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case ENOLCK:
			PR_SetError(PR_FILE_IS_LOCKED_ERROR, err);
			break;
#ifdef ENOLINK
		case ENOLINK:
			PR_SetError(PR_REMOTE_FILE_ERROR, err);
			break;
#endif
		case ENXIO:
			PR_SetError(PR_IO_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_unlink_error(int err)
{
	switch (err) {
		case EACCES:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case EBUSY:
			PR_SetError(PR_FILESYSTEM_MOUNTED_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case EINTR:
			PR_SetError(PR_PENDING_INTERRUPT_ERROR, err);
			break;
		case ELOOP:
			PR_SetError(PR_LOOP_ERROR, err);
			break;
		case ENAMETOOLONG:
			PR_SetError(PR_NAME_TOO_LONG_ERROR, err);
			break;
		case ENOENT:
			PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
			break;
		case ENOTDIR:
			PR_SetError(PR_NOT_DIRECTORY_ERROR, err);
			break;
		case EPERM:
			PR_SetError(PR_IS_DIRECTORY_ERROR, err);
			break;
		case EROFS:
			PR_SetError(PR_READ_ONLY_FILESYSTEM_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_stat_error(int err)
{
	switch (err) {
		case EACCES:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case EINTR:
			PR_SetError(PR_PENDING_INTERRUPT_ERROR, err);
			break;
		case ETIMEDOUT:
			PR_SetError(PR_REMOTE_FILE_ERROR, err);
			break;
		case ELOOP:
			PR_SetError(PR_LOOP_ERROR, err);
			break;
		case ENAMETOOLONG:
			PR_SetError(PR_NAME_TOO_LONG_ERROR, err);
			break;
		case ENOENT:
			PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
			break;
		case ENOTDIR:
			PR_SetError(PR_NOT_DIRECTORY_ERROR, err);
			break;
#ifdef EOVERFLOW
		case EOVERFLOW:
			PR_SetError(PR_BUFFER_OVERFLOW_ERROR, err);
			break;
#endif
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_fstat_error(int err)
{
	switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case EINTR:
			PR_SetError(PR_PENDING_INTERRUPT_ERROR, err);
			break;
		case ETIMEDOUT:
#ifdef ENOLINK
		case ENOLINK:
#endif
			PR_SetError(PR_REMOTE_FILE_ERROR, err);
			break;
#ifdef EOVERFLOW
		case EOVERFLOW:
			PR_SetError(PR_BUFFER_OVERFLOW_ERROR, err);
			break;
#endif
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_rename_error(int err)
{
	switch (err) {
		case EACCES:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case EBUSY:
			PR_SetError(PR_FILESYSTEM_MOUNTED_ERROR, err);
			break;
#ifdef EDQUOT
		case EDQUOT:
			PR_SetError(PR_NO_DEVICE_SPACE_ERROR, err);
			break;
#endif
		case EEXIST:
			PR_SetError(PR_DIRECTORY_NOT_EMPTY_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case EINTR:
			PR_SetError(PR_PENDING_INTERRUPT_ERROR, err);
			break;
		case EINVAL:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		case EIO:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case EISDIR:
			PR_SetError(PR_IS_DIRECTORY_ERROR, err);
			break;
		case ELOOP:
			PR_SetError(PR_LOOP_ERROR, err);
			break;
		case ENAMETOOLONG:
			PR_SetError(PR_NAME_TOO_LONG_ERROR, err);
			break;
		case ENOENT:
			PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
			break;
		case ENOSPC:
			PR_SetError(PR_NO_DEVICE_SPACE_ERROR, err);
			break;
		case ENOTDIR:
			PR_SetError(PR_NOT_DIRECTORY_ERROR, err);
			break;
		case EROFS:
			PR_SetError(PR_READ_ONLY_FILESYSTEM_ERROR, err);
			break;
		case EXDEV:
			PR_SetError(PR_NOT_SAME_DEVICE_ERROR, err);
			break;
		case EMLINK:
			PR_SetError(PR_MAX_DIRECTORY_ENTRIES_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_access_error(int err)
{
	switch (err) {
		case EACCES:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case EINTR:
			PR_SetError(PR_PENDING_INTERRUPT_ERROR, err);
			break;
		case EINVAL:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		case ELOOP:
			PR_SetError(PR_LOOP_ERROR, err);
			break;
		case ETIMEDOUT:
			PR_SetError(PR_REMOTE_FILE_ERROR, err);
			break;
		case ENAMETOOLONG:
			PR_SetError(PR_NAME_TOO_LONG_ERROR, err);
			break;
		case ENOENT:
			PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
			break;
		case ENOTDIR:
			PR_SetError(PR_NOT_DIRECTORY_ERROR, err);
			break;
		case EROFS:
			PR_SetError(PR_READ_ONLY_FILESYSTEM_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_mkdir_error(int err)
{
	switch (err) {
		case ENOTDIR:
			PR_SetError(PR_NOT_DIRECTORY_ERROR, err);
			break;
		case ENOENT:
			PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
			break;
		case ENAMETOOLONG:
			PR_SetError(PR_NAME_TOO_LONG_ERROR, err);
			break;
		case EACCES:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case EEXIST:
			PR_SetError(PR_FILE_EXISTS_ERROR, err);
			break;
		case EROFS:
			PR_SetError(PR_READ_ONLY_FILESYSTEM_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ELOOP:
			PR_SetError(PR_LOOP_ERROR, err);
			break;
		case EMLINK:
			PR_SetError(PR_MAX_DIRECTORY_ENTRIES_ERROR, err);
			break;
		case ENOSPC:
			PR_SetError(PR_NO_DEVICE_SPACE_ERROR, err);
			break;
#ifdef EDQUOT
		case EDQUOT:
			PR_SetError(PR_NO_DEVICE_SPACE_ERROR, err);
			break;
#endif
		case EIO:
			PR_SetError(PR_IO_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_rmdir_error(int err)
{

	switch (err) {
		case EACCES:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case EBUSY:
			PR_SetError(PR_FILESYSTEM_MOUNTED_ERROR, err);
			break;
		case EEXIST:
			PR_SetError(PR_DIRECTORY_NOT_EMPTY_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case EINVAL:
			PR_SetError(PR_DIRECTORY_NOT_EMPTY_ERROR, err);
			break;
		case EIO:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case ELOOP:
			PR_SetError(PR_LOOP_ERROR, err);
			break;
		case ETIMEDOUT:
			PR_SetError(PR_REMOTE_FILE_ERROR, err);
			break;
		case ENAMETOOLONG:
			PR_SetError(PR_NAME_TOO_LONG_ERROR, err);
			break;
		case ENOENT:
			PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
			break;
		case ENOTDIR:
			PR_SetError(PR_NOT_DIRECTORY_ERROR, err);
			break;
		case EROFS:
			PR_SetError(PR_READ_ONLY_FILESYSTEM_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_read_error(int err)
{
	switch (err) {
		case EACCES:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case EAGAIN:
#if EWOULDBLOCK != EAGAIN
		case EWOULDBLOCK:
#endif
			PR_SetError(PR_WOULD_BLOCK_ERROR, err);
			break;
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
#ifdef EBADMSG
		case EBADMSG:
			PR_SetError(PR_IO_ERROR, err);
			break;
#endif
		case EDEADLK:
			PR_SetError(PR_DEADLOCK_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case EINTR:
			PR_SetError(PR_PENDING_INTERRUPT_ERROR, err);
			break;
		case EINVAL:
			PR_SetError(PR_INVALID_METHOD_ERROR, err);
			break;
		case EIO:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case ENOLCK:
			PR_SetError(PR_FILE_IS_LOCKED_ERROR, err);
			break;
		case ENXIO:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		case EISDIR:
			PR_SetError(PR_IS_DIRECTORY_ERROR, err);
			break;
		case ECONNRESET:
		case EPIPE:
			PR_SetError(PR_CONNECT_RESET_ERROR, err);
			break;
#ifdef ENOLINK
		case ENOLINK:
			PR_SetError(PR_REMOTE_FILE_ERROR, err);
			break;
#endif
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_write_error(int err)
{
	switch (err) {
		case EAGAIN:
#if EWOULDBLOCK != EAGAIN
		case EWOULDBLOCK:
#endif
			PR_SetError(PR_WOULD_BLOCK_ERROR, err);
			break;
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case EDEADLK:
			PR_SetError(PR_DEADLOCK_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case EFBIG:
			PR_SetError(PR_FILE_TOO_BIG_ERROR, err);
			break;
		case EINTR:
			PR_SetError(PR_PENDING_INTERRUPT_ERROR, err);
			break;
		case EINVAL:
			PR_SetError(PR_INVALID_METHOD_ERROR, err);
			break;
		case EIO:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case ENOLCK:
			PR_SetError(PR_FILE_IS_LOCKED_ERROR, err);
			break;
#ifdef ENOSR
		case ENOSR:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
#endif
		case ENOSPC:
			PR_SetError(PR_NO_DEVICE_SPACE_ERROR, err);
			break;
		case ENXIO:
			PR_SetError(PR_INVALID_METHOD_ERROR, err);
			break;
		case ERANGE:
			PR_SetError(PR_INVALID_METHOD_ERROR, err);
			break;
		case ETIMEDOUT:
			PR_SetError(PR_REMOTE_FILE_ERROR, err);
			break;
		case ECONNRESET:
		case EPIPE:
			PR_SetError(PR_CONNECT_RESET_ERROR, err);
			break;
#ifdef EDQUOT
		case EDQUOT:
			PR_SetError(PR_NO_DEVICE_SPACE_ERROR, err);
			break;
#endif
#ifdef ENOLINK
		case ENOLINK:
			PR_SetError(PR_REMOTE_FILE_ERROR, err);
			break;
#endif
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_lseek_error(int err)
{
	switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case ESPIPE:
			PR_SetError(PR_INVALID_METHOD_ERROR, err);
			break;
		case EINVAL:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_fsync_error(int err)
{
	switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
#ifdef ENOLINK
		case ENOLINK:
#endif
		case ETIMEDOUT:
			PR_SetError(PR_REMOTE_FILE_ERROR, err);
			break;
		case EINTR:
			PR_SetError(PR_PENDING_INTERRUPT_ERROR, err);
			break;
		case EIO:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case EINVAL:
			PR_SetError(PR_INVALID_METHOD_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_close_error(int err)
{
	switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case EINTR:
			PR_SetError(PR_PENDING_INTERRUPT_ERROR, err);
			break;
#ifdef ENOLINK
		case ENOLINK:
#endif
		case ETIMEDOUT:
			PR_SetError(PR_REMOTE_FILE_ERROR, err);
			break;
		case EIO:
			PR_SetError(PR_IO_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_socket_error(int err)
{
	switch (err) {
		case EPROTONOSUPPORT:
			PR_SetError(PR_PROTOCOL_NOT_SUPPORTED_ERROR, err);
			break;
		case EMFILE:
			PR_SetError(PR_PROC_DESC_TABLE_FULL_ERROR, err);
			break;
		case ENFILE:
			PR_SetError(PR_SYS_DESC_TABLE_FULL_ERROR, err);
			break;
		case EACCES:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
#if !defined(SCO)
		case ENOBUFS:
#endif /* !defined(SCO) */
		case ENOMEM:
#ifdef ENOSR
		case ENOSR:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
#endif
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_socketavailable_error(int err)
{
	PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
}

void _MD_unix_map_recv_error(int err)
{
	switch (err) {
		case EAGAIN:
#if EWOULDBLOCK != EAGAIN
		case EWOULDBLOCK:
#endif
			PR_SetError(PR_WOULD_BLOCK_ERROR, err);
			break;
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
#if !defined(BEOS)
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
#endif
		case EINTR:
			PR_SetError(PR_PENDING_INTERRUPT_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ENOMEM:
			PR_SetError(PR_OUT_OF_MEMORY_ERROR, err);
			break;
		case ECONNRESET:
		case EPIPE:
			PR_SetError(PR_CONNECT_RESET_ERROR, err);
			break;
#ifdef ENOSR
		case ENOSR:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
#endif
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_recvfrom_error(int err)
{
	switch (err) {
		case EAGAIN:
#if EWOULDBLOCK != EAGAIN
		case EWOULDBLOCK:
#endif
			PR_SetError(PR_WOULD_BLOCK_ERROR, err);
			break;
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
#if !defined(BEOS)
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
#endif
		case EINTR:
			PR_SetError(PR_PENDING_INTERRUPT_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ENOMEM:
			PR_SetError(PR_OUT_OF_MEMORY_ERROR, err);
			break;
#ifdef ENOSR
		case ENOSR:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
#endif
		case ECONNRESET:
			PR_SetError(PR_CONNECT_RESET_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_send_error(int err)
{
	switch (err) {
		case EAGAIN:
#if EWOULDBLOCK != EAGAIN
		case EWOULDBLOCK:
#endif
			PR_SetError(PR_WOULD_BLOCK_ERROR, err);
			break;
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
#if !defined(BEOS)
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
#endif
#if !defined(BEOS)
		case EMSGSIZE:
#endif
		case EINVAL:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
#if !defined(SCO)
		case ENOBUFS:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
#endif /* !defined(SCO) */
		case ECONNREFUSED:
			PR_SetError(PR_CONNECT_REFUSED_ERROR, err);
			break;
		case EISCONN:
			PR_SetError(PR_IS_CONNECTED_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case EINTR:
			PR_SetError(PR_PENDING_INTERRUPT_ERROR, err);
			break;
		case ENOMEM:
			PR_SetError(PR_OUT_OF_MEMORY_ERROR, err);
			break;
#ifdef ENOSR
		case ENOSR:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
#endif
		case ECONNRESET:
		case EPIPE:
			PR_SetError(PR_CONNECT_RESET_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_sendto_error(int err)
{
	switch (err) {
		case EAGAIN:
#if EWOULDBLOCK != EAGAIN
		case EWOULDBLOCK:
#endif
			PR_SetError(PR_WOULD_BLOCK_ERROR, err);
			break;
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
#if !defined(BEOS)
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
#endif
#if !defined(BEOS)
		case EMSGSIZE:
#endif
		case EINVAL:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
#if !defined(SCO)
		case ENOBUFS:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
#endif /* !defined(SCO) */
		case ECONNREFUSED:
			PR_SetError(PR_CONNECT_REFUSED_ERROR, err);
			break;
		case EISCONN:
			PR_SetError(PR_IS_CONNECTED_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case EINTR:
			PR_SetError(PR_PENDING_INTERRUPT_ERROR, err);
			break;
		case ENOMEM:
			PR_SetError(PR_OUT_OF_MEMORY_ERROR, err);
			break;
#ifdef ENOSR
		case ENOSR:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
#endif
		case ECONNRESET:
		case EPIPE:
			PR_SetError(PR_CONNECT_RESET_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_writev_error(int err)
{
	switch (err) {
		case EAGAIN:
#if EWOULDBLOCK != EAGAIN
		case EWOULDBLOCK:
#endif
			PR_SetError(PR_WOULD_BLOCK_ERROR, err);
			break;
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case EINTR:
			PR_SetError(PR_PENDING_INTERRUPT_ERROR, err);
			break;
#ifdef ENOSR
		case ENOSR:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
#endif
		case EINVAL:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		case ECONNRESET:
		case EPIPE:
			PR_SetError(PR_CONNECT_RESET_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_accept_error(int err)
{
	switch (err) {
		case EAGAIN:
#if EWOULDBLOCK != EAGAIN
		case EWOULDBLOCK:
#endif
			PR_SetError(PR_WOULD_BLOCK_ERROR, err);
			break;
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
#if !defined(BEOS)
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
#endif
#if !defined(BEOS)
		case EOPNOTSUPP:
#endif
		case ENODEV:
			PR_SetError(PR_NOT_TCP_SOCKET_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case EMFILE:
			PR_SetError(PR_PROC_DESC_TABLE_FULL_ERROR, err);
			break;
		case ENFILE:
			PR_SetError(PR_SYS_DESC_TABLE_FULL_ERROR, err);
			break;
		case EINTR:
			PR_SetError(PR_PENDING_INTERRUPT_ERROR, err);
			break;
		case ENOMEM:
			PR_SetError(PR_OUT_OF_MEMORY_ERROR, err);
			break;
#ifdef ENOSR
		case ENOSR:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
#endif
#ifdef EPROTO
		case EPROTO:
			PR_SetError(PR_IO_ERROR, err);
			break;
#endif
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_connect_error(int err)
{
	switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case EADDRNOTAVAIL:
			PR_SetError(PR_ADDRESS_NOT_AVAILABLE_ERROR, err);
			break;
		case EINPROGRESS:
			PR_SetError(PR_IN_PROGRESS_ERROR, err);
			break;
		case EALREADY:
			PR_SetError(PR_ALREADY_INITIATED_ERROR, err);
			break;
#if !defined(BEOS)
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
#endif
		case EAFNOSUPPORT:
			PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, err);
			break;
		case EISCONN:
			PR_SetError(PR_IS_CONNECTED_ERROR, err);
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
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		/*
		 * UNIX domain sockets are not supported in NSPR
		 */
		case EACCES:
			PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, err);
			break;
		case EINTR:
			PR_SetError(PR_PENDING_INTERRUPT_ERROR, err);
			break;
		case EINVAL:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		case EIO:
#if defined(UNIXWARE) || defined(SNI) || defined(NEC)
			/*
			 * On some platforms, if we connect to a port on
			 * the local host (the loopback address) that no
			 * process is listening on, we get EIO instead
			 * of ECONNREFUSED.
			 */
			PR_SetError(PR_CONNECT_REFUSED_ERROR, err);
#else
			PR_SetError(PR_IO_ERROR, err);
#endif
			break;
		case ELOOP:
			PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, err);
			break;
		case ENOENT:
			PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, err);
			break;
#ifdef ENOSR
		case ENOSR:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
#endif
		case ENXIO:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case EPROTOTYPE:
			PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_bind_error(int err)
{
	switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
#if !defined(BEOS)
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
#endif
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
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
#ifdef ENOSR
		case ENOSR:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
#endif
		/*
		 * UNIX domain sockets are not supported in NSPR
		 */
		case EIO:
		case EISDIR:
		case ELOOP:
		case ENOENT:
		case ENOTDIR:
		case EROFS:
			PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_listen_error(int err)
{
	switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
#if !defined(BEOS)
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
#endif
#if !defined(BEOS)
		case EOPNOTSUPP:
			PR_SetError(PR_NOT_TCP_SOCKET_ERROR, err);
			break;
#endif
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_shutdown_error(int err)
{
	switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
#if !defined(BEOS)
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
#endif
		case ENOTCONN:
			PR_SetError(PR_NOT_CONNECTED_ERROR, err);
			break;
		case ENOMEM:
			PR_SetError(PR_OUT_OF_MEMORY_ERROR, err);
			break;
#ifdef ENOSR
		case ENOSR:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
#endif
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_socketpair_error(int err)
{
	switch (err) {
		case EMFILE:
			PR_SetError(PR_PROC_DESC_TABLE_FULL_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case ENOMEM:
#ifdef ENOSR
		case ENOSR:
#endif
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case EAFNOSUPPORT:
		case EPROTONOSUPPORT:
#if !defined(BEOS)
		case EOPNOTSUPP:
#endif
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_getsockname_error(int err)
{
	switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
#if !defined(BEOS)
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
#endif
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
#if !defined(SCO)
		case ENOBUFS:
#endif /* !defined(SCO) */
		case ENOMEM:
#ifdef ENOSR
		case ENOSR:
#endif
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_getpeername_error(int err)
{

	switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
#if !defined(BEOS)
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
#endif
		case ENOTCONN:
			PR_SetError(PR_NOT_CONNECTED_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
#if !defined(SCO)
		case ENOBUFS:
#endif /* !defined(SCO) */
		case ENOMEM:
#ifdef ENOSR
		case ENOSR:
#endif
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_getsockopt_error(int err)
{
	switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
#if !defined(BEOS)
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
#endif
		case ENOPROTOOPT:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case EINVAL:
			PR_SetError(PR_BUFFER_OVERFLOW_ERROR, err);
			break;
		case ENOMEM:
#ifdef ENOSR
		case ENOSR:
#endif
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_setsockopt_error(int err)
{
	switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
#if !defined(BEOS)
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
#endif
		case ENOPROTOOPT:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case EINVAL:
			PR_SetError(PR_BUFFER_OVERFLOW_ERROR, err);
			break;
		case ENOMEM:
#ifdef ENOSR
		case ENOSR:
#endif
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_open_error(int err)
{
	switch (err) {
		case EACCES:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case EAGAIN:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case EBUSY:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case EEXIST:
			PR_SetError(PR_FILE_EXISTS_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		case EINTR:
			PR_SetError(PR_PENDING_INTERRUPT_ERROR, err);
			break;
		case EINVAL:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		case EIO:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case EISDIR:
			PR_SetError(PR_IS_DIRECTORY_ERROR, err);
			break;
		case ELOOP:
			PR_SetError(PR_LOOP_ERROR, err);
			break;
		case EMFILE:
			PR_SetError(PR_PROC_DESC_TABLE_FULL_ERROR, err);
			break;
		case ENAMETOOLONG:
			PR_SetError(PR_NAME_TOO_LONG_ERROR, err);
			break;
		case ENFILE:
			PR_SetError(PR_SYS_DESC_TABLE_FULL_ERROR, err);
			break;
		case ENODEV:
		case ENOENT:
		case ENXIO:
			PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
			break;
		case ENOMEM:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case ENOSPC:
			PR_SetError(PR_NO_DEVICE_SPACE_ERROR, err);
			break;
#ifdef ENOSR
		case ENOSR:
#endif
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case ENOTDIR:
			PR_SetError(PR_NOT_DIRECTORY_ERROR, err);
			break;
		case EPERM:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case ETIMEDOUT:
			PR_SetError(PR_REMOTE_FILE_ERROR, err);
			break;
		case EROFS:
			PR_SetError(PR_READ_ONLY_FILESYSTEM_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_mmap_error(int err)
{

	switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case EAGAIN:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		case EACCES:
			PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, err);
			break;
		case ENOMEM:
			PR_SetError(PR_OUT_OF_MEMORY_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

void _MD_unix_map_gethostname_error(int err)
{
    switch (err) {
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
    }
}

void _MD_unix_map_select_error(int err)
{
    switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case EINTR:
			PR_SetError(PR_PENDING_INTERRUPT_ERROR, err);
			break;
		case EINVAL:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
    }
}

void _MD_unix_map_poll_error(int err)
{
    PRErrorCode prerror;
    switch (err) {
        case EAGAIN:
            prerror = PR_INSUFFICIENT_RESOURCES_ERROR;
            break;
        case EINVAL:
            prerror = PR_INVALID_ARGUMENT_ERROR;
            break;
        case EFAULT:
            prerror = PR_ACCESS_FAULT_ERROR;
            break;
        default:
            prerror = PR_UNKNOWN_ERROR;
            break;
    }
    PR_SetError(prerror, err);
}

void _MD_unix_map_flock_error(int err)
{
    switch (err) {
		case EBADF:
		case EINVAL:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case EWOULDBLOCK:
			PR_SetError(PR_FILE_IS_LOCKED_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
    }
}

void _MD_unix_map_lockf_error(int err)
{
    switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case EACCES:
			PR_SetError(PR_FILE_IS_LOCKED_ERROR, err);
			break;
		case EDEADLK:
			PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
    }
}

#ifdef HPUX11
void _MD_hpux_map_sendfile_error(int oserror)
{
    PRErrorCode prerror;

    switch (oserror) {
        case ENOTSOCK:
            prerror = PR_NOT_SOCKET_ERROR;
            break;
        case EFAULT:
            prerror = PR_ACCESS_FAULT_ERROR;
            break;
        case ENOBUFS:
            prerror = PR_INSUFFICIENT_RESOURCES_ERROR;
            break;
        case EINVAL:
            prerror = PR_INVALID_ARGUMENT_ERROR;
            break;
        case ENOTCONN:
            prerror = PR_NOT_CONNECTED_ERROR;
            break;
        case EPIPE:
            prerror = PR_CONNECT_RESET_ERROR;
            break;
        case ENOMEM:
            prerror = PR_OUT_OF_MEMORY_ERROR;
            break;
        case EOPNOTSUPP:
            prerror = PR_NOT_TCP_SOCKET_ERROR;
            break;
        default:
            prerror = PR_UNKNOWN_ERROR;
    }
    PR_SetError(prerror, oserror); 
}
#endif /* HPUX11 */
