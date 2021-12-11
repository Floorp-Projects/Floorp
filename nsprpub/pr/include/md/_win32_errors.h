/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nspr_win32_errors_h___
#define nspr_win32_errors_h___

#include <windows.h>
#include <winsock.h>
#include <errno.h>


extern void _MD_win32_map_default_error(PRInt32 err);
#define _PR_MD_MAP_DEFAULT_ERROR    _MD_win32_map_default_error

extern void _MD_win32_map_opendir_error(PRInt32 err);
#define _PR_MD_MAP_OPENDIR_ERROR    _MD_win32_map_opendir_error

extern void _MD_win32_map_closedir_error(PRInt32 err);
#define _PR_MD_MAP_CLOSEDIR_ERROR   _MD_win32_map_closedir_error

extern void _MD_unix_readdir_error(PRInt32 err);
#define _PR_MD_MAP_READDIR_ERROR    _MD_unix_readdir_error

extern void _MD_win32_map_delete_error(PRInt32 err);
#define _PR_MD_MAP_DELETE_ERROR _MD_win32_map_delete_error

extern void _MD_win32_map_stat_error(PRInt32 err);
#define _PR_MD_MAP_STAT_ERROR   _MD_win32_map_stat_error

extern void _MD_win32_map_fstat_error(PRInt32 err);
#define _PR_MD_MAP_FSTAT_ERROR  _MD_win32_map_fstat_error

extern void _MD_win32_map_rename_error(PRInt32 err);
#define _PR_MD_MAP_RENAME_ERROR _MD_win32_map_rename_error

extern void _MD_win32_map_access_error(PRInt32 err);
#define _PR_MD_MAP_ACCESS_ERROR _MD_win32_map_access_error

extern void _MD_win32_map_mkdir_error(PRInt32 err);
#define _PR_MD_MAP_MKDIR_ERROR  _MD_win32_map_mkdir_error

extern void _MD_win32_map_rmdir_error(PRInt32 err);
#define _PR_MD_MAP_RMDIR_ERROR  _MD_win32_map_rmdir_error

extern void _MD_win32_map_read_error(PRInt32 err);
#define _PR_MD_MAP_READ_ERROR   _MD_win32_map_read_error

extern void _MD_win32_map_transmitfile_error(PRInt32 err);
#define _PR_MD_MAP_TRANSMITFILE_ERROR   _MD_win32_map_transmitfile_error

extern void _MD_win32_map_write_error(PRInt32 err);
#define _PR_MD_MAP_WRITE_ERROR  _MD_win32_map_write_error

extern void _MD_win32_map_lseek_error(PRInt32 err);
#define _PR_MD_MAP_LSEEK_ERROR  _MD_win32_map_lseek_error

extern void _MD_win32_map_fsync_error(PRInt32 err);
#define _PR_MD_MAP_FSYNC_ERROR  _MD_win32_map_fsync_error

extern void _MD_win32_map_close_error(PRInt32 err);
#define _PR_MD_MAP_CLOSE_ERROR  _MD_win32_map_close_error

extern void _MD_win32_map_socket_error(PRInt32 err);
#define _PR_MD_MAP_SOCKET_ERROR _MD_win32_map_socket_error

extern void _MD_win32_map_recv_error(PRInt32 err);
#define _PR_MD_MAP_RECV_ERROR   _MD_win32_map_recv_error

extern void _MD_win32_map_recvfrom_error(PRInt32 err);
#define _PR_MD_MAP_RECVFROM_ERROR   _MD_win32_map_recvfrom_error

extern void _MD_win32_map_send_error(PRInt32 err);
#define _PR_MD_MAP_SEND_ERROR   _MD_win32_map_send_error

extern void _MD_win32_map_sendto_error(PRInt32 err);
#define _PR_MD_MAP_SENDTO_ERROR _MD_win32_map_sendto_error

extern void _MD_win32_map_accept_error(PRInt32 err);
#define _PR_MD_MAP_ACCEPT_ERROR _MD_win32_map_accept_error

extern void _MD_win32_map_acceptex_error(PRInt32 err);
#define _PR_MD_MAP_ACCEPTEX_ERROR   _MD_win32_map_acceptex_error

extern void _MD_win32_map_connect_error(PRInt32 err);
#define _PR_MD_MAP_CONNECT_ERROR    _MD_win32_map_connect_error

extern void _MD_win32_map_bind_error(PRInt32 err);
#define _PR_MD_MAP_BIND_ERROR   _MD_win32_map_bind_error

extern void _MD_win32_map_listen_error(PRInt32 err);
#define _PR_MD_MAP_LISTEN_ERROR _MD_win32_map_listen_error

extern void _MD_win32_map_shutdown_error(PRInt32 err);
#define _PR_MD_MAP_SHUTDOWN_ERROR   _MD_win32_map_shutdown_error

extern void _MD_win32_map_getsockname_error(PRInt32 err);
#define _PR_MD_MAP_GETSOCKNAME_ERROR    _MD_win32_map_getsockname_error

extern void _MD_win32_map_getpeername_error(PRInt32 err);
#define _PR_MD_MAP_GETPEERNAME_ERROR    _MD_win32_map_getpeername_error

extern void _MD_win32_map_getsockopt_error(PRInt32 err);
#define _PR_MD_MAP_GETSOCKOPT_ERROR _MD_win32_map_getsockopt_error

extern void _MD_win32_map_setsockopt_error(PRInt32 err);
#define _PR_MD_MAP_SETSOCKOPT_ERROR _MD_win32_map_setsockopt_error

extern void _MD_win32_map_open_error(PRInt32 err);
#define _PR_MD_MAP_OPEN_ERROR   _MD_win32_map_open_error

extern void _MD_win32_map_gethostname_error(PRInt32 err);
#define _PR_MD_MAP_GETHOSTNAME_ERROR    _MD_win32_map_gethostname_error

extern void _MD_win32_map_select_error(PRInt32 err);
#define _PR_MD_MAP_SELECT_ERROR _MD_win32_map_select_error

extern void _MD_win32_map_lockf_error(int err);
#define _PR_MD_MAP_LOCKF_ERROR  _MD_win32_map_lockf_error

#endif /* nspr_win32_errors_h___ */
