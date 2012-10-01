/*
 * This file essentially replicates NSPR's source for the functions that
 * map system-specific error codes to NSPR error codes.  We would use 
 * NSPR's functions, instead of duplicating them, but they're private.
 * As long as SSL's server session cache code must do platform native I/O
 * to accomplish its job, and NSPR's error mapping functions remain private,
 * This code will continue to need to be replicated.
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id: os2_err.h,v 1.5 2012/04/25 14:50:12 gerv%gerv.net Exp $ */

/*  NSPR doesn't make these functions public, so we have to duplicate
**  them in NSS.
*/

//HCT  Based on Win32err.h
extern void nss_MD_os2_map_accept_error(PRInt32 err);
extern void nss_MD_os2_map_acceptex_error(PRInt32 err);
extern void nss_MD_os2_map_access_error(PRInt32 err);
extern void nss_MD_os2_map_bind_error(PRInt32 err);
extern void nss_MD_os2_map_close_error(PRInt32 err);
extern void nss_MD_os2_map_closedir_error(PRInt32 err);
extern void nss_MD_os2_map_connect_error(PRInt32 err);
extern void nss_MD_os2_map_default_error(PRInt32 err);
extern void nss_MD_os2_map_delete_error(PRInt32 err);
extern void nss_MD_os2_map_fstat_error(PRInt32 err);
extern void nss_MD_os2_map_fsync_error(PRInt32 err);
extern void nss_MD_os2_map_gethostname_error(PRInt32 err);
extern void nss_MD_os2_map_getpeername_error(PRInt32 err);
extern void nss_MD_os2_map_getsockname_error(PRInt32 err);
extern void nss_MD_os2_map_getsockopt_error(PRInt32 err);
extern void nss_MD_os2_map_listen_error(PRInt32 err);
extern void nss_MD_os2_map_lockf_error(PRInt32 err);
extern void nss_MD_os2_map_lseek_error(PRInt32 err);
extern void nss_MD_os2_map_mkdir_error(PRInt32 err);
extern void nss_MD_os2_map_open_error(PRInt32 err);
extern void nss_MD_os2_map_opendir_error(PRInt32 err);
extern void nss_MD_os2_map_read_error(PRInt32 err);
extern void nss_MD_os2_map_readdir_error(PRInt32 err);
extern void nss_MD_os2_map_recv_error(PRInt32 err);
extern void nss_MD_os2_map_recvfrom_error(PRInt32 err);
extern void nss_MD_os2_map_rename_error(PRInt32 err);
extern void nss_MD_os2_map_rmdir_error(PRInt32 err);
extern void nss_MD_os2_map_select_error(PRInt32 err);
extern void nss_MD_os2_map_send_error(PRInt32 err);
extern void nss_MD_os2_map_sendto_error(PRInt32 err);
extern void nss_MD_os2_map_setsockopt_error(PRInt32 err);
extern void nss_MD_os2_map_shutdown_error(PRInt32 err);
extern void nss_MD_os2_map_socket_error(PRInt32 err);
extern void nss_MD_os2_map_stat_error(PRInt32 err);
extern void nss_MD_os2_map_transmitfile_error(PRInt32 err);
extern void nss_MD_os2_map_write_error(PRInt32 err);
