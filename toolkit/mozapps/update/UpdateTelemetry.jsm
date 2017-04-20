/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "AUSTLMY"
];

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm", this);

this.AUSTLMY = {
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

  /**
   * Values for the UPDATE_CHECK_CODE_NOTIFY and UPDATE_CHECK_CODE_EXTERNAL
   * Telemetry histograms.
   */
  // No update found (no notification)
  CHK_NO_UPDATE_FOUND: 0,
  // Update will be downloaded in the background (background download)
  CHK_DOWNLOAD_UPDATE: 1,
  // Showing prompt due to the update.xml specifying showPrompt
  // (update notification)
  CHK_SHOWPROMPT_SNIPPET: 2,
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
  // Background checks disabled by preference (no notification)
  CHK_PREF_DISABLED: 12,
  // Update checks disabled by admin locked preference (no notification)
  CHK_ADMIN_DISABLED: 13,
  // Unable to check for updates per hasUpdateMutex() (no notification)
  CHK_NO_MUTEX: 14,
  // Unable to check for updates per gCanCheckForUpdates (no notification). This
  // should be covered by other codes and is recorded just in case.
  CHK_UNABLE_TO_CHECK: 15,
  // Background checks disabled for the current session (no notification)
  CHK_DISABLED_FOR_SESSION: 16,
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
  // Update found for a version with the never preference set (no notification)
  CHK_UPDATE_NEVER_PREF: 26,
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
  // Invalid url for app.update.url default preference (no notification)
  CHK_INVALID_DEFAULT_URL: 32,
  // Update elevation failures or cancelations threshold reached for this
  // version, OSX only (no notification)
  CHK_ELEVATION_DISABLED_FOR_VERSION: 35,
  // User opted out of elevated updates for the available update version, OSX
  // only (no notification)
  CHK_ELEVATION_OPTOUT_FOR_VERSION: 36,

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
   *         UPDATE_CHECK_NO_UPDATE_EXTERNAL
   *         UPDATE_CHECK_NO_UPDATE_NOTIFY
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
   *         UPDATE_CHK_EXTENDED_ERROR_EXTERNAL
   *         UPDATE_CHK_EXTENDED_ERROR_NOTIFY
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
   * Values for the UPDATE_DOWNLOAD_CODE_COMPLETE and
   * UPDATE_DOWNLOAD_CODE_PARTIAL Telemetry histograms.
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
  DWNLD_ERR_VERIFY_NO_HASH_MATCH: 15,

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
   * Submit the interval in days since the last notification for this background
   * update check or a boolean if the last notification is in the future.
   *
   * @param  aSuffix
   *         The histogram id suffix for histogram IDs:
   *         UPDATE_INVALID_LASTUPDATETIME_EXTERNAL
   *         UPDATE_INVALID_LASTUPDATETIME_NOTIFY
   *         UPDATE_LAST_NOTIFY_INTERVAL_DAYS_EXTERNAL
   *         UPDATE_LAST_NOTIFY_INTERVAL_DAYS_NOTIFY
   */
  pingLastUpdateTime: function UT_pingLastUpdateTime(aSuffix) {
    const PREF_APP_UPDATE_LASTUPDATETIME = "app.update.lastUpdateTime.background-update-timer";
    if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_LASTUPDATETIME)) {
      let lastUpdateTimeSeconds = Services.prefs.getIntPref(PREF_APP_UPDATE_LASTUPDATETIME);
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
          let intervalDays = (currentTimeSeconds - lastUpdateTimeSeconds) /
                             (60 * 60 * 24);
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
   * Submit a telemetry ping for the last page displayed by the update wizard.
   *
   * @param  aPageID
   *         The page id for the last page displayed.
   */
  pingWizLastPageCode: function UT_pingWizLastPageCode(aPageID) {
    let pageMap = { invalid: 0,
                    dummy: 1,
                    checking: 2,
                    pluginupdatesfound: 3,
                    noupdatesfound: 4,
                    manualUpdate: 5,
                    unsupported: 6,
                    updatesfoundbasic: 8,
                    updatesfoundbillboard: 9,
                    downloading: 12,
                    errors: 13,
                    errorextra: 14,
                    errorpatching: 15,
                    finished: 16,
                    finishedBackground: 17,
                    installed: 18 };
    try {
      let id = "UPDATE_WIZ_LAST_PAGE_CODE";
      // enumerated type histogram
      Services.telemetry.getHistogramById(id).add(pageMap[aPageID] ||
                                                  pageMap.invalid);
    } catch (e) {
      Cu.reportError(e);
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
   *         UPDATE_SERVICE_MANUALLY_UNINSTALLED_EXTERNAL
   *         UPDATE_SERVICE_MANUALLY_UNINSTALLED_NOTIFY
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
      let wrk = Cc["@mozilla.org/windows-registry-key;1"].
                createInstance(Ci.nsIWindowsRegKey);
      wrk.open(wrk.ROOT_KEY_LOCAL_MACHINE,
               "SOFTWARE\\Mozilla\\MaintenanceService",
               wrk.ACCESS_READ | wrk.WOW64_64);
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
  }
};
Object.freeze(AUSTLMY);
