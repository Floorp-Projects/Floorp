/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["AbuseReporter", "AbuseReportError"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const PREF_ABUSE_REPORT_URL = "extensions.abuseReport.url";
const PREF_AMO_DETAILS_API_URL = "extensions.abuseReport.amoDetailsURL";

// Name associated with the report dialog window.
const DIALOG_WINDOW_NAME = "addons-abuse-report-dialog";

// Maximum length of the string properties sent to the API endpoint.
const MAX_STRING_LENGTH = 255;

// Minimum time between report submissions (in ms).
const MIN_MS_BETWEEN_SUBMITS = 30000;

// The addon types currently supported by the integrated abuse report panel.
const SUPPORTED_ADDON_TYPES = ["extension", "theme", "sitepermission"];

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AMTelemetry: "resource://gre/modules/AddonManager.jsm",
  ClientID: "resource://gre/modules/ClientID.jsm",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "ABUSE_REPORT_URL",
  PREF_ABUSE_REPORT_URL
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "AMO_DETAILS_API_URL",
  PREF_AMO_DETAILS_API_URL
);

const PRIVATE_REPORT_PROPS = Symbol("privateReportProps");

const ERROR_TYPES = Object.freeze([
  "ERROR_ABORTED_SUBMIT",
  "ERROR_ADDON_NOTFOUND",
  "ERROR_CLIENT",
  "ERROR_NETWORK",
  "ERROR_UNKNOWN",
  "ERROR_RECENT_SUBMIT",
  "ERROR_SERVER",
  "ERROR_AMODETAILS_NOTFOUND",
  "ERROR_AMODETAILS_FAILURE",
]);

class AbuseReportError extends Error {
  constructor(errorType, errorInfo = undefined) {
    if (!ERROR_TYPES.includes(errorType)) {
      throw new Error(`Unknown AbuseReportError type "${errorType}"`);
    }

    let message = errorInfo ? `${errorType} - ${errorInfo}` : errorType;

    super(message);
    this.name = "AbuseReportError";
    this.errorType = errorType;
    this.errorInfo = errorInfo;
  }
}

/**
 * Create an error info string from a fetch response object.
 *
 * @param {Response} response
 *        A fetch response object to convert into an errorInfo string.
 *
 * @returns {Promise<string>}
 *          The errorInfo string to be included in an AbuseReportError.
 */
async function responseToErrorInfo(response) {
  return JSON.stringify({
    status: response.status,
    responseText: await response.text().catch(err => ""),
  });
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

  isSupportedAddonType(addonType) {
    return SUPPORTED_ADDON_TYPES.includes(addonType);
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
   * @returns {Promise<AbuseReport>}
   *          Returns a promise that resolves to an instance of the AbuseReport
   *          class, which represent an ongoing report.
   */
  async createAbuseReport(addonId, { reportEntryPoint } = {}) {
    let addon = await lazy.AddonManager.getAddonByID(addonId);

    if (!addon) {
      // The addon isn't installed, query the details from the AMO API endpoint.
      addon = await this.queryAMOAddonDetails(addonId, reportEntryPoint);
    }

    if (!addon) {
      lazy.AMTelemetry.recordReportEvent({
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
   * Retrieves the addon details from the AMO API endpoint, used to create
   * abuse reports on non-installed addon-ons.
   *
   * For the addon details that may be translated (e.g. addon name, description etc.)
   * the function will try to retrieve the string localized in the same locale used
   * by Gecko (and fallback to "en-US" if that locale is unavailable).
   *
   * The addon creator properties are set to the first author available.
   *
   * @param {string} addonId
   *        The id of the addon to retrieve the details available on AMO.
   * @param {string} reportEntryPoint
   *        The entry point for the report flow (to be included in the telemetry
   *        recorded in case of failures).
   *
   * @returns {Promise<AMOAddonDetails|null>}
   *          Returns a promise that resolves to an AMOAddonDetails object,
   *          which has the subset of the AddonWrapper properties which are
   *          needed by the abuse report panel or the report data sent to
   *          the abuse report API endpoint), or null if it fails to
   *          retrieve the details from AMO.
   *
   * @typedef {object} AMOAddonDetails
   *   @prop  {string} id
   *   @prop  {string} name
   *   @prop  {string} version
   *   @prop  {string} description
   *   @prop  {string} type
   *   @prop  {string} iconURL
   *   @prop  {string} homepageURL
   *   @prop  {string} supportURL
   *   @prop  {AMOAddonCreator} creator
   *   @prop  {boolean} isRecommended
   *   @prop  {number} signedState=AddonManager.SIGNEDSTATE_UNKNOWN
   *   @prop  {object} installTelemetryInfo={ source: "not_installed" }
   *
   * @typedef {object} AMOAddonCreator
   *   @prop  {string} name
   *   @prop  {string} url
   */
  async queryAMOAddonDetails(addonId, reportEntryPoint) {
    let details;
    try {
      // This should be the API endpoint documented at:
      // https://addons-server.readthedocs.io/en/latest/topics/api/addons.html#detail
      details = await fetch(`${lazy.AMO_DETAILS_API_URL}/${addonId}`, {
        credentials: "omit",
        referrerPolicy: "no-referrer",
        headers: { "Content-Type": "application/json" },
      }).then(async response => {
        if (response.status === 200) {
          return response.json();
        }

        let errorInfo = await responseToErrorInfo(response).catch(
          err => undefined
        );

        if (response.status === 404) {
          // Record a different telemetry event for 404 errors.
          throw new AbuseReportError("ERROR_AMODETAILS_NOTFOUND", errorInfo);
        }

        throw new AbuseReportError("ERROR_AMODETAILS_FAILURE", errorInfo);
      });
    } catch (err) {
      // Log the original error in the browser console.
      Cu.reportError(err);

      lazy.AMTelemetry.recordReportEvent({
        addonId,
        errorType: err.errorType || "ERROR_AMODETAILS_FAILURE",
        reportEntryPoint,
      });

      return null;
    }

    const locale = Services.locale.appLocaleAsBCP47;

    // Get a string value from a translated value
    // (https://addons-server.readthedocs.io/en/latest/topics/api/overview.html#api-overview-translations)
    const getTranslatedValue = value => {
      if (typeof value === "string") {
        return value;
      }
      return value && (value[locale] || value["en-US"]);
    };

    const getAuthorField = fieldName =>
      details.authors && details.authors[0] && details.authors[0][fieldName];

    // Normalize type "statictheme" (which is the type used on the AMO API side)
    // into "theme" (because it is the type we use and expect on the Firefox side
    // for this addon type).
    const addonType = details.type === "statictheme" ? "theme" : details.type;

    return {
      id: addonId,
      name: getTranslatedValue(details.name),
      version: details.current_version.version,
      description: getTranslatedValue(details.summary),
      type: addonType,
      iconURL: details.icon_url,
      homepageURL: getTranslatedValue(details.homepage),
      supportURL: getTranslatedValue(details.support_url),
      // Set the addon creator to the first author in the AMO details.
      creator: {
        name: getAuthorField("name"),
        url: getAuthorField("url"),
      },
      isRecommended: details.is_recommended,
      // Set signed state to unknown because it isn't installed.
      signedState: lazy.AddonManager.SIGNEDSTATE_UNKNOWN,
      // Set the installTelemetryInfo.source to "not_installed".
      installTelemetryInfo: { source: "not_installed" },
    };
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
    const truncateString = text =>
      typeof text == "string" ? text.slice(0, MAX_STRING_LENGTH) : text;

    // Normalize addon_install_source and addon_install_method values
    // as expected by the server API endpoint. Returns null if the
    // value is not a string.
    const normalizeValue = text =>
      typeof text == "string"
        ? text.toLowerCase().replace(/[- :]/g, "_")
        : null;

    const installInfo = addon.installTelemetryInfo || {};

    const data = {
      addon: addon.id,
      addon_version: addon.version,
      addon_name: truncateString(addon.name),
      addon_summary: truncateString(addon.description),
      addon_install_origin:
        addon.sourceURI && truncateString(addon.sourceURI.spec),
      install_date: addon.installDate && addon.installDate.toISOString(),
      addon_install_source: normalizeValue(installInfo.source),
      addon_install_source_url:
        installInfo.sourceURL && truncateString(installInfo.sourceURL),
      addon_install_method: normalizeValue(installInfo.method),
    };

    switch (addon.signedState) {
      case lazy.AddonManager.SIGNEDSTATE_BROKEN:
        data.addon_signature = "broken";
        break;
      case lazy.AddonManager.SIGNEDSTATE_UNKNOWN:
        data.addon_signature = "unknown";
        break;
      case lazy.AddonManager.SIGNEDSTATE_MISSING:
        data.addon_signature = "missing";
        break;
      case lazy.AddonManager.SIGNEDSTATE_PRELIMINARY:
        data.addon_signature = "preliminary";
        break;
      case lazy.AddonManager.SIGNEDSTATE_SIGNED:
        data.addon_signature = "signed";
        break;
      case lazy.AddonManager.SIGNEDSTATE_SYSTEM:
        data.addon_signature = "system";
        break;
      case lazy.AddonManager.SIGNEDSTATE_PRIVILEGED:
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

    data.client_id = await lazy.ClientID.getClientIdHash();

    data.app = Services.appinfo.name.toLowerCase();
    data.appversion = Services.appinfo.version;
    data.lang = Services.locale.appLocaleAsBCP47;
    data.operating_system = AppConstants.platform;
    data.operating_system_version = Services.sysinfo.getProperty("version");

    return data;
  },

  /**
   * Helper function that returns a reference to a report dialog window
   * already opened (if any).
   *
   * @returns {Window?}
   */
  getOpenDialog() {
    return Services.ww.getWindowByName(DIALOG_WINDOW_NAME, null);
  },

  /**
   * Helper function that opens an abuse report form in a new dialog window.
   *
   * @param {string} addonId
   *        The addonId being reported.
   * @param {string} reportEntryPoint
   *        The entry point from which the user has triggered the abuse report
   *        flow.
   * @param {XULElement} browser
   *        The browser element (if any) that is opening the report window.
   *
   * @return {Promise<AbuseReportDialog>}
   *         Returns an AbuseReportDialog object, rejects if it fails to open
   *         the dialog.
   *
   * @typedef {object}                        AbuseReportDialog
   *          An object that represents the abuse report dialog.
   * @prop    {function}                      close
   *          A method that closes the report dialog (used by the caller
   *          to close the dialog when the user chooses to close the window
   *          that started the abuse report flow).
   * @prop    {Promise<AbuseReport|undefined} promiseReport
   *          A promise resolved to an AbuseReport instance if the report should
   *          be submitted, or undefined if the user has cancelled the report.
   *          Rejects if it fails to create an AbuseReport instance or to open
   *          the abuse report window.
   */
  async openDialog(addonId, reportEntryPoint, browser) {
    const chromeWin = browser && browser.ownerGlobal;
    if (!chromeWin) {
      throw new Error("Abuse Reporter dialog cancelled, opener tab closed");
    }

    const dialogWin = this.getOpenDialog();

    if (dialogWin) {
      // If an abuse report dialog is already open, cancel the
      // previous report flow and start a new one.
      const {
        deferredReport,
        promiseReport,
      } = dialogWin.arguments[0].wrappedJSObject;
      deferredReport.resolve({ userCancelled: true });
      await promiseReport;
    }

    const report = await AbuseReporter.createAbuseReport(addonId, {
      reportEntryPoint,
    });

    if (!SUPPORTED_ADDON_TYPES.includes(report.addon.type)) {
      throw new Error(
        `Addon type "${report.addon.type}" is not currently supported by the integrated abuse reporting feature`
      );
    }

    const params = Cc["@mozilla.org/array;1"].createInstance(
      Ci.nsIMutableArray
    );

    const dialogInit = {
      report,
      openWebLink(url) {
        chromeWin.openWebLinkIn(url, "tab", {
          relatedToCurrent: true,
        });
      },
    };

    params.appendElement(dialogInit);

    let win;
    function closeDialog() {
      if (win && !win.closed) {
        win.close();
      }
    }

    const promiseReport = new Promise((resolve, reject) => {
      dialogInit.deferredReport = { resolve, reject };
    }).then(
      ({ userCancelled }) => {
        closeDialog();
        return userCancelled ? undefined : report;
      },
      err => {
        Cu.reportError(
          `Unexpected abuse report panel error: ${err} :: ${err.stack}`
        );
        closeDialog();
        return Promise.reject({
          message: "Unexpected abuse report panel error",
        });
      }
    );

    const promiseReportPanel = new Promise((resolve, reject) => {
      dialogInit.deferredReportPanel = { resolve, reject };
    });

    dialogInit.promiseReport = promiseReport;
    dialogInit.promiseReportPanel = promiseReportPanel;

    win = Services.ww.openWindow(
      chromeWin,
      "chrome://mozapps/content/extensions/abuse-report-frame.html",
      DIALOG_WINDOW_NAME,
      // Set the dialog window options (including a reasonable initial
      // window height size, eventually adjusted by the panel once it
      // has been rendered its content).
      "dialog,centerscreen,height=700",
      params
    );

    return {
      close: closeDialog,
      promiseReport,

      // Properties used in tests
      promiseReportPanel,
      window: win,
    };
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
  constructor({ addon, createErrorType, reportData, reportEntryPoint }) {
    this[PRIVATE_REPORT_PROPS] = {
      aborted: false,
      abortController: new AbortController(),
      addon,
      reportData,
      reportEntryPoint,
      // message and reason are initially null, and then set by the panel
      // using the related set method.
      message: null,
      reason: null,
    };
  }

  recordTelemetry(errorType) {
    const { addon, reportEntryPoint } = this;
    lazy.AMTelemetry.recordReportEvent({
      addonId: addon.id,
      addonType: addon.type,
      errorType,
      reportEntryPoint,
    });
  }

  /**
   * Submit the current report, given a reason and a message.
   *
   * @returns {Promise<void>}
   *          Resolves once the report has been successfully submitted.
   *          It rejects with an AbuseReportError if the report couldn't be
   *          submitted for a known reason (or another Error type otherwise).
   */
  async submit() {
    const {
      aborted,
      abortController,
      message,
      reason,
      reportData,
      reportEntryPoint,
    } = this[PRIVATE_REPORT_PROPS];

    // Record telemetry event and throw an AbuseReportError.
    const rejectReportError = async (errorType, { response } = {}) => {
      this.recordTelemetry(errorType);

      // Leave errorInfo empty if there is no response or fails to
      // be converted into an error info object.
      const errorInfo = response
        ? await responseToErrorInfo(response).catch(err => undefined)
        : undefined;

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
      response = await fetch(lazy.ABUSE_REPORT_URL, {
        signal: abortController.signal,
        method: "POST",
        credentials: "omit",
        referrerPolicy: "no-referrer",
        headers: { "Content-Type": "application/json" },
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
      return rejectReportError("ERROR_CLIENT", { response });
    }

    if (response.status >= 500 && response.status < 600) {
      return rejectReportError("ERROR_SERVER", { response });
    }

    // We got an unexpected HTTP status code.
    return rejectReportError("ERROR_UNKNOWN", { response });
  }

  /**
   * Abort the report submission.
   */
  abort() {
    const { abortController } = this[PRIVATE_REPORT_PROPS];
    abortController.abort();
    this[PRIVATE_REPORT_PROPS].aborted = true;
  }

  get addon() {
    return this[PRIVATE_REPORT_PROPS].addon;
  }

  get reportEntryPoint() {
    return this[PRIVATE_REPORT_PROPS].reportEntryPoint;
  }

  /**
   * Set the open message (called from the panel when the user submit the report)
   *
   * @parm {string} message
   *         An optional string which contains a description for the reported issue.
   */
  setMessage(message) {
    this[PRIVATE_REPORT_PROPS].message = message;
  }

  /**
   * Set the report reason (called from the panel when the user submit the report)
   *
   * @parm {string} reason
   *       String identifier for the report reason.
   */
  setReason(reason) {
    this[PRIVATE_REPORT_PROPS].reason = reason;
  }
}
