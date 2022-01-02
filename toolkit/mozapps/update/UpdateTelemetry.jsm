/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AUSTLMY"];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { BitsError, BitsUnknownError } = ChromeUtils.import(
  "resource://gre/modules/Bits.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var AUSTLMY = {
  // Telemetry for the application update background update check occurs when
  // the background update timer fires after the update interval which is
  // determined by the app.update.interval preference and its telemetry
  // histogram IDs have the suffix '_NOTIFY'.
  // Telemetry for the externally initiated background update check occurs when
  // a call is made to |checkForBackgroundUpdates| which is typically initiated
  // by an application when it has determined that the application should have
  // received an update. This has separate telemetry so it is possible to
  // analyze using the telemetry data systems that have not been updating when
  // they should have.

  // The update check was performed by the call to checkForBackgroundUpdates in
  // nsUpdateService.js.
  EXTERNAL: "EXTERNAL",
  // The update check was performed by the call to notify in nsUpdateService.js.
  NOTIFY: "NOTIFY",
  // The update check was performed after an update is already ready. There is
  // currently no way for a user to initiate an update check when there is a
  // ready update (the UI just prompts you to install the ready update). So
  // subsequent update checks are necessarily "notify" update checks, not
  // "external" ones.
  SUBSEQUENT: "SUBSEQUENT",

  /**
   * Values for the UPDATE_CHECK_CODE_NOTIFY and UPDATE_CHECK_CODE_EXTERNAL
   * Telemetry histograms.
   */
  // No update found (no notification)
  CHK_NO_UPDATE_FOUND: 0,
  // Update will be downloaded in the background (background download)
  CHK_DOWNLOAD_UPDATE: 1,
  // Showing prompt due to preference (update notification)
  CHK_SHOWPROMPT_PREF: 3,
  // Already has an active update in progress (no notification)
  CHK_HAS_ACTIVEUPDATE: 8,
  // A background download is already in progress (no notification)
  CHK_IS_DOWNLOADING: 9,
  // An update is already staged (no notification)
  CHK_IS_STAGED: 10,
  // An update is already downloaded (no notification)
  CHK_IS_DOWNLOADED: 11,
  // Note: codes 12-13 were removed along with the |app.update.enabled| pref.
  // Unable to check for updates per hasUpdateMutex() (no notification)
  CHK_NO_MUTEX: 14,
  // Unable to check for updates per gCanCheckForUpdates (no notification). This
  // should be covered by other codes and is recorded just in case.
  CHK_UNABLE_TO_CHECK: 15,
  // Note: code 16 was removed when the feature for disabling updates for the
  // session was removed.
  // Unable to perform a background check while offline (no notification)
  CHK_OFFLINE: 17,
  // Note: codes 18 - 21 were removed along with the certificate checking code.
  // General update check failure and threshold reached
  // (check failure notification)
  CHK_GENERAL_ERROR_PROMPT: 22,
  // General update check failure and threshold not reached (no notification)
  CHK_GENERAL_ERROR_SILENT: 23,
  // No compatible update found though there were updates (no notification)
  CHK_NO_COMPAT_UPDATE_FOUND: 24,
  // Update found for a previous version (no notification)
  CHK_UPDATE_PREVIOUS_VERSION: 25,
  // Update found without a type attribute (no notification)
  CHK_UPDATE_INVALID_TYPE: 27,
  // The system is no longer supported (system unsupported notification)
  CHK_UNSUPPORTED: 28,
  // Unable to apply updates (manual install to update notification)
  CHK_UNABLE_TO_APPLY: 29,
  // Unable to check for updates due to no OS version (no notification)
  CHK_NO_OS_VERSION: 30,
  // Unable to check for updates due to no OS ABI (no notification)
  CHK_NO_OS_ABI: 31,
  // Invalid update url (no notification)
  CHK_INVALID_DEFAULT_URL: 32,
  // Update elevation failures or cancelations threshold reached for this
  // version, OSX only (no notification)
  CHK_ELEVATION_DISABLED_FOR_VERSION: 35,
  // User opted out of elevated updates for the available update version, OSX
  // only (no notification)
  CHK_ELEVATION_OPTOUT_FOR_VERSION: 36,
  // Update checks disabled by enterprise policy
  CHK_DISABLED_BY_POLICY: 37,
  // Update check failed due to write error
  CHK_ERR_WRITE_FAILURE: 38,
  // Update check was delayed because another instance of the application is
  // currently running
  CHK_OTHER_INSTANCE: 39,
  // Cannot yet download update because no partial patch is available and an
  // update has already been downloaded.
  CHK_NO_PARTIAL_PATCH: 40,

  /**
   * Submit a telemetry ping for the update check result code or a telemetry
   * ping for a count type histogram count when no update was found. The no
   * update found ping is separate since it is the typical result, is less
   * interesting than the other result codes, and it is easier to analyze the
   * other codes without including it.
   *
   * @param  aSuffix
   *         The histogram id suffix for histogram IDs:
   *         UPDATE_CHECK_CODE_EXTERNAL
   *         UPDATE_CHECK_CODE_NOTIFY
   *         UPDATE_CHECK_CODE_SUBSEQUENT
   *         UPDATE_CHECK_NO_UPDATE_EXTERNAL
   *         UPDATE_CHECK_NO_UPDATE_NOTIFY
   *         UPDATE_CHECK_NO_UPDATE_SUBSEQUENT
   * @param  aCode
   *         An integer value as defined by the values that start with CHK_ in
   *         the above section.
   */
  pingCheckCode: function UT_pingCheckCode(aSuffix, aCode) {
    try {
      if (aCode == this.CHK_NO_UPDATE_FOUND) {
        let id = "UPDATE_CHECK_NO_UPDATE_" + aSuffix;
        // count type histogram
        Services.telemetry.getHistogramById(id).add();
      } else {
        let id = "UPDATE_CHECK_CODE_" + aSuffix;
        // enumerated type histogram
        Services.telemetry.getHistogramById(id).add(aCode);
      }
    } catch (e) {
      Cu.reportError(e);
    }
  },

  /**
   * Submit a telemetry ping for a failed update check's unhandled error code
   * when the pingCheckCode is CHK_GENERAL_ERROR_SILENT. The histogram is a
   * keyed count type with key names that are prefixed with 'AUS_CHECK_EX_ERR_'.
   *
   * @param  aSuffix
   *         The histogram id suffix for histogram IDs:
   *         UPDATE_CHECK_EXTENDED_ERROR_EXTERNAL
   *         UPDATE_CHECK_EXTENDED_ERROR_NOTIFY
   *         UPDATE_CHECK_EXTENDED_ERROR_SUBSEQUENT
   * @param  aCode
   *         The extended error value return by a failed update check.
   */
  pingCheckExError: function UT_pingCheckExError(aSuffix, aCode) {
    try {
      let id = "UPDATE_CHECK_EXTENDED_ERROR_" + aSuffix;
      let val = "AUS_CHECK_EX_ERR_" + aCode;
      // keyed count type histogram
      Services.telemetry.getKeyedHistogramById(id).add(val);
    } catch (e) {
      Cu.reportError(e);
    }
  },

  // The state code and if present the status error code were read on startup.
  STARTUP: "STARTUP",
  // The state code and status error code if present were read after staging.
  STAGE: "STAGE",

  // Patch type Complete
  PATCH_COMPLETE: "COMPLETE",
  // Patch type partial
  PATCH_PARTIAL: "PARTIAL",
  // Patch type unknown
  PATCH_UNKNOWN: "UNKNOWN",

  /**
   * Values for the UPDATE_DOWNLOAD_CODE_COMPLETE, UPDATE_DOWNLOAD_CODE_PARTIAL,
   * and UPDATE_DOWNLOAD_CODE_UNKNOWN Telemetry histograms.
   */
  DWNLD_SUCCESS: 0,
  DWNLD_RETRY_OFFLINE: 1,
  DWNLD_RETRY_NET_TIMEOUT: 2,
  DWNLD_RETRY_CONNECTION_REFUSED: 3,
  DWNLD_RETRY_NET_RESET: 4,
  DWNLD_ERR_NO_UPDATE: 5,
  DWNLD_ERR_NO_UPDATE_PATCH: 6,
  DWNLD_ERR_PATCH_SIZE_LARGER: 8,
  DWNLD_ERR_PATCH_SIZE_NOT_EQUAL: 9,
  DWNLD_ERR_BINDING_ABORTED: 10,
  DWNLD_ERR_ABORT: 11,
  DWNLD_ERR_DOCUMENT_NOT_CACHED: 12,
  DWNLD_ERR_VERIFY_NO_REQUEST: 13,
  DWNLD_ERR_VERIFY_PATCH_SIZE_NOT_EQUAL: 14,
  DWNLD_ERR_WRITE_FAILURE: 15,
  // Temporary failure code to see if there are failures without an update phase
  DWNLD_UNKNOWN_PHASE_ERR_WRITE_FAILURE: 40,

  /**
   * Submit a telemetry ping for the update download result code.
   *
   * @param  aIsComplete
   *         If true the histogram is for a patch type complete, if false the
   *         histogram is for a patch type partial, and when undefined the
   *         histogram is for an unknown patch type. This is used to determine
   *         the histogram ID out of the following histogram IDs:
   *         UPDATE_DOWNLOAD_CODE_COMPLETE
   *         UPDATE_DOWNLOAD_CODE_PARTIAL
   *         UPDATE_DOWNLOAD_CODE_UNKNOWN
   * @param  aCode
   *         An integer value as defined by the values that start with DWNLD_ in
   *         the above section.
   */
  pingDownloadCode: function UT_pingDownloadCode(aIsComplete, aCode) {
    let patchType = this.PATCH_UNKNOWN;
    if (aIsComplete === true) {
      patchType = this.PATCH_COMPLETE;
    } else if (aIsComplete === false) {
      patchType = this.PATCH_PARTIAL;
    }
    try {
      let id = "UPDATE_DOWNLOAD_CODE_" + patchType;
      // enumerated type histogram
      Services.telemetry.getHistogramById(id).add(aCode);
    } catch (e) {
      Cu.reportError(e);
    }
  },

  // Previous state codes are defined in pingStateAndStatusCodes() in
  // nsUpdateService.js
  STATE_WRITE_FAILURE: 14,

  /**
   * Submit a telemetry ping for the update status state code.
   *
   * @param  aSuffix
   *         The histogram id suffix for histogram IDs:
   *         UPDATE_STATE_CODE_COMPLETE_STARTUP
   *         UPDATE_STATE_CODE_PARTIAL_STARTUP
   *         UPDATE_STATE_CODE_UNKNOWN_STARTUP
   *         UPDATE_STATE_CODE_COMPLETE_STAGE
   *         UPDATE_STATE_CODE_PARTIAL_STAGE
   *         UPDATE_STATE_CODE_UNKNOWN_STAGE
   * @param  aCode
   *         An integer value as defined by the values that start with STATE_ in
   *         the above section for the update state from the update.status file.
   */
  pingStateCode: function UT_pingStateCode(aSuffix, aCode) {
    try {
      let id = "UPDATE_STATE_CODE_" + aSuffix;
      // enumerated type histogram
      Services.telemetry.getHistogramById(id).add(aCode);
    } catch (e) {
      Cu.reportError(e);
    }
  },

  /**
   * Submit a telemetry ping for the update status error code. This does not
   * submit a success value which can be determined from the state code.
   *
   * @param  aSuffix
   *         The histogram id suffix for histogram IDs:
   *         UPDATE_STATUS_ERROR_CODE_COMPLETE_STARTUP
   *         UPDATE_STATUS_ERROR_CODE_PARTIAL_STARTUP
   *         UPDATE_STATUS_ERROR_CODE_UNKNOWN_STARTUP
   *         UPDATE_STATUS_ERROR_CODE_COMPLETE_STAGE
   *         UPDATE_STATUS_ERROR_CODE_PARTIAL_STAGE
   *         UPDATE_STATUS_ERROR_CODE_UNKNOWN_STAGE
   * @param  aCode
   *         An integer value for the error code from the update.status file.
   */
  pingStatusErrorCode: function UT_pingStatusErrorCode(aSuffix, aCode) {
    try {
      let id = "UPDATE_STATUS_ERROR_CODE_" + aSuffix;
      // enumerated type histogram
      Services.telemetry.getHistogramById(id).add(aCode);
    } catch (e) {
      Cu.reportError(e);
    }
  },

  /**
   * Submit a telemetry ping for a failing binary transparency result.
   *
   * @param  aSuffix
   *         Key to use on the update.binarytransparencyresult collection.
   *         Must be one of "COMPLETE_STARTUP", "PARTIAL_STARTUP",
   *         "UNKNOWN_STARTUP", "COMPLETE_STAGE", "PARTIAL_STAGE",
   *         "UNKNOWN_STAGE".
   * @param  aCode
   *         An integer value for the error code from the update.bt file.
   */
  pingBinaryTransparencyResult: function UT_pingBinaryTransparencyResult(
    aSuffix,
    aCode
  ) {
    try {
      let id = "update.binarytransparencyresult";
      let key = aSuffix.toLowerCase().replace("_", "-");
      Services.telemetry.keyedScalarSet(id, key, aCode);
    } catch (e) {
      Cu.reportError(e);
    }
  },

  /**
   * Records a failed BITS update download using Telemetry.
   * In addition to the BITS Result histogram, this also sends an
   * update.bitshresult scalar value.
   *
   * @param aIsComplete
   *        If true the histogram is for a patch type complete, if false the
   *        histogram is for a patch type partial. This will determine the
   *        histogram id out of the following histogram ids:
   *        UPDATE_BITS_RESULT_COMPLETE
   *        UPDATE_BITS_RESULT_PARTIAL
   *        This value is also used to determine the key for the keyed scalar
   *        update.bitshresult (key is either "COMPLETE" or "PARTIAL")
   * @param aError
   *        The BitsError that occurred. See Bits.jsm for details on BitsError.
   */
  pingBitsError: function UT_pingBitsError(aIsComplete, aError) {
    if (AppConstants.platform != "win") {
      Cu.reportError(
        "Warning: Attempted to submit BITS telemetry on a " +
          "non-Windows platform"
      );
      return;
    }
    if (!(aError instanceof BitsError)) {
      Cu.reportError("Error sending BITS Error ping: Error is not a BitsError");
      aError = new BitsUnknownError();
    }
    // Coerce the error to integer
    let type = +aError.type;
    if (isNaN(type)) {
      Cu.reportError(
        "Error sending BITS Error ping: Either error is not a " +
          "BitsError, or error type is not an integer."
      );
      type = Ci.nsIBits.ERROR_TYPE_UNKNOWN;
    } else if (type == Ci.nsIBits.ERROR_TYPE_SUCCESS) {
      Cu.reportError(
        "Error sending BITS Error ping: The error type must not " +
          "be the success type."
      );
      type = Ci.nsIBits.ERROR_TYPE_UNKNOWN;
    }
    this._pingBitsResult(aIsComplete, type);

    if (aError.codeType == Ci.nsIBits.ERROR_CODE_TYPE_HRESULT) {
      let scalarKey;
      if (aIsComplete) {
        scalarKey = this.PATCH_COMPLETE;
      } else {
        scalarKey = this.PATCH_PARTIAL;
      }
      try {
        Services.telemetry.keyedScalarSet(
          "update.bitshresult",
          scalarKey,
          aError.code
        );
      } catch (e) {
        Cu.reportError(e);
      }
    }
  },

  /**
   * Records a successful BITS update download using Telemetry.
   *
   * @param aIsComplete
   *        If true the histogram is for a patch type complete, if false the
   *        histogram is for a patch type partial. This will determine the
   *        histogram id out of the following histogram ids:
   *        UPDATE_BITS_RESULT_COMPLETE
   *        UPDATE_BITS_RESULT_PARTIAL
   */
  pingBitsSuccess: function UT_pingBitsSuccess(aIsComplete) {
    if (AppConstants.platform != "win") {
      Cu.reportError(
        "Warning: Attempted to submit BITS telemetry on a " +
          "non-Windows platform"
      );
      return;
    }
    this._pingBitsResult(aIsComplete, Ci.nsIBits.ERROR_TYPE_SUCCESS);
  },

  /**
   * This is the helper function that does all the work for pingBitsError and
   * pingBitsSuccess. It submits a telemetry ping indicating the result of the
   * BITS update download.
   *
   * @param aIsComplete
   *        If true the histogram is for a patch type complete, if false the
   *        histogram is for a patch type partial. This will determine the
   *        histogram id out of the following histogram ids:
   *        UPDATE_BITS_RESULT_COMPLETE
   *        UPDATE_BITS_RESULT_PARTIAL
   * @param aResultType
   *        The result code. This will be one of the ERROR_TYPE_* values defined
   *        in the nsIBits interface.
   */
  _pingBitsResult: function UT_pingBitsResult(aIsComplete, aResultType) {
    let patchType;
    if (aIsComplete) {
      patchType = this.PATCH_COMPLETE;
    } else {
      patchType = this.PATCH_PARTIAL;
    }
    try {
      let id = "UPDATE_BITS_RESULT_" + patchType;
      Services.telemetry.getHistogramById(id).add(aResultType);
    } catch (e) {
      Cu.reportError(e);
    }
  },

  /**
   * Submit the interval in days since the last notification for this background
   * update check or a boolean if the last notification is in the future.
   *
   * @param  aSuffix
   *         The histogram id suffix for histogram IDs:
   *         UPDATE_INVALID_LASTUPDATETIME_EXTERNAL
   *         UPDATE_INVALID_LASTUPDATETIME_NOTIFY
   *         UPDATE_INVALID_LASTUPDATETIME_SUBSEQUENT
   *         UPDATE_LAST_NOTIFY_INTERVAL_DAYS_EXTERNAL
   *         UPDATE_LAST_NOTIFY_INTERVAL_DAYS_NOTIFY
   *         UPDATE_LAST_NOTIFY_INTERVAL_DAYS_SUBSEQUENT
   */
  pingLastUpdateTime: function UT_pingLastUpdateTime(aSuffix) {
    const PREF_APP_UPDATE_LASTUPDATETIME =
      "app.update.lastUpdateTime.background-update-timer";
    if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_LASTUPDATETIME)) {
      let lastUpdateTimeSeconds = Services.prefs.getIntPref(
        PREF_APP_UPDATE_LASTUPDATETIME
      );
      if (lastUpdateTimeSeconds) {
        let currentTimeSeconds = Math.round(Date.now() / 1000);
        if (lastUpdateTimeSeconds > currentTimeSeconds) {
          try {
            let id = "UPDATE_INVALID_LASTUPDATETIME_" + aSuffix;
            // count type histogram
            Services.telemetry.getHistogramById(id).add();
          } catch (e) {
            Cu.reportError(e);
          }
        } else {
          let intervalDays =
            (currentTimeSeconds - lastUpdateTimeSeconds) / (60 * 60 * 24);
          try {
            let id = "UPDATE_LAST_NOTIFY_INTERVAL_DAYS_" + aSuffix;
            // exponential type histogram
            Services.telemetry.getHistogramById(id).add(intervalDays);
          } catch (e) {
            Cu.reportError(e);
          }
        }
      }
    }
  },

  /**
   * Submit a telemetry ping for a boolean type histogram that indicates if the
   * service is installed and a telemetry ping for a boolean type histogram that
   * indicates if the service was at some point installed and is now
   * uninstalled.
   *
   * @param  aSuffix
   *         The histogram id suffix for histogram IDs:
   *         UPDATE_SERVICE_INSTALLED_EXTERNAL
   *         UPDATE_SERVICE_INSTALLED_NOTIFY
   *         UPDATE_SERVICE_INSTALLED_SUBSEQUENT
   *         UPDATE_SERVICE_MANUALLY_UNINSTALLED_EXTERNAL
   *         UPDATE_SERVICE_MANUALLY_UNINSTALLED_NOTIFY
   *         UPDATE_SERVICE_MANUALLY_UNINSTALLED_SUBSEQUENT
   * @param  aInstalled
   *         Whether the service is installed.
   */
  pingServiceInstallStatus: function UT_PSIS(aSuffix, aInstalled) {
    // Report the error but don't throw since it is more important to
    // successfully update than to throw.
    if (!("@mozilla.org/windows-registry-key;1" in Cc)) {
      Cu.reportError(Cr.NS_ERROR_NOT_AVAILABLE);
      return;
    }

    try {
      let id = "UPDATE_SERVICE_INSTALLED_" + aSuffix;
      // boolean type histogram
      Services.telemetry.getHistogramById(id).add(aInstalled);
    } catch (e) {
      Cu.reportError(e);
    }

    let attempted = 0;
    try {
      let wrk = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
        Ci.nsIWindowsRegKey
      );
      wrk.open(
        wrk.ROOT_KEY_LOCAL_MACHINE,
        "SOFTWARE\\Mozilla\\MaintenanceService",
        wrk.ACCESS_READ | wrk.WOW64_64
      );
      // Was the service at some point installed, but is now uninstalled?
      attempted = wrk.readIntValue("Attempted");
      wrk.close();
    } catch (e) {
      // Since this will throw if the registry key doesn't exist (e.g. the
      // service has never been installed) don't report an error.
    }

    try {
      let id = "UPDATE_SERVICE_MANUALLY_UNINSTALLED_" + aSuffix;
      if (!aInstalled && attempted) {
        // count type histogram
        Services.telemetry.getHistogramById(id).add();
      }
    } catch (e) {
      Cu.reportError(e);
    }
  },

  /**
   * Submit a telemetry ping for a count type histogram when the expected value
   * does not equal the boolean value of a pref or if the pref isn't present
   * when the expected value does not equal default value. This lessens the
   * amount of data submitted to telemetry.
   *
   * @param  aID
   *         The histogram ID to report to.
   * @param  aPref
   *         The preference to check.
   * @param  aDefault
   *         The default value when the preference isn't present.
   * @param  aExpected (optional)
   *         If specified and the value is the same as the value that will be
   *         added the value won't be added to telemetry.
   */
  pingBoolPref: function UT_pingBoolPref(aID, aPref, aDefault, aExpected) {
    try {
      let val = aDefault;
      if (Services.prefs.getPrefType(aPref) != Ci.nsIPrefBranch.PREF_INVALID) {
        val = Services.prefs.getBoolPref(aPref);
      }
      if (val != aExpected) {
        // count type histogram
        Services.telemetry.getHistogramById(aID).add();
      }
    } catch (e) {
      Cu.reportError(e);
    }
  },

  /**
   * Submit a telemetry ping for a histogram with the integer value of a
   * preference when it is not the expected value or the default value when it
   * is not the expected value. This lessens the amount of data submitted to
   * telemetry.
   *
   * @param  aID
   *         The histogram ID to report to.
   * @param  aPref
   *         The preference to check.
   * @param  aDefault
   *         The default value when the pref is not set.
   * @param  aExpected (optional)
   *         If specified and the value is the same as the value that will be
   *         added the value won't be added to telemetry.
   */
  pingIntPref: function UT_pingIntPref(aID, aPref, aDefault, aExpected) {
    try {
      let val = aDefault;
      if (Services.prefs.getPrefType(aPref) != Ci.nsIPrefBranch.PREF_INVALID) {
        val = Services.prefs.getIntPref(aPref);
      }
      if (aExpected === undefined || val != aExpected) {
        // enumerated or exponential type histogram
        Services.telemetry.getHistogramById(aID).add(val);
      }
    } catch (e) {
      Cu.reportError(e);
    }
  },

  /**
   * Submit a telemetry ping for all histogram types that take a single
   * parameter to the telemetry add function and the count type histogram when
   * the aExpected parameter is specified. If the aExpected parameter is
   * specified and it equals the value specified by the aValue
   * parameter the telemetry submission will be skipped.
   *
   * @param  aID
   *         The histogram ID to report to.
   * @param  aValue
   *         The value to add when aExpected is not defined or the value to
   *         check if it is equal to when aExpected is defined.
   * @param  aExpected (optional)
   *         If specified and the value is the same as the value specified by
   *         aValue parameter the submission will be skipped.
   */
  pingGeneric: function UT_pingGeneric(aID, aValue, aExpected) {
    try {
      if (aExpected === undefined) {
        Services.telemetry.getHistogramById(aID).add(aValue);
      } else if (aValue != aExpected) {
        // count type histogram
        Services.telemetry.getHistogramById(aID).add();
      }
    } catch (e) {
      Cu.reportError(e);
    }
  },

  /**
   * Valid keys for the update.moveresult scalar.
   */
  MOVE_RESULT_SUCCESS: "SUCCESS",
  MOVE_RESULT_UNKNOWN_FAILURE: "UNKNOWN_FAILURE",

  /**
   * Reports the passed result of attempting to move the downloading update
   * into the ready update directory.
   */
  pingMoveResult: function UT_pingMoveResult(aResult) {
    Services.telemetry.keyedScalarAdd("update.move_result", aResult, 1);
  },
};
Object.freeze(AUSTLMY);
