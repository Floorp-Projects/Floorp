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

const EXPORTED_SYMBOLS = ["WEAVE_VERSION", "COMPATIBLE_VERSION",
			  "PREFS_BRANCH", "PWDMGR_HOST",
			  'MODE_RDONLY', 'MODE_WRONLY',
			  'MODE_CREATE', 'MODE_APPEND', 'MODE_TRUNCATE',
			  'PERMS_FILE', 'PERMS_PASSFILE', 'PERMS_DIRECTORY',
			  'ONE_BYTE', 'ONE_KILOBYTE', 'ONE_MEGABYTE',
        'CONNECTION_TIMEOUT', 'MAX_UPLOAD_RECORDS',
        'SYNC_SUCCEEDED', 'LOGIN_SUCCEEDED', 'ENGINE_SUCCEEDED',
        'STATUS_OK', 'LOGIN_FAILED', 'SYNC_FAILED',
        'SYNC_FAILED_PARTIAL', 'STATUS_DISABLED', 'SERVER_LOW_QUOTA',
        'SERVER_DOWNTIME', 'SERVER_UNREACHABLE',
        'LOGIN_FAILED_NO_USERNAME', 'LOGIN_FAILED_NO_PASSWORD',
        'LOGIN_FAILED_NETWORK_ERROR','LOGIN_FAILED_INVALID_PASSPHRASE', 
        'LOGIN_FAILED_LOGIN_REJECTED', 'METARECORD_DOWNLOAD_FAIL',
        'VERSION_OUT_OF_DATE', 'DESKTOP_VERSION_OUT_OF_DATE',
        'KEYS_DOWNLOAD_FAIL', 'NO_KEYS_NO_KEYGEN', 'KEYS_UPLOAD_FAIL',
        'ENGINE_UPLOAD_FAIL', 'ENGINE_DOWNLOAD_FAIL', 'ENGINE_UNKNOWN_FAIL',
        'ENGINE_METARECORD_UPLOAD_FAIL',
        'SETUP_FAILED_NO_PASSPHRASE', 'ABORT_SYNC_COMMAND',
        'kSyncWeaveDisabled', 'kSyncNotLoggedIn',
        'kSyncNetworkOffline', 'kSyncInPrivateBrowsing',
        'kSyncNotScheduled',
        'FIREFOX_ID', 'THUNDERBIRD_ID', 'FENNEC_ID', 'SEAMONKEY_ID'];

const WEAVE_VERSION = "@weave_version@";

// last client version's server storage this version supports
// e.g. if set to the current version, this client will wipe the server
// data stored by any older client
const COMPATIBLE_VERSION = "@compatible_version@";

const PREFS_BRANCH = "extensions.weave.";

// Host "key" to access Weave Identity in the password manager
const PWDMGR_HOST = "chrome://weave";

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

// How many records to upload in a single POST
// If there are more, multiple POST calls will be made.
// Record size limit is currently 10K, so 100 is a bit over 1MB
const MAX_UPLOAD_RECORDS = 100;

// Top-level statuses:
const STATUS_OK =                         "success.status_ok";
const SYNC_FAILED =                       "error.sync.failed";
const LOGIN_FAILED =                      "error.login.failed";
const SYNC_FAILED_PARTIAL =               "error.sync.failed_partial";
const STATUS_DISABLED =                   "service.disabled";

// success states
const LOGIN_SUCCEEDED =                   "success.login";
const SYNC_SUCCEEDED =                    "success.sync";
const ENGINE_SUCCEEDED =                  "success.engine";

// login failure status codes:
const LOGIN_FAILED_NO_USERNAME =          "error.login.reason.no_username";
const LOGIN_FAILED_NO_PASSWORD =          "error.login.reason.no_password";
const LOGIN_FAILED_NETWORK_ERROR =        "error.login.reason.network";
const LOGIN_FAILED_INVALID_PASSPHRASE =   "error.login.reason.passphrase.";
const LOGIN_FAILED_LOGIN_REJECTED =       "error.login.reason.password";

// sync failure status codes
const METARECORD_DOWNLOAD_FAIL =          "error.sync.reason.metarecord_download_fail";
const VERSION_OUT_OF_DATE =               "error.sync.reason.version_out_of_date";
const DESKTOP_VERSION_OUT_OF_DATE =       "error.sync.reason.desktop_version_out_of_date";
const KEYS_DOWNLOAD_FAIL =                "error.sync.reason.keys_download_fail";
const NO_KEYS_NO_KEYGEN =                 "error.sync.reason.no_keys_no_keygen";
const KEYS_UPLOAD_FAIL =                  "error.sync.reason.keys_upload_fail";
const SETUP_FAILED_NO_PASSPHRASE =        "error.sync.reason.setup_failed_no_passphrase";

// engine failure status codes
const ENGINE_UPLOAD_FAIL =                "error.engine.reason.record_upload_fail";
const ENGINE_DOWNLOAD_FAIL =              "error.engine.reason.record_download_fail";
const ENGINE_UNKNOWN_FAIL =               "error.engine.reason.unknown_fail";
const ENGINE_METARECORD_UPLOAD_FAIL =     "error.engine.reason.metarecord_upload_fail";

// Server statuses (Not mutually exclusive):
const SERVER_LOW_QUOTA = "Getting close to your Weave server storage quota.";
const SERVER_DOWNTIME = "Weave server is overloaded, try agian in 30 sec.";
const SERVER_UNREACHABLE = "Weave server is unreachable.";

// Ways that a sync can be disabled
const kSyncWeaveDisabled = "Weave is disabled";
const kSyncNotLoggedIn = "User is not logged in";
const kSyncNetworkOffline = "Network is offline";
const kSyncInPrivateBrowsing = "Private browsing is enabled";
const kSyncNotScheduled = "Not scheduled to do sync";
const kSyncBackoffNotMet = "Trying to sync before the server said it's okay";


// Ways that a sync can be aborted:
const ABORT_SYNC_COMMAND = "aborting sync, process commands said so";

// Application IDs
const FIREFOX_ID = "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}";
const THUNDERBIRD_ID = "{3550f703-e582-4d05-9a08-453d09bdfdc6}";
const FENNEC_ID = "{a23983c0-fd0e-11dc-95ff-0800200c9a66}";
const SEAMONKEY_ID = "{92650c4d-4b8e-4d2a-b7eb-24ecf4f6b63a}";

//UI constants

// How many data types (bookmarks, history, etc) to display per row
const UI_DATA_TYPES_PER_ROW = 3;