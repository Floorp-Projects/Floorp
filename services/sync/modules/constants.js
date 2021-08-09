/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Process each item in the "constants hash" to add to "global" and give a name
var EXPORTED_SYMBOLS = [];
for (let [key, val] of Object.entries({
  // Don't manually modify this line, as it is automatically replaced on merge day
  // by the gecko_migration.py script.
  WEAVE_VERSION: "1.95.0",

  // Sync Server API version that the client supports.
  SYNC_API_VERSION: "1.5",

  // Version of the data format this client supports. The data format describes
  // how records are packaged; this is separate from the Server API version and
  // the per-engine cleartext formats.
  STORAGE_VERSION: 5,
  PREFS_BRANCH: "services.sync.",

  // Put in [] because those aren't allowed in a collection name.
  DEFAULT_KEYBUNDLE_NAME: "[default]",

  // Key dimensions.
  SYNC_KEY_ENCODED_LENGTH: 26,
  SYNC_KEY_DECODED_LENGTH: 16,

  NO_SYNC_NODE_INTERVAL: 10 * 60 * 1000, // 10 minutes

  MAX_ERROR_COUNT_BEFORE_BACKOFF: 3,

  // Backoff intervals
  MINIMUM_BACKOFF_INTERVAL: 15 * 60 * 1000, // 15 minutes
  MAXIMUM_BACKOFF_INTERVAL: 8 * 60 * 60 * 1000, // 8 hours

  // HMAC event handling timeout.
  // 10 minutes: a compromise between the multi-desktop sync interval
  // and the mobile sync interval.
  HMAC_EVENT_INTERVAL: 600000,

  // How long to wait between sync attempts if the Master Password is locked.
  MASTER_PASSWORD_LOCKED_RETRY_INTERVAL: 15 * 60 * 1000, // 15 minutes

  // 50 is hardcoded here because of URL length restrictions.
  // (GUIDs can be up to 64 chars long.)
  // Individual engines can set different values for their limit if their
  // identifiers are shorter.
  DEFAULT_GUID_FETCH_BATCH_SIZE: 50,

  // Default batch size for download batching
  // (how many records are fetched at a time from the server when batching is used).
  DEFAULT_DOWNLOAD_BATCH_SIZE: 1000,

  // score thresholds for early syncs
  SINGLE_USER_THRESHOLD: 1000,
  MULTI_DEVICE_THRESHOLD: 300,

  // Other score increment constants
  SCORE_INCREMENT_SMALL: 1,
  SCORE_INCREMENT_MEDIUM: 10,

  // Instant sync score increment
  SCORE_INCREMENT_XLARGE: 300 + 1, //MULTI_DEVICE_THRESHOLD + 1

  // Delay before incrementing global score
  SCORE_UPDATE_DELAY: 100,

  // Delay for the back observer debouncer. This is chosen to be longer than any
  // observed spurious idle/back events and short enough to pre-empt user activity.
  IDLE_OBSERVER_BACK_DELAY: 100,

  // Duplicate URI_LENGTH_MAX from Places (from nsNavHistory.h), used to discard
  // tabs with huge uris during tab sync.
  URI_LENGTH_MAX: 65536,

  MAX_HISTORY_UPLOAD: 5000,
  MAX_HISTORY_DOWNLOAD: 5000,

  // Top-level statuses:
  STATUS_OK: "success.status_ok",
  SYNC_FAILED: "error.sync.failed",
  LOGIN_FAILED: "error.login.failed",
  SYNC_FAILED_PARTIAL: "error.sync.failed_partial",
  CLIENT_NOT_CONFIGURED: "service.client_not_configured",
  STATUS_DISABLED: "service.disabled",
  MASTER_PASSWORD_LOCKED: "service.master_password_locked",

  // success states
  LOGIN_SUCCEEDED: "success.login",
  SYNC_SUCCEEDED: "success.sync",
  ENGINE_SUCCEEDED: "success.engine",

  // login failure status codes:
  LOGIN_FAILED_NO_USERNAME: "error.login.reason.no_username",
  LOGIN_FAILED_NO_PASSPHRASE: "error.login.reason.no_recoverykey",
  LOGIN_FAILED_NETWORK_ERROR: "error.login.reason.network",
  LOGIN_FAILED_SERVER_ERROR: "error.login.reason.server",
  LOGIN_FAILED_INVALID_PASSPHRASE: "error.login.reason.recoverykey",
  LOGIN_FAILED_LOGIN_REJECTED: "error.login.reason.account",

  // sync failure status codes
  METARECORD_DOWNLOAD_FAIL: "error.sync.reason.metarecord_download_fail",
  VERSION_OUT_OF_DATE: "error.sync.reason.version_out_of_date",
  CREDENTIALS_CHANGED: "error.sync.reason.credentials_changed",
  ABORT_SYNC_COMMAND: "aborting sync, process commands said so",
  NO_SYNC_NODE_FOUND: "error.sync.reason.no_node_found",
  OVER_QUOTA: "error.sync.reason.over_quota",
  SERVER_MAINTENANCE: "error.sync.reason.serverMaintenance",

  RESPONSE_OVER_QUOTA: "14",

  // engine failure status codes
  ENGINE_UPLOAD_FAIL: "error.engine.reason.record_upload_fail",
  ENGINE_DOWNLOAD_FAIL: "error.engine.reason.record_download_fail",
  ENGINE_UNKNOWN_FAIL: "error.engine.reason.unknown_fail",
  ENGINE_APPLY_FAIL: "error.engine.reason.apply_fail",
  // an upload failure where the batch was interrupted with a 412
  ENGINE_BATCH_INTERRUPTED: "error.engine.reason.batch_interrupted",

  // Ways that a sync can be disabled (messages only to be printed in debug log)
  kSyncMasterPasswordLocked: "User elected to leave Master Password locked",
  kSyncWeaveDisabled: "Weave is disabled",
  kSyncNetworkOffline: "Network is offline",
  kSyncBackoffNotMet: "Trying to sync before the server said it's okay",
  kFirstSyncChoiceNotMade: "User has not selected an action for first sync",
  kSyncNotConfigured: "Sync is not configured",
  kFirefoxShuttingDown: "Firefox is about to shut down",

  DEVICE_TYPE_DESKTOP: "desktop",
  DEVICE_TYPE_MOBILE: "mobile",

  SQLITE_MAX_VARIABLE_NUMBER: 999,
})) {
  this[key] = val;
  this.EXPORTED_SYMBOLS.push(key);
}
