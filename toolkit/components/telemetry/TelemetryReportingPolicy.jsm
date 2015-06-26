/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "TelemetryReportingPolicy"
];

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/Timer.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://services-common/observers.js", this);

XPCOMUtils.defineLazyModuleGetter(this, "TelemetrySend",
                                  "resource://gre/modules/TelemetrySend.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UpdateChannel",
                                  "resource://gre/modules/UpdateChannel.jsm");

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetryReportingPolicy::";

// Oldest year to allow in date preferences. The FHR infobar was implemented in
// 2012 and no dates older than that should be encountered.
const OLDEST_ALLOWED_ACCEPTANCE_YEAR = 2012;

const PREF_BRANCH = "datareporting.policy.";
// Indicates whether this is the first run or not. This is used to decide when to display
// the policy.
const PREF_FIRST_RUN = "toolkit.telemetry.reportingpolicy.firstRun";
// Allows to skip the datachoices infobar. This should only be used in tests.
const PREF_BYPASS_NOTIFICATION = PREF_BRANCH + "dataSubmissionPolicyBypassNotification";
// The submission kill switch: if this preference is disable, no submission will ever take place.
const PREF_DATA_SUBMISSION_ENABLED = PREF_BRANCH + "dataSubmissionEnabled";
// This preference holds the current policy version, which overrides
// DEFAULT_DATAREPORTING_POLICY_VERSION
const PREF_CURRENT_POLICY_VERSION = PREF_BRANCH + "currentPolicyVersion";
// This indicates the minimum required policy version. If the accepted policy version
// is lower than this, the notification bar must be showed again.
const PREF_MINIMUM_POLICY_VERSION = PREF_BRANCH + "minimumPolicyVersion";
// The version of the accepted policy.
const PREF_ACCEPTED_POLICY_VERSION = PREF_BRANCH + "dataSubmissionPolicyAcceptedVersion";
// The date user accepted the policy.
const PREF_ACCEPTED_POLICY_DATE = PREF_BRANCH + "dataSubmissionPolicyNotifiedTime";
// The following preferences are deprecated and will be purged during the preferences
// migration process.
const DEPRECATED_FHR_PREFS = [
  PREF_BRANCH + "dataSubmissionPolicyAccepted",
  PREF_BRANCH + "dataSubmissionPolicyBypassAcceptance",
  PREF_BRANCH + "dataSubmissionPolicyResponseType",
  PREF_BRANCH + "dataSubmissionPolicyResponseTime"
];

// How much time until we display the data choices notification bar, on the first run.
const NOTIFICATION_DELAY_FIRST_RUN_MSEC = 60 * 1000; // 60s
// Same as above, for the next runs.
const NOTIFICATION_DELAY_NEXT_RUNS_MSEC = 10 * 1000; // 10s

/**
 * Represents a request to display data policy.
 *
 * Receivers of these instances are expected to call one or more of the on*
 * functions when events occur.
 *
 * When one of these requests is received, the first thing a callee should do
 * is present notification to the user of the data policy. When the notice
 * is displayed to the user, the callee should call `onUserNotifyComplete`.
 *
 * If for whatever reason the callee could not display a notice,
 * it should call `onUserNotifyFailed`.
 *
 * @param {Object} aLog The log object used to log the error in case of failures.
 */
function NotifyPolicyRequest(aLog) {
  this._log = aLog;
}

NotifyPolicyRequest.prototype = Object.freeze({
  /**
   * Called when the user is notified of the policy.
   */
  onUserNotifyComplete: function () {
    return TelemetryReportingPolicyImpl._infobarShownCallback();
   },

  /**
   * Called when there was an error notifying the user about the policy.
   *
   * @param error
   *        (Error) Explains what went wrong.
   */
  onUserNotifyFailed: function (error) {
    this._log.error("onUserNotifyFailed - " + error);
  },
});

this.TelemetryReportingPolicy = {
  // The current policy version number. If the version number stored in the prefs
  // is smaller than this, data upload will be disabled until the user is re-notified
  // about the policy changes.
  DEFAULT_DATAREPORTING_POLICY_VERSION: 1,

  /**
   * Setup the policy.
   */
  setup: function() {
    return TelemetryReportingPolicyImpl.setup();
  },

  /**
   * Shutdown and clear the policy.
   */
  shutdown: function() {
    return TelemetryReportingPolicyImpl.shutdown();
  },

  /**
   * Check if we are allowed to upload data. In order to submit data both these conditions
   * should be true:
   * - The data submission preference should be true.
   * - The datachoices infobar should have been displayed.
   *
   * @return {Boolean} True if we are allowed to upload data, false otherwise.
   */
  canUpload: function() {
    return TelemetryReportingPolicyImpl.canUpload();
  },
};

let TelemetryReportingPolicyImpl = {
  _logger: null,
  // Keep track of the notification status if user wasn't notified already.
  _notificationInProgress: false,
  // The timer used to show the datachoices notification at startup.
  _startupNotificationTimerId: null,

  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, LOGGER_PREFIX);
    }

    return this._logger;
  },

  /**
   * Get the date the policy was notified.
   * @return {Object} A date object or null on errors.
   */
  get dataSubmissionPolicyNotifiedDate() {
    let prefString = Preferences.get(PREF_ACCEPTED_POLICY_DATE, 0);
    let valueInteger = parseInt(prefString, 10);

    // If nothing or an invalid value is saved in the prefs, bail out.
    if (Number.isNaN(valueInteger) || valueInteger == 0) {
      this._log.error("get dataSubmissionPolicyNotifiedDate - Invalid date stored.");
      return null;
    }

    // Make sure the notification date is newer then the oldest allowed date.
    let date = new Date(valueInteger);
    if (date.getFullYear() < OLDEST_ALLOWED_ACCEPTANCE_YEAR) {
      this._log.error("get dataSubmissionPolicyNotifiedDate - The stored date is too old.");
      return null;
    }

    return date;
  },

  /**
   * Set the date the policy was notified.
   * @param {Object} aDate A valid date object.
   */
  set dataSubmissionPolicyNotifiedDate(aDate) {
    this._log.trace("set dataSubmissionPolicyNotifiedDate - aDate: " + aDate);

    if (!aDate || aDate.getFullYear() < OLDEST_ALLOWED_ACCEPTANCE_YEAR) {
      this._log.error("set dataSubmissionPolicyNotifiedDate - Invalid notification date.");
      return;
    }

    Preferences.set(PREF_ACCEPTED_POLICY_DATE, aDate.getTime().toString());
  },

  /**
   * Whether submission of data is allowed.
   *
   * This is the master switch for remote server communication. If it is
   * false, we never request upload or deletion.
   */
  get dataSubmissionEnabled() {
    // Default is true because we are opt-out.
    return Preferences.get(PREF_DATA_SUBMISSION_ENABLED, true);
  },

  get currentPolicyVersion() {
    return Preferences.get(PREF_CURRENT_POLICY_VERSION,
                           TelemetryReportingPolicy.DEFAULT_DATAREPORTING_POLICY_VERSION);
  },

  /**
   * The minimum policy version which for dataSubmissionPolicyAccepted to
   * to be valid.
   */
  get minimumPolicyVersion() {
    const minPolicyVersion = Preferences.get(PREF_MINIMUM_POLICY_VERSION, 1);

    // First check if the current channel has a specific minimum policy version. If not,
    // use the general minimum policy version.
    let channel = "";
    try {
      channel = UpdateChannel.get(false);
    } catch(e) {
      this._log.error("minimumPolicyVersion - Unable to retrieve the current channel.");
      return minPolicyVersion;
    }
    const channelPref = PREF_MINIMUM_POLICY_VERSION + ".channel-" + channel;
    return Preferences.get(channelPref, minPolicyVersion);
  },

  get dataSubmissionPolicyAcceptedVersion() {
    return Preferences.get(PREF_ACCEPTED_POLICY_VERSION, 0);
  },

  set dataSubmissionPolicyAcceptedVersion(value) {
    Preferences.set(PREF_ACCEPTED_POLICY_VERSION, value);
  },

  /**
   * Checks to see if the user has been notified about data submission
   * @return {Bool} True if user has been notified and the notification is still valid,
   *         false otherwise.
   */
  get isUserNotifiedOfCurrentPolicy() {
    // If we don't have a sane notification date, the user was not notified yet.
    if (!this.dataSubmissionPolicyNotifiedDate ||
        this.dataSubmissionPolicyNotifiedDate.getTime() <= 0) {
      return false;
    }

    // The accepted policy version should not be less than the minimum policy version.
    if (this.dataSubmissionPolicyAcceptedVersion < this.minimumPolicyVersion) {
      return false;
    }

    // Otherwise the user was already notified.
    return true;
  },

  /**
   * Setup the policy.
   */
  setup: function() {
    this._log.trace("setup");

    // Migrate the data choices infobar, if needed.
    this._migratePreferences();

    // Add the event observers.
    Services.obs.addObserver(this, "sessionstore-windows-restored", false);
  },

  /**
   * Clean up the reporting policy.
   */
  shutdown: function() {
    this._log.trace("shutdown");

    this._detachObservers();

    clearTimeout(this._startupNotificationTimerId);
  },

  /**
   * Detach the observers that were attached during setup.
   */
  _detachObservers: function() {
    Services.obs.removeObserver(this, "sessionstore-windows-restored");
  },

  /**
   * Check if we are allowed to upload data. In order to submit data both these conditions
   * should be true:
   * - The data submission preference should be true.
   * - The datachoices infobar should have been displayed.
   *
   * @return {Boolean} True if we are allowed to upload data, false otherwise.
   */
  canUpload: function() {
    // If data submission is disabled, there's no point in showing the infobar. Just
    // forbid to upload.
    if (!this.dataSubmissionEnabled) {
      return false;
    }

    // Make sure the user is notified of the current policy. If he isn't, don't try
    // to upload anything.
    if (!this._ensureUserNotified()) {
      return false;
    }

    // Submission is enabled and user is notified: upload is allowed.
    return true;
  },

  /**
   * Migrate the data policy preferences, if needed.
   */
  _migratePreferences: function() {
    // Current prefs are mostly the same than the old ones, except for some deprecated ones.
    for (let pref of DEPRECATED_FHR_PREFS) {
      Preferences.reset(pref);
    }
  },

  /**
   * Make sure the user is notified about the policy before allowing upload.
   * @return {Boolean} True if the user was notified, false otherwise.
   */
  _ensureUserNotified: function() {
    const BYPASS_NOTIFICATION = Preferences.get(PREF_BYPASS_NOTIFICATION, false);
    if (this.isUserNotifiedOfCurrentPolicy || BYPASS_NOTIFICATION) {
      return true;
    }

    this._log.trace("ensureUserNotified - User not notified, notifying now.");
    if (this._notificationInProgress) {
      this._log.trace("ensureUserNotified - User not notified, notification in progress.");
      return false;
    }

    this._notificationInProgress = true;
    let request = new NotifyPolicyRequest(this._log);
    Observers.notify("datareporting:notify-data-policy:request", request);

    return false;
  },

  /**
   * Called when the user is notified with the infobar.
   */
  _infobarShownCallback: function() {
    this._log.trace("_infobarShownCallback");
    this._recordNotificationData();
    TelemetrySend.notifyCanUpload();
  },

  /**
   * Record date and the version of the accepted policy.
   */
  _recordNotificationData: function() {
    this._log.trace("_recordNotificationData");
    this.dataSubmissionPolicyNotifiedDate = new Date();
    this.dataSubmissionPolicyAcceptedVersion = this.currentPolicyVersion;
    // The user was notified and the notification data saved: the notification
    // is no longer in progress.
    this._notificationInProgress = false;
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic != "sessionstore-windows-restored") {
      return;
    }

    const isFirstRun = Preferences.get(PREF_FIRST_RUN, true);
    const delay =
      isFirstRun ? NOTIFICATION_DELAY_FIRST_RUN_MSEC: NOTIFICATION_DELAY_NEXT_RUNS_MSEC;

    this._startupNotificationTimerId = setTimeout(
        // Calling |canUpload| eventually shows the infobar, if needed.
        () => this.canUpload(), delay);
    // We performed at least a run, flip the firstRun preference.
    Preferences.set(PREF_FIRST_RUN, false);
  },
};
