/*
 * This file essentially replicates NSPR's source for the functions that
 * map system-specific error codes to NSPR error codes.  We would use 
 * NSPR's functions, instead of duplicating them, but they're private.
 * As long as SSL's server session cache code must do platform native I/O
 * to accomplish its job, and NSPR's error mapping functions remain private,
 * this code will continue to need to be replicated.
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 *
 * $Id: unix_err.h,v 1.1 2000/03/31 19:37:18 relyea%netscape.com Exp $
 */

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
