/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Log.jsm");

// loglevel should be one of "Fatal", "Error", "Warn", "Info", "Config",
// "Debug", "Trace" or "All". If none is specified, "Debug" will be used by
// default.  Note "Debug" is usually appropriate so that when this log is
// included in the Sync file logs we get verbose output.
const PREF_LOG_LEVEL = "identity.fxaccounts.loglevel";
// The level of messages that will be dumped to the console.  If not specified,
// "Error" will be used.
const PREF_LOG_LEVEL_DUMP = "identity.fxaccounts.log.appender.dump";

// A pref that can be set so "sensitive" information (eg, personally
// identifiable info, credentials, etc) will be logged.
const PREF_LOG_SENSITIVE_DETAILS = "identity.fxaccounts.log.sensitive";

XPCOMUtils.defineLazyGetter(this, 'log', function() {
  let log = Log.repository.getLogger("FirefoxAccounts");
  // We set the log level to debug, but the default dump appender is set to
  // the level reflected in the pref.  Other code that consumes FxA may then
  // choose to add another appender at a different level.
  log.level = Log.Level.Debug;
  let appender = new Log.DumpAppender();
  appender.level = Log.Level.Error;

  log.addAppender(appender);
  try {
    // The log itself.
    let level =
      Services.prefs.getPrefType(PREF_LOG_LEVEL) == Ci.nsIPrefBranch.PREF_STRING
      && Services.prefs.getCharPref(PREF_LOG_LEVEL);
    log.level = Log.Level[level] || Log.Level.Debug;

    // The appender.
    level =
      Services.prefs.getPrefType(PREF_LOG_LEVEL_DUMP) == Ci.nsIPrefBranch.PREF_STRING
      && Services.prefs.getCharPref(PREF_LOG_LEVEL_DUMP);
    appender.level = Log.Level[level] || Log.Level.Error;
  } catch (e) {
    log.error(e);
  }

  return log;
});

// A boolean to indicate if personally identifiable information (or anything
// else sensitive, such as credentials) should be logged.
XPCOMUtils.defineLazyGetter(this, 'logPII', function() {
  try {
    return Services.prefs.getBoolPref(PREF_LOG_SENSITIVE_DETAILS);
  } catch (_) {
    return false;
  }
});

this.FXACCOUNTS_PERMISSION = "firefox-accounts";

this.DATA_FORMAT_VERSION = 1;
this.DEFAULT_STORAGE_FILENAME = "signedInUser.json";

// Token life times.
// Having this parameter be short has limited security value and can cause
// spurious authentication values if the client's clock is skewed and
// we fail to adjust. See Bug 983256.
this.ASSERTION_LIFETIME = 1000 * 3600 * 24 * 365 * 25; // 25 years
// This is a time period we want to guarantee that the assertion will be
// valid after we generate it (e.g., the signed cert won't expire in this
// period).
this.ASSERTION_USE_PERIOD = 1000 * 60 * 5; // 5 minutes
this.CERT_LIFETIME      = 1000 * 3600 * 6;  // 6 hours
this.KEY_LIFETIME       = 1000 * 3600 * 12; // 12 hours

// Polling timings.
this.POLL_SESSION       = 1000 * 60 * 5;    // 5 minutes
this.POLL_STEP          = 1000 * 3;         // 3 seconds

// Observer notifications.
this.ONLOGIN_NOTIFICATION = "fxaccounts:onlogin";
this.ONVERIFIED_NOTIFICATION = "fxaccounts:onverified";
this.ONLOGOUT_NOTIFICATION = "fxaccounts:onlogout";
// Internal to services/fxaccounts only
this.ON_FXA_UPDATE_NOTIFICATION = "fxaccounts:update";

// UI Requests.
this.UI_REQUEST_SIGN_IN_FLOW = "signInFlow";
this.UI_REQUEST_REFRESH_AUTH = "refreshAuthentication";

// Server errno.
// From https://github.com/mozilla/fxa-auth-server/blob/master/docs/api.md#response-format
this.ERRNO_ACCOUNT_ALREADY_EXISTS         = 101;
this.ERRNO_ACCOUNT_DOES_NOT_EXIST         = 102;
this.ERRNO_INCORRECT_PASSWORD             = 103;
this.ERRNO_UNVERIFIED_ACCOUNT             = 104;
this.ERRNO_INVALID_VERIFICATION_CODE      = 105;
this.ERRNO_NOT_VALID_JSON_BODY            = 106;
this.ERRNO_INVALID_BODY_PARAMETERS        = 107;
this.ERRNO_MISSING_BODY_PARAMETERS        = 108;
this.ERRNO_INVALID_REQUEST_SIGNATURE      = 109;
this.ERRNO_INVALID_AUTH_TOKEN             = 110;
this.ERRNO_INVALID_AUTH_TIMESTAMP         = 111;
this.ERRNO_MISSING_CONTENT_LENGTH         = 112;
this.ERRNO_REQUEST_BODY_TOO_LARGE         = 113;
this.ERRNO_TOO_MANY_CLIENT_REQUESTS       = 114;
this.ERRNO_INVALID_AUTH_NONCE             = 115;
this.ERRNO_ENDPOINT_NO_LONGER_SUPPORTED   = 116;
this.ERRNO_INCORRECT_LOGIN_METHOD         = 117;
this.ERRNO_INCORRECT_KEY_RETRIEVAL_METHOD = 118;
this.ERRNO_INCORRECT_API_VERSION          = 119;
this.ERRNO_INCORRECT_EMAIL_CASE           = 120;
this.ERRNO_SERVICE_TEMP_UNAVAILABLE       = 201;
this.ERRNO_UNKNOWN_ERROR                  = 999;

// Errors.
this.ERROR_ACCOUNT_ALREADY_EXISTS         = "ACCOUNT_ALREADY_EXISTS";
this.ERROR_ACCOUNT_DOES_NOT_EXIST         = "ACCOUNT_DOES_NOT_EXIST ";
this.ERROR_ALREADY_SIGNED_IN_USER         = "ALREADY_SIGNED_IN_USER";
this.ERROR_ENDPOINT_NO_LONGER_SUPPORTED   = "ENDPOINT_NO_LONGER_SUPPORTED";
this.ERROR_INCORRECT_API_VERSION          = "INCORRECT_API_VERSION";
this.ERROR_INCORRECT_EMAIL_CASE           = "INCORRECT_EMAIL_CASE";
this.ERROR_INCORRECT_KEY_RETRIEVAL_METHOD = "INCORRECT_KEY_RETRIEVAL_METHOD";
this.ERROR_INCORRECT_LOGIN_METHOD         = "INCORRECT_LOGIN_METHOD";
this.ERROR_INVALID_EMAIL                  = "INVALID_EMAIL";
this.ERROR_INVALID_AUDIENCE               = "INVALID_AUDIENCE";
this.ERROR_INVALID_AUTH_TOKEN             = "INVALID_AUTH_TOKEN";
this.ERROR_INVALID_AUTH_TIMESTAMP         = "INVALID_AUTH_TIMESTAMP";
this.ERROR_INVALID_AUTH_NONCE             = "INVALID_AUTH_NONCE";
this.ERROR_INVALID_BODY_PARAMETERS        = "INVALID_BODY_PARAMETERS";
this.ERROR_INVALID_PASSWORD               = "INVALID_PASSWORD";
this.ERROR_INVALID_VERIFICATION_CODE      = "INVALID_VERIFICATION_CODE";
this.ERROR_INVALID_REFRESH_AUTH_VALUE     = "INVALID_REFRESH_AUTH_VALUE";
this.ERROR_INVALID_REQUEST_SIGNATURE      = "INVALID_REQUEST_SIGNATURE";
this.ERROR_INTERNAL_INVALID_USER          = "INTERNAL_ERROR_INVALID_USER";
this.ERROR_MISSING_BODY_PARAMETERS        = "MISSING_BODY_PARAMETERS";
this.ERROR_MISSING_CONTENT_LENGTH         = "MISSING_CONTENT_LENGTH";
this.ERROR_NO_TOKEN_SESSION               = "NO_TOKEN_SESSION";
this.ERROR_NO_SILENT_REFRESH_AUTH         = "NO_SILENT_REFRESH_AUTH";
this.ERROR_NOT_VALID_JSON_BODY            = "NOT_VALID_JSON_BODY";
this.ERROR_OFFLINE                        = "OFFLINE";
this.ERROR_PERMISSION_DENIED              = "PERMISSION_DENIED";
this.ERROR_REQUEST_BODY_TOO_LARGE         = "REQUEST_BODY_TOO_LARGE";
this.ERROR_SERVER_ERROR                   = "SERVER_ERROR";
this.ERROR_TOO_MANY_CLIENT_REQUESTS       = "TOO_MANY_CLIENT_REQUESTS";
this.ERROR_SERVICE_TEMP_UNAVAILABLE       = "SERVICE_TEMPORARY_UNAVAILABLE";
this.ERROR_UI_ERROR                       = "UI_ERROR";
this.ERROR_UI_REQUEST                     = "UI_REQUEST";
this.ERROR_UNKNOWN                        = "UNKNOWN_ERROR";
this.ERROR_UNVERIFIED_ACCOUNT             = "UNVERIFIED_ACCOUNT";

// Error matching.
this.SERVER_ERRNO_TO_ERROR = {};
SERVER_ERRNO_TO_ERROR[ERRNO_ACCOUNT_ALREADY_EXISTS]         = ERROR_ACCOUNT_ALREADY_EXISTS;
SERVER_ERRNO_TO_ERROR[ERRNO_ACCOUNT_DOES_NOT_EXIST]         = ERROR_ACCOUNT_DOES_NOT_EXIST;
SERVER_ERRNO_TO_ERROR[ERRNO_INCORRECT_PASSWORD]             = ERROR_INVALID_PASSWORD;
SERVER_ERRNO_TO_ERROR[ERRNO_UNVERIFIED_ACCOUNT]             = ERROR_UNVERIFIED_ACCOUNT;
SERVER_ERRNO_TO_ERROR[ERRNO_INVALID_VERIFICATION_CODE]      = ERROR_INVALID_VERIFICATION_CODE;
SERVER_ERRNO_TO_ERROR[ERRNO_NOT_VALID_JSON_BODY]            = ERROR_NOT_VALID_JSON_BODY;
SERVER_ERRNO_TO_ERROR[ERRNO_INVALID_BODY_PARAMETERS]        = ERROR_INVALID_BODY_PARAMETERS;
SERVER_ERRNO_TO_ERROR[ERRNO_MISSING_BODY_PARAMETERS]        = ERROR_MISSING_BODY_PARAMETERS;
SERVER_ERRNO_TO_ERROR[ERRNO_INVALID_REQUEST_SIGNATURE]      = ERROR_INVALID_REQUEST_SIGNATURE;
SERVER_ERRNO_TO_ERROR[ERRNO_INVALID_AUTH_TOKEN]             = ERROR_INVALID_AUTH_TOKEN;
SERVER_ERRNO_TO_ERROR[ERRNO_INVALID_AUTH_TIMESTAMP]         = ERROR_INVALID_AUTH_TIMESTAMP;
SERVER_ERRNO_TO_ERROR[ERRNO_MISSING_CONTENT_LENGTH]         = ERROR_MISSING_CONTENT_LENGTH;
SERVER_ERRNO_TO_ERROR[ERRNO_REQUEST_BODY_TOO_LARGE]         = ERROR_REQUEST_BODY_TOO_LARGE;
SERVER_ERRNO_TO_ERROR[ERRNO_TOO_MANY_CLIENT_REQUESTS]       = ERROR_TOO_MANY_CLIENT_REQUESTS;
SERVER_ERRNO_TO_ERROR[ERRNO_INVALID_AUTH_NONCE]             = ERROR_INVALID_AUTH_NONCE;
SERVER_ERRNO_TO_ERROR[ERRNO_ENDPOINT_NO_LONGER_SUPPORTED]   = ERROR_ENDPOINT_NO_LONGER_SUPPORTED;
SERVER_ERRNO_TO_ERROR[ERRNO_INCORRECT_LOGIN_METHOD]         = ERROR_INCORRECT_LOGIN_METHOD;
SERVER_ERRNO_TO_ERROR[ERRNO_INCORRECT_KEY_RETRIEVAL_METHOD] = ERROR_INCORRECT_KEY_RETRIEVAL_METHOD;
SERVER_ERRNO_TO_ERROR[ERRNO_INCORRECT_API_VERSION]          = ERROR_INCORRECT_API_VERSION;
SERVER_ERRNO_TO_ERROR[ERRNO_INCORRECT_EMAIL_CASE]           = ERROR_INCORRECT_EMAIL_CASE;
SERVER_ERRNO_TO_ERROR[ERRNO_SERVICE_TEMP_UNAVAILABLE]       = ERROR_SERVICE_TEMP_UNAVAILABLE;
SERVER_ERRNO_TO_ERROR[ERRNO_UNKNOWN_ERROR]                  = ERROR_UNKNOWN;

// Allow this file to be imported via Components.utils.import().
this.EXPORTED_SYMBOLS = Object.keys(this);
