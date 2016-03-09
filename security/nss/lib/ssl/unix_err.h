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

/*  NSPR doesn't make these functions public, so we have to duplicate
**  them in NSS.
*/
extern void nss_MD_hpux_map_sendfile_error(int err);
extern void nss_MD_unix_map_accept_error(int err);
extern void nss_MD_unix_map_access_error(int err);
extern void nss_MD_unix_map_bind_error(int err);
extern void nss_MD_unix_map_close_error(int err);
extern void nss_MD_unix_map_closedir_error(int err);
extern void nss_MD_unix_map_connect_error(int err);
extern void nss_MD_unix_map_default_error(int err);
extern void nss_MD_unix_map_flock_error(int err);
extern void nss_MD_unix_map_fstat_error(int err);
extern void nss_MD_unix_map_fsync_error(int err);
extern void nss_MD_unix_map_gethostname_error(int err);
extern void nss_MD_unix_map_getpeername_error(int err);
extern void nss_MD_unix_map_getsockname_error(int err);
extern void nss_MD_unix_map_getsockopt_error(int err);
extern void nss_MD_unix_map_listen_error(int err);
extern void nss_MD_unix_map_lockf_error(int err);
extern void nss_MD_unix_map_lseek_error(int err);
extern void nss_MD_unix_map_mkdir_error(int err);
extern void nss_MD_unix_map_mmap_error(int err);
extern void nss_MD_unix_map_open_error(int err);
extern void nss_MD_unix_map_opendir_error(int err);
extern void nss_MD_unix_map_poll_error(int err);
extern void nss_MD_unix_map_poll_revents_error(int err);
extern void nss_MD_unix_map_read_error(int err);
extern void nss_MD_unix_map_readdir_error(int err);
extern void nss_MD_unix_map_recv_error(int err);
extern void nss_MD_unix_map_recvfrom_error(int err);
extern void nss_MD_unix_map_rename_error(int err);
extern void nss_MD_unix_map_rmdir_error(int err);
extern void nss_MD_unix_map_select_error(int err);
extern void nss_MD_unix_map_send_error(int err);
extern void nss_MD_unix_map_sendto_error(int err);
extern void nss_MD_unix_map_setsockopt_error(int err);
extern void nss_MD_unix_map_shutdown_error(int err);
extern void nss_MD_unix_map_socket_error(int err);
extern void nss_MD_unix_map_socketavailable_error(int err);
extern void nss_MD_unix_map_socketpair_error(int err);
extern void nss_MD_unix_map_stat_error(int err);
extern void nss_MD_unix_map_unlink_error(int err);
extern void nss_MD_unix_map_write_error(int err);
extern void nss_MD_unix_map_writev_error(int err);
