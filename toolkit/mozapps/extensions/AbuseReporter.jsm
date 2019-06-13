/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = [ "AbuseReporter", "AbuseReportError" ];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.importGlobalProperties(["fetch"]);

const PREF_ABUSE_REPORT_URL  = "extensions.abuseReport.url";

// Maximum length of the string properties sent to the API endpoint.
const MAX_STRING_LENGTH = 255;

// Minimum time between report submissions (in ms).
const MIN_MS_BETWEEN_SUBMITS = 30000;

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AMTelemetry: "resource://gre/modules/AddonManager.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  ClientID: "resource://gre/modules/ClientID.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyPreferenceGetter(this, "ABUSE_REPORT_URL", PREF_ABUSE_REPORT_URL);

const PRIVATE_REPORT_PROPS = Symbol("privateReportProps");

const ERROR_TYPES = Object.freeze([
  "ERROR_ABORTED_SUBMIT",
  "ERROR_ADDON_NOTFOUND",
  "ERROR_CLIENT",
  "ERROR_NETWORK",
  "ERROR_UNKNOWN",
  "ERROR_RECENT_SUBMIT",
  "ERROR_SERVER",
]);

class AbuseReportError extends Error {
  constructor(errorType, errorInfo = undefined) {
    if (!ERROR_TYPES.includes(errorType)) {
      throw new Error(`Unknown AbuseReportError type "${errorType}"`);
    }

    let message = errorInfo ?
      `${errorType} - ${errorInfo}` : errorType;

    super(message);
    this.name = "AbuseReportError";
    this.errorType = errorType;
    this.errorInfo = errorInfo;
  }
}

/**
 * A singleton object used to create new AbuseReport instances for a given addonId
 * and enforce a minium amount of time between two report submissions .
 */
const AbuseReporter = {
  _lastReportTimestamp: null,

  // Error types.
  updateLastReportTimestamp() {
    this._lastReportTimestamp = Date.now();
  },

  getTimeFromLastReport() {
    const currentTimestamp = Date.now();
    if (this._lastReportTimestamp > currentTimestamp) {
      // Reset the last report timestamp if it is in the future.
      this._lastReportTimestamp = null;
    }

    if (!this._lastReportTimestamp) {
      return Infinity;
    }

    return currentTimestamp - this._lastReportTimestamp;
  },

  /**
   * Create an AbuseReport instance, given the addonId and a reportEntryPoint.
   *
   * @param {string} addonId
   *        The id of the addon to create the report instance for.
   * @param {object} options
   * @param {string} options.reportEntryPoint
   *        An identifier that represent the entry point for the report flow.
   *
   * @returns {AbuseReport}
   *          An instance of the AbuseReport class, which represent an ongoing
   *          report.
   */
  async createAbuseReport(addonId, {reportEntryPoint} = {}) {
    const addon = await AddonManager.getAddonByID(addonId);

    if (!addon) {
      AMTelemetry.recordReportEvent({
        addonId,
        errorType: "ERROR_ADDON_NOTFOUND",
        reportEntryPoint,
      });
      throw new AbuseReportError("ERROR_ADDON_NOTFOUND");
    }

    const reportData = await this.getReportData(addon);

    return new AbuseReport({
      addon,
      reportData,
      reportEntryPoint,
    });
  },

  /**
   * Helper function that retrieves from an addon object all the data to send
   * as part of the submission request, besides the `reason`, `message` which are
   * going to be received from the submit method of the report object returned
   * by `createAbuseReport`.
   * (See https://addons-server.readthedocs.io/en/latest/topics/api/abuse.html)
   *
   * @param {AddonWrapper} addon
   *        The addon object to collect the detail from.
   *
   * @return {object}
   *         An object that contains the collected details.
   */
  async getReportData(addon) {
    const truncateString = (text) =>
      typeof text == "string" ? text.slice(0, MAX_STRING_LENGTH) : text;

    const data = {
      addon: addon.id,
      addon_version: addon.version,
      addon_name: truncateString(addon.name),
      addon_summary: truncateString(addon.description),
      addon_install_origin: addon.sourceURI && truncateString(addon.sourceURI.spec),
      install_date: addon.installDate && addon.installDate.toISOString(),
    };

    // Map addon.installTelemetryInfo values to the supported addon_install_method
    // values supported by the API endpoint (See API endpoint docs at
    // https://addons-server.readthedocs.io/en/latest/topics/api/abuse.html).
    let install_method = "other";
    if (addon.installTelemetryInfo) {
      const {source, method} = addon.installTelemetryInfo;
      switch (source) {
        case "enterprise-policy":
        case "file-url":
        case "system-addon":
        case "temporary-addon":
          install_method = source.replace(/-/g, "_");
          break;
        case "distribution":
        case "sync":
          install_method = source;
          break;
        default:
          install_method = "other";
      }

      switch (method) {
        case "sideload":
        case "link":
          install_method = method;
          break;
        case "amWebAPI":
        case "installTrigger":
          install_method = method.toLowerCase();
          break;
        case "drag-and-drop":
        case "install-from-file":
        case "management-webext-api":
          install_method = method.replace(/-/g, "_");
          break;
      }
    }
    data.addon_install_method = install_method;

    switch (addon.signedState) {
      case AddonManager.SIGNEDSTATE_BROKEN:
        data.addon_signature = "broken";
        break;
      case AddonManager.SIGNEDSTATE_UNKNOWN:
        data.addon_signature = "unknown";
        break;
      case AddonManager.SIGNEDSTATE_MISSING:
        data.addon_signature = "missing";
        break;
      case AddonManager.SIGNEDSTATE_PRELIMINARY:
        data.addon_signature = "preliminary";
        break;
      case AddonManager.SIGNEDSTATE_SIGNED:
        data.addon_signature = "signed";
        break;
      case AddonManager.SIGNEDSTATE_SYSTEM:
        data.addon_signature = "system";
        break;
      case AddonManager.SIGNEDSTATE_PRIVILEGED:
        data.addon_signature = "privileged";
        break;
      default:
        data.addon_signature = `unknown: ${addon.signedState}`;
    }

    // Set "curated" as addon_signature on recommended addons
    // (addon.isRecommended internally checks that the addon is also
    // signed correctly).
    if (addon.isRecommended) {
      data.addon_signature = "curated";
    }

    data.client_id = await ClientID.getClientIdHash();

    data.app = Services.appinfo.name.toLowerCase();
    data.appversion = Services.appinfo.version;
    data.lang = Services.locale.appLocaleAsLangTag;
    data.operating_system = AppConstants.platform;
    data.operating_system_version = Services.sysinfo.getProperty("version");

    return data;
  },
};

/**
 * Represents an ongoing abuse report. Instances of this class are created
 * by the `AbuseReporter.createAbuseReport` method.
 *
 * This object is used by the reporting UI panel and message bars to:
 *
 * - get an errorType in case of a report creation error (e.g. because of a
 *   previously submitted report)
 * - get the addon details used inside the reporting panel
 * - submit the abuse report (and re-submit if a previous submission failed
 *   and the user choose to retry to submit it again)
 * - abort an ongoing submission
 *
 * @param {object}            options
 * @param {AddonWrapper|null} options.addon
 *        AddonWrapper instance for the extension/theme being reported.
 *        (May be null if the extension has not been found).
 * @param {object|null}       options.reportData
 *        An object which contains addon and environment details to send as part of a submission
 *        (may be null if the report has a createErrorType).
 * @param {string}            options.reportEntryPoint
 *        A string that identify how the report has been triggered.
 */
class AbuseReport {
  constructor({addon, createErrorType, reportData, reportEntryPoint}) {
    this[PRIVATE_REPORT_PROPS] = {
      aborted: false,
      abortController: new AbortController(),
      addon,
      reportData,
      reportEntryPoint,
    };
  }

  recordTelemetry(errorType) {
    const {addon, reportEntryPoint} = this;
    AMTelemetry.recordReportEvent({
      addonId: addon.id,
      addonType: addon.type,
      errorType,
      reportEntryPoint,
    });
  }

  /**
   * Submit the current report, given a reason and a message.
   *
   * @params {object} options
   * @params {string} options.reason
   *         String identifier for the report reason.
   * @params {string} [options.message]
   *         An optional string which contains a description for the reported issue.
   *
   * @returns {Promise<void>}
   *          Resolves once the report has been successfully submitted.
   *          It rejects with an AbuseReportError if the report couldn't be
   *          submitted for a known reason (or another Error type otherwise).
   */
  async submit({reason, message}) {
    const {
      aborted, abortController,
      reportData,
      reportEntryPoint,
    } = this[PRIVATE_REPORT_PROPS];

    // Record telemetry event and throw an AbuseReportError.
    const rejectReportError = async (errorType, {response} = {}) => {
      this.recordTelemetry(errorType);

      let errorInfo;
      if (response) {
        try {
          errorInfo = JSON.stringify({
            status: response.status,
            responseText: await response.text().catch(err => ""),
          });
        } catch (err) {
          // leave the errorInfo empty if we failed to stringify it.
        }
      }
      throw new AbuseReportError(errorType, errorInfo);
    };

    if (aborted) {
      // Report aborted before being actually submitted.
      return rejectReportError("ERROR_ABORTED_SUBMIT");
    }

    // Prevent submit of a new abuse report in less than MIN_MS_BETWEEN_SUBMITS.
    let msFromLastReport = AbuseReporter.getTimeFromLastReport();
    if (msFromLastReport < MIN_MS_BETWEEN_SUBMITS) {
      return rejectReportError("ERROR_RECENT_SUBMIT");
    }

    let response;
    try {
      response = await fetch(ABUSE_REPORT_URL, {
        signal: abortController.signal,
        method: "POST",
        credentials: "omit",
        referrerPolicy: "no-referrer",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify({
          ...reportData,
          report_entry_point: reportEntryPoint,
          message,
          reason,
        }),
      });
    } catch (err) {
      if (err.name === "AbortError") {
        return rejectReportError("ERROR_ABORTED_SUBMIT");
      }
      Cu.reportError(err);
      return rejectReportError("ERROR_NETWORK");
    }

    if (response.ok && response.status >= 200 && response.status < 400) {
      // Ensure that the response is also a valid json format.
      try {
        await response.json();
      } catch (err) {
        this.recordTelemetry("ERROR_UNKNOWN");
        throw err;
      }
      AbuseReporter.updateLastReportTimestamp();
      this.recordTelemetry();
      return undefined;
    }

    if (response.status >= 400 && response.status < 500) {
      return rejectReportError("ERROR_CLIENT", {response});
    }

    if (response.status >= 500 && response.status < 600) {
      return rejectReportError("ERROR_SERVER", {response});
    }

    // We got an unexpected HTTP status code.
    return rejectReportError("ERROR_UNKNOWN", {response});
  }

  /**
   * Abort the report submission.
   */
  abort() {
    const {abortController} = this[PRIVATE_REPORT_PROPS];
    abortController.abort();
    this[PRIVATE_REPORT_PROPS].aborted = true;
  }

  get addon() {
    return this[PRIVATE_REPORT_PROPS].addon;
  }

  get reportEntryPoint() {
    return this[PRIVATE_REPORT_PROPS].reportEntryPoint;
  }
}
