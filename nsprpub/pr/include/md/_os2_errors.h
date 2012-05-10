/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nspr_os2_errors_h___
#define nspr_os2_errors_h___

#include "md/_os2.h"
#ifndef assert
  #include <assert.h>
#endif  

NSPR_API(void) _MD_os2_map_default_error(PRInt32 err);
#define	_PR_MD_MAP_DEFAULT_ERROR	_MD_os2_map_default_error

NSPR_API(void) _MD_os2_map_opendir_error(PRInt32 err);
#define	_PR_MD_MAP_OPENDIR_ERROR	_MD_os2_map_opendir_error

NSPR_API(void) _MD_os2_map_closedir_error(PRInt32 err);
#define	_PR_MD_MAP_CLOSEDIR_ERROR	_MD_os2_map_closedir_error

NSPR_API(void) _MD_os2_readdir_error(PRInt32 err);
#define	_PR_MD_MAP_READDIR_ERROR	_MD_os2_readdir_error

NSPR_API(void) _MD_os2_map_delete_error(PRInt32 err);
#define	_PR_MD_MAP_DELETE_ERROR	_MD_os2_map_delete_error

NSPR_API(void) _MD_os2_map_stat_error(PRInt32 err);
#define	_PR_MD_MAP_STAT_ERROR	_MD_os2_map_stat_error

NSPR_API(void) _MD_os2_map_fstat_error(PRInt32 err);
#define	_PR_MD_MAP_FSTAT_ERROR	_MD_os2_map_fstat_error

NSPR_API(void) _MD_os2_map_rename_error(PRInt32 err);
#define	_PR_MD_MAP_RENAME_ERROR	_MD_os2_map_rename_error

NSPR_API(void) _MD_os2_map_access_error(PRInt32 err);
#define	_PR_MD_MAP_ACCESS_ERROR	_MD_os2_map_access_error

NSPR_API(void) _MD_os2_map_mkdir_error(PRInt32 err);
#define	_PR_MD_MAP_MKDIR_ERROR	_MD_os2_map_mkdir_error

NSPR_API(void) _MD_os2_map_rmdir_error(PRInt32 err);
#define	_PR_MD_MAP_RMDIR_ERROR	_MD_os2_map_rmdir_error

NSPR_API(void) _MD_os2_map_read_error(PRInt32 err);
#define	_PR_MD_MAP_READ_ERROR	_MD_os2_map_read_error

NSPR_API(void) _MD_os2_map_transmitfile_error(PRInt32 err);
#define	_PR_MD_MAP_TRANSMITFILE_ERROR	_MD_os2_map_transmitfile_error

NSPR_API(void) _MD_os2_map_write_error(PRInt32 err);
#define	_PR_MD_MAP_WRITE_ERROR	_MD_os2_map_write_error

NSPR_API(void) _MD_os2_map_lseek_error(PRInt32 err);
#define	_PR_MD_MAP_LSEEK_ERROR	_MD_os2_map_lseek_error

NSPR_API(void) _MD_os2_map_fsync_error(PRInt32 err);
#define	_PR_MD_MAP_FSYNC_ERROR	_MD_os2_map_fsync_error

NSPR_API(void) _MD_os2_map_close_error(PRInt32 err);
#define	_PR_MD_MAP_CLOSE_ERROR	_MD_os2_map_close_error

NSPR_API(void) _MD_os2_map_socket_error(PRInt32 err);
#define	_PR_MD_MAP_SOCKET_ERROR	_MD_os2_map_socket_error

NSPR_API(void) _MD_os2_map_recv_error(PRInt32 err);
#define	_PR_MD_MAP_RECV_ERROR	_MD_os2_map_recv_error

NSPR_API(void) _MD_os2_map_recvfrom_error(PRInt32 err);
#define	_PR_MD_MAP_RECVFROM_ERROR	_MD_os2_map_recvfrom_error

NSPR_API(void) _MD_os2_map_send_error(PRInt32 err);
#define	_PR_MD_MAP_SEND_ERROR	_MD_os2_map_send_error

NSPR_API(void) _MD_os2_map_sendto_error(PRInt32 err);
#define	_PR_MD_MAP_SENDTO_ERROR	_MD_os2_map_sendto_error

NSPR_API(void) _MD_os2_map_writev_error(int err);
#define	_PR_MD_MAP_WRITEV_ERROR	_MD_os2_map_writev_error

NSPR_API(void) _MD_os2_map_accept_error(PRInt32 err);
#define	_PR_MD_MAP_ACCEPT_ERROR	_MD_os2_map_accept_error

NSPR_API(void) _MD_os2_map_acceptex_error(PRInt32 err);
#define	_PR_MD_MAP_ACCEPTEX_ERROR	_MD_os2_map_acceptex_error

NSPR_API(void) _MD_os2_map_connect_error(PRInt32 err);
#define	_PR_MD_MAP_CONNECT_ERROR	_MD_os2_map_connect_error

NSPR_API(void) _MD_os2_map_bind_error(PRInt32 err);
#define	_PR_MD_MAP_BIND_ERROR	_MD_os2_map_bind_error

NSPR_API(void) _MD_os2_map_listen_error(PRInt32 err);
#define	_PR_MD_MAP_LISTEN_ERROR	_MD_os2_map_listen_error

NSPR_API(void) _MD_os2_map_shutdown_error(PRInt32 err);
#define	_PR_MD_MAP_SHUTDOWN_ERROR	_MD_os2_map_shutdown_error

NSPR_API(void) _MD_os2_map_socketpair_error(int err);
#define	_PR_MD_MAP_SOCKETPAIR_ERROR	_MD_os2_map_socketpair_error

NSPR_API(void) _MD_os2_map_getsockname_error(PRInt32 err);
#define	_PR_MD_MAP_GETSOCKNAME_ERROR	_MD_os2_map_getsockname_error

NSPR_API(void) _MD_os2_map_getpeername_error(PRInt32 err);
#define	_PR_MD_MAP_GETPEERNAME_ERROR	_MD_os2_map_getpeername_error

NSPR_API(void) _MD_os2_map_getsockopt_error(PRInt32 err);
#define	_PR_MD_MAP_GETSOCKOPT_ERROR	_MD_os2_map_getsockopt_error

NSPR_API(void) _MD_os2_map_setsockopt_error(PRInt32 err);
#define	_PR_MD_MAP_SETSOCKOPT_ERROR	_MD_os2_map_setsockopt_error

NSPR_API(void) _MD_os2_map_open_error(PRInt32 err);
#define	_PR_MD_MAP_OPEN_ERROR	_MD_os2_map_open_error

NSPR_API(void) _MD_os2_map_gethostname_error(PRInt32 err);
#define	_PR_MD_MAP_GETHOSTNAME_ERROR	_MD_os2_map_gethostname_error

NSPR_API(void) _MD_os2_map_select_error(PRInt32 err);
#define	_PR_MD_MAP_SELECT_ERROR	_MD_os2_map_select_error

NSPR_API(void) _MD_os2_map_lockf_error(int err);
#define _PR_MD_MAP_LOCKF_ERROR  _MD_os2_map_lockf_error

#endif /* nspr_os2_errors_h___ */
