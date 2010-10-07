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

// Process each item in the "constants hash" to add to "global" and give a name
let EXPORTED_SYMBOLS = [((this[key] = val), key) for ([key, val] in Iterator({

WEAVE_CHANNEL:                         "@xpi_type@",
WEAVE_VERSION:                         "@weave_version@",
WEAVE_ID:                              "@weave_id@",

// Version of the data format this client supports. The data format describes
// how records are packaged; this is separate from the Server API version and
// the per-engine cleartext formats.
STORAGE_VERSION:                       3,

UPDATED_DEV_URL:                       "https://services.mozilla.com/sync/updated/?version=@weave_version@&channel=@xpi_type@",
UPDATED_REL_URL:                       "http://www.mozilla.com/firefox/sync/updated.html",

PREFS_BRANCH:                          "services.sync.",

// Host "key" to access Weave Identity in the password manager
PWDMGR_HOST:                           "chrome://weave",
PWDMGR_PASSWORD_REALM:                 "Mozilla Services Password",
PWDMGR_PASSPHRASE_REALM:               "Mozilla Services Encryption Passphrase",

// Sync intervals for various clients configurations
SINGLE_USER_SYNC:                      24 * 60 * 60 * 1000, // 1 day
MULTI_DESKTOP_SYNC:                    60 * 60 * 1000, // 1 hour
MULTI_MOBILE_SYNC:                     5 * 60 * 1000, // 5 minutes
PARTIAL_DATA_SYNC:                     60 * 1000, // 1 minute

// score thresholds for early syncs
SINGLE_USER_THRESHOLD:                 1000,
MULTI_DESKTOP_THRESHOLD:               500,
MULTI_MOBILE_THRESHOLD:                100,

// File IO Flags
MODE_RDONLY:                           0x01,
MODE_WRONLY:                           0x02,
MODE_CREATE:                           0x08,
MODE_APPEND:                           0x10,
MODE_TRUNCATE:                         0x20,

// File Permission flags
PERMS_FILE:                            0644,
PERMS_PASSFILE:                        0600,
PERMS_DIRECTORY:                       0755,

// Number of records to upload in a single POST (multiple POSTS if exceeded)
// FIXME: Record size limit is 256k (new cluster), so this can be quite large!
// (Bug 569295)
MAX_UPLOAD_RECORDS:                    100,

// Top-level statuses:
STATUS_OK:                             "success.status_ok",
SYNC_FAILED:                           "error.sync.failed",
LOGIN_FAILED:                          "error.login.failed",
SYNC_FAILED_PARTIAL:                   "error.sync.failed_partial",
CLIENT_NOT_CONFIGURED:                 "service.client_not_configured",
STATUS_DISABLED:                       "service.disabled",

// success states
LOGIN_SUCCEEDED:                       "success.login",
SYNC_SUCCEEDED:                        "success.sync",
ENGINE_SUCCEEDED:                      "success.engine",

// login failure status codes:
LOGIN_FAILED_NO_USERNAME:              "error.login.reason.no_username",
LOGIN_FAILED_NO_PASSWORD:              "error.login.reason.no_password",
LOGIN_FAILED_NO_PASSPHRASE:            "error.login.reason.no_synckey",
LOGIN_FAILED_NETWORK_ERROR:            "error.login.reason.network",
LOGIN_FAILED_SERVER_ERROR:             "error.login.reason.server",
LOGIN_FAILED_INVALID_PASSPHRASE:       "error.login.reason.synckey",
LOGIN_FAILED_LOGIN_REJECTED:           "error.login.reason.account",

// sync failure status codes
METARECORD_DOWNLOAD_FAIL:              "error.sync.reason.metarecord_download_fail",
VERSION_OUT_OF_DATE:                   "error.sync.reason.version_out_of_date",
DESKTOP_VERSION_OUT_OF_DATE:           "error.sync.reason.desktop_version_out_of_date",
KEYS_DOWNLOAD_FAIL:                    "error.sync.reason.keys_download_fail",
NO_KEYS_NO_KEYGEN:                     "error.sync.reason.no_keys_no_keygen",
KEYS_UPLOAD_FAIL:                      "error.sync.reason.keys_upload_fail",
SETUP_FAILED_NO_PASSPHRASE:            "error.sync.reason.setup_failed_no_passphrase",
CREDENTIALS_CHANGED:                   "error.sync.reason.credentials_changed",
ABORT_SYNC_COMMAND:                    "aborting sync, process commands said so",
NO_SYNC_NODE_FOUND:                    "error.sync.reason.no_node_found",
OVER_QUOTA:                            "error.sync.reason.over_quota",

RESPONSE_OVER_QUOTA:                   "14",

// engine failure status codes
ENGINE_UPLOAD_FAIL:                    "error.engine.reason.record_upload_fail",
ENGINE_DOWNLOAD_FAIL:                  "error.engine.reason.record_download_fail",
ENGINE_UNKNOWN_FAIL:                   "error.engine.reason.unknown_fail",
ENGINE_METARECORD_DOWNLOAD_FAIL:       "error.engine.reason.metarecord_download_fail",
ENGINE_METARECORD_UPLOAD_FAIL:         "error.engine.reason.metarecord_upload_fail",

// Ways that a sync can be disabled (messages only to be printed in debug log)
kSyncWeaveDisabled:                    "Weave is disabled",
kSyncNotLoggedIn:                      "User is not logged in",
kSyncNetworkOffline:                   "Network is offline",
kSyncBackoffNotMet:                    "Trying to sync before the server said it's okay",
kFirstSyncChoiceNotMade:               "User has not selected an action for first sync",

// Application IDs
FIREFOX_ID:                            "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}",
FENNEC_ID:                             "{a23983c0-fd0e-11dc-95ff-0800200c9a66}",
SEAMONKEY_ID:                          "{92650c4d-4b8e-4d2a-b7eb-24ecf4f6b63a}",
TEST_HARNESS_ID:                       "xuth@mozilla.org",

MIN_PP_LENGTH:                         12,
MIN_PASS_LENGTH:                       8

}))];
