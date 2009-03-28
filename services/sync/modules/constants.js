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
 * The Original Code is Bookmarks Sync.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dan Mills <thunder@mozilla.com>
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

const EXPORTED_SYMBOLS = ['WEAVE_VERSION', 'MIN_SERVER_STORAGE_VERSION',
			  'PREFS_BRANCH',
			  'MODE_RDONLY', 'MODE_WRONLY',
			  'MODE_CREATE', 'MODE_APPEND', 'MODE_TRUNCATE',
			  'PERMS_FILE', 'PERMS_PASSFILE', 'PERMS_DIRECTORY',
			  'ONE_BYTE', 'ONE_KILOBYTE', 'ONE_MEGABYTE',
                          'CONNECTION_TIMEOUT', 'KEEP_DELTAS',
                          'WEAVE_STATUS_OK', 'WEAVE_STATUS_FAILED',
                          'WEAVE_STATUS_PARTIAL', 'SERVER_LOW_QUOTA',
                          'SERVER_DOWNTIME', 'SERVER_UNREACHABLE',
                          'LOGIN_FAILED_NO_USERNAME', 'LOGIN_FAILED_NO_PASSWORD',
                          'LOGIN_FAILED_REJECTED', 'METARECORD_DOWNLOAD_FAIL',
                          'VERSION_OUT_OF_DATE', 'DESKTOP_VERSION_OUT_OF_DATE',
                          'KEYS_DOWNLOAD_FAIL', 'NO_KEYS_NO_KEYGEN', 'KEYS_UPLOAD_FAIL',
                          'SETUP_FAILED_NO_PASSPHRASE', 'ABORT_SYNC_COMMAND',
                          'kSyncWeaveDisabled', 'kSyncNotLoggedIn',
                          'kSyncNetworkOffline', 'kSyncInPrivateBrowsing',
                          'kSyncNotScheduled'];

const WEAVE_VERSION = "@weave_version@";

// last client version's server storage this version supports
// e.g. if set to the current version, this client will wipe the server
// data stored by any older client
const MIN_SERVER_STORAGE_VERSION = "@weave_version@";

const PREFS_BRANCH = "extensions.weave.";

const MODE_RDONLY   = 0x01;
const MODE_WRONLY   = 0x02;
const MODE_CREATE   = 0x08;
const MODE_APPEND   = 0x10;
const MODE_TRUNCATE = 0x20;

const PERMS_FILE      = 0644;
const PERMS_PASSFILE  = 0600;
const PERMS_DIRECTORY = 0755;

const ONE_BYTE = 1;
const ONE_KILOBYTE = 1024 * ONE_BYTE;
const ONE_MEGABYTE = 1024 * ONE_KILOBYTE;

const CONNECTION_TIMEOUT = 30000;

const KEEP_DELTAS = 25;

// Top-level statuses:
const WEAVE_STATUS_OK = "Sync succeeded.";
const WEAVE_STATUS_FAILED = "Sync failed.";
const WEAVE_STATUS_PARTIAL = "Sync partially succeeded, some data failed to sync.";

// Server statuses (Not mutually exclusive):
const SERVER_LOW_QUOTA = "Getting close to your Weave server storage quota.";
const SERVER_DOWNTIME = "Weave server is overloaded, try agian in 30 sec.";
const SERVER_UNREACHABLE = "Weave server is unreachable.";

// Ways that a sync can fail during setup or login:
const LOGIN_FAILED_NO_USERNAME = "No username set, login failed.";
const LOGIN_FAILED_NO_PASSWORD = "No password set, login failed.";
const LOGIN_FAILED_REJECTED = "Incorrect username or password.";
const METARECORD_DOWNLOAD_FAIL = "Can't download metadata record, HTTP error.";
const VERSION_OUT_OF_DATE = "This copy of Weave needs to be updated.";
const DESKTOP_VERSION_OUT_OF_DATE = "Weave needs updating on your desktop browser.";
const KEYS_DOWNLOAD_FAIL = "Can't download keys from server, HTTP error.";
const NO_KEYS_NO_KEYGEN = "Key generation disabled. Sync from the desktop first.";
const KEYS_UPLOAD_FAIL = "Could not upload keys.";
const SETUP_FAILED_NO_PASSPHRASE = "Could not get encryption passphrase.";

// Ways that a sync can be disabled
const kSyncWeaveDisabled = "Weave is disabled";
const kSyncNotLoggedIn = "User is not logged in";
const kSyncNetworkOffline = "Network is offline";
const kSyncInPrivateBrowsing = "Private browsing is enabled";
const kSyncNotScheduled = "Not scheduled to do sync";
// If one of these happens, leave the top-level status the same!

// Ways that a sync can be aborted:
const ABORT_SYNC_COMMAND = "aborting sync, process commands said so";

