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

#ifndef nspr_os2_errors_h___
#define nspr_os2_errors_h___

#include "md/_os2.h"
#ifndef assert
  #include <assert.h>
#endif  

PR_EXTERN(void) _MD_os2_map_opendir_error(PRInt32 err);
#define	_PR_MD_MAP_OPENDIR_ERROR	_MD_os2_map_opendir_error

PR_EXTERN(void) _MD_os2_map_closedir_error(PRInt32 err);
#define	_PR_MD_MAP_CLOSEDIR_ERROR	_MD_os2_map_closedir_error

PR_EXTERN(void) _MD_unix_readdir_error(PRInt32 err);
#define	_PR_MD_MAP_READDIR_ERROR	_MD_unix_readdir_error

PR_EXTERN(void) _MD_os2_map_delete_error(PRInt32 err);
#define	_PR_MD_MAP_DELETE_ERROR	_MD_os2_map_delete_error

PR_EXTERN(void) _MD_os2_map_stat_error(PRInt32 err);
#define	_PR_MD_MAP_STAT_ERROR	_MD_os2_map_stat_error

PR_EXTERN(void) _MD_os2_map_fstat_error(PRInt32 err);
#define	_PR_MD_MAP_FSTAT_ERROR	_MD_os2_map_fstat_error

PR_EXTERN(void) _MD_os2_map_rename_error(PRInt32 err);
#define	_PR_MD_MAP_RENAME_ERROR	_MD_os2_map_rename_error

PR_EXTERN(void) _MD_os2_map_access_error(PRInt32 err);
#define	_PR_MD_MAP_ACCESS_ERROR	_MD_os2_map_access_error

PR_EXTERN(void) _MD_os2_map_mkdir_error(PRInt32 err);
#define	_PR_MD_MAP_MKDIR_ERROR	_MD_os2_map_mkdir_error

PR_EXTERN(void) _MD_os2_map_rmdir_error(PRInt32 err);
#define	_PR_MD_MAP_RMDIR_ERROR	_MD_os2_map_rmdir_error

PR_EXTERN(void) _MD_os2_map_read_error(PRInt32 err);
#define	_PR_MD_MAP_READ_ERROR	_MD_os2_map_read_error

PR_EXTERN(void) _MD_os2_map_transmitfile_error(PRInt32 err);
#define	_PR_MD_MAP_TRANSMITFILE_ERROR	_MD_os2_map_transmitfile_error

PR_EXTERN(void) _MD_os2_map_write_error(PRInt32 err);
#define	_PR_MD_MAP_WRITE_ERROR	_MD_os2_map_write_error

PR_EXTERN(void) _MD_os2_map_lseek_error(PRInt32 err);
#define	_PR_MD_MAP_LSEEK_ERROR	_MD_os2_map_lseek_error

PR_EXTERN(void) _MD_os2_map_fsync_error(PRInt32 err);
#define	_PR_MD_MAP_FSYNC_ERROR	_MD_os2_map_fsync_error

PR_EXTERN(void) _MD_os2_map_close_error(PRInt32 err);
#define	_PR_MD_MAP_CLOSE_ERROR	_MD_os2_map_close_error

PR_EXTERN(void) _MD_os2_map_socket_error(PRInt32 err);
#define	_PR_MD_MAP_SOCKET_ERROR	_MD_os2_map_socket_error

PR_EXTERN(void) _MD_os2_map_recv_error(PRInt32 err);
#define	_PR_MD_MAP_RECV_ERROR	_MD_os2_map_recv_error

PR_EXTERN(void) _MD_os2_map_recvfrom_error(PRInt32 err);
#define	_PR_MD_MAP_RECVFROM_ERROR	_MD_os2_map_recvfrom_error

PR_EXTERN(void) _MD_os2_map_send_error(PRInt32 err);
#define	_PR_MD_MAP_SEND_ERROR	_MD_os2_map_send_error

PR_EXTERN(void) _MD_os2_map_sendto_error(PRInt32 err);
#define	_PR_MD_MAP_SENDTO_ERROR	_MD_os2_map_sendto_error

PR_EXTERN(void) _MD_os2_map_accept_error(PRInt32 err);
#define	_PR_MD_MAP_ACCEPT_ERROR	_MD_os2_map_accept_error

PR_EXTERN(void) _MD_os2_map_acceptex_error(PRInt32 err);
#define	_PR_MD_MAP_ACCEPTEX_ERROR	_MD_os2_map_acceptex_error

PR_EXTERN(void) _MD_os2_map_connect_error(PRInt32 err);
#define	_PR_MD_MAP_CONNECT_ERROR	_MD_os2_map_connect_error

PR_EXTERN(void) _MD_os2_map_bind_error(PRInt32 err);
#define	_PR_MD_MAP_BIND_ERROR	_MD_os2_map_bind_error

PR_EXTERN(void) _MD_os2_map_listen_error(PRInt32 err);
#define	_PR_MD_MAP_LISTEN_ERROR	_MD_os2_map_listen_error

PR_EXTERN(void) _MD_os2_map_shutdown_error(PRInt32 err);
#define	_PR_MD_MAP_SHUTDOWN_ERROR	_MD_os2_map_shutdown_error

PR_EXTERN(void) _MD_os2_map_getsockname_error(PRInt32 err);
#define	_PR_MD_MAP_GETSOCKNAME_ERROR	_MD_os2_map_getsockname_error

PR_EXTERN(void) _MD_os2_map_getpeername_error(PRInt32 err);
#define	_PR_MD_MAP_GETPEERNAME_ERROR	_MD_os2_map_getpeername_error

PR_EXTERN(void) _MD_os2_map_getsockopt_error(PRInt32 err);
#define	_PR_MD_MAP_GETSOCKOPT_ERROR	_MD_os2_map_getsockopt_error

PR_EXTERN(void) _MD_os2_map_setsockopt_error(PRInt32 err);
#define	_PR_MD_MAP_SETSOCKOPT_ERROR	_MD_os2_map_setsockopt_error

PR_EXTERN(void) _MD_os2_map_open_error(PRInt32 err);
#define	_PR_MD_MAP_OPEN_ERROR	_MD_os2_map_open_error

PR_EXTERN(void) _MD_os2_map_gethostname_error(PRInt32 err);
#define	_PR_MD_MAP_GETHOSTNAME_ERROR	_MD_os2_map_gethostname_error

PR_EXTERN(void) _MD_os2_map_select_error(PRInt32 err);
#define	_PR_MD_MAP_SELECT_ERROR	_MD_os2_map_select_error

PR_EXTERN(void) _MD_os2_map_lockf_error(int err);
#define _PR_MD_MAP_LOCKF_ERROR  _MD_os2_map_lockf_error

#endif /* nspr_os2_errors_h___ */
