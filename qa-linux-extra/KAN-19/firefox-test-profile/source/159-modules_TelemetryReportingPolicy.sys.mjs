/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Log } from "resource://gre/modules/Log.sys.mjs";
import { clearTimeout, setTimeout } from "resource://gre/modules/Timer.sys.mjs";

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { Observers } from "resource://services-common/observers.sys.mjs";
import { TelemetryUtils } from "resource://gre/modules/TelemetryUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserUtils: "resource://gre/modules/BrowserUtils.sys.mjs",
  TelemetrySend: "resource://gre/modules/TelemetrySend.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  OnboardingMessageProvider:
    "resource:///modules/asrouter/OnboardingMessageProvider.sys.mjs",
  WinTaskbarJumpList: "resource:///modules/WindowsJumpLists.sys.mjs",
});

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetryReportingPolicy::";

// Oldest year to allow in telemetry policy date preferences. The FHR infobar
// was implemented in 2012 and no dates older than that should be encountered.
const OLDEST_ALLOWED_TELEMETRY_POLICY_ACCEPTANCE_YEAR = 2012;
// Oldest year to allow in terms of use date preferences.
const OLDEST_ALLOWED_TOU_ACCEPTANCE_YEAR = 2025;

const PREF_BRANCH = "datareporting.policy.";

const TOU_ACCEPTED_VERSION_PREF = "termsofuse.acceptedVersion";
const TOU_CURRENT_VERSION_PREF = "termsofuse.currentVersion";
const TOU_MINIMUM_VERSION_PREF = "termsofuse.minimumVersion";
const TOU_ACCEPTED_DATE_PREF = "termsofuse.acceptedDate";
const TOU_BYPASS_NOTIFICATION_PREF = "termsofuse.bypassNotification";
const TOU_PREF_MIGRATION_CHECK = "browser.termsofuse.prefMigrationCheck";

// The following preferences are deprecated and will be purged during the preferences
// migration process.
const DEPRECATED_FHR_PREFS = [
  PREF_BRANCH + "dataSubmissionPolicyAccepted",
  PREF_BRANCH + "dataSubmissionPolicyBypassAcceptance",
  PREF_BRANCH + "dataSubmissionPolicyResponseType",
  PREF_BRANCH + "dataSubmissionPolicyResponseTime",
];

// How much time until we display the data choices notification bar, on the first run.
const NOTIFICATION_DELAY_FIRST_RUN_MSEC = 60 * 1000; // 60s
// Same as above, for the next runs.
const NOTIFICATION_DELAY_NEXT_RUNS_MSEC = 10 * 1000; // 10s

const NOTIFICATION_TYPES = {
  DATAREPORTING_POLICY_TAB_OR_INFOBAR: 0,
  TERMS_OF_SERVICE_MODAL: 1,
};

/**
 * This is a policy object used to override behavior within this module.
 * Tests override properties on this object to allow for control of behavior
 * that would otherwise be very hard to cover.
 */
export var Policy = {
  now: () => new Date(),
  setShowInfobarTimeout: (callback, delayMs) => setTimeout(callback, delayMs),
  clearShowInfobarTimeout: id => clearTimeout(id),
  // This test method fakes a session restore event which triggers
  // `_delayedSetup` and thus kicks off the reporting flow
  fakeSessionRestoreNotification: async () => {
    TelemetryReportingPolicyImpl.observe(
      null,
      "sessionstore-windows-restored",
      null
    );
  },
  showModal: async data => {
    return TelemetryReportingPolicyImpl._showModal(data);
  },
  delayedSetup: async () => TelemetryReportingPolicyImpl._delayedSetup(),
  // Windows and macOS are enabled through the default pref value in firefox.js
  // and don't need a runtime opt in. Otherwise, TOU is only supported on
  // official Mozilla Linux distributions. All other platforms are excluded both
  // by the pref default in firefox.js and by this explicit Linux gate.
  shouldEnableTOUAtRuntime: () => {
    return AppConstants.platform === "linux" && Policy.isEligibleOnLinux();
  },
  isEligibleOnLinux: () => {
    return Services.prefs
      .getDefaultBranch(null)
      .getCharPref("distribution.id", "")
      .startsWith("mozilla");
  },
};

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
 * @param {object} aLog The log object used to log the error in case of failures.
 * @param {function} aResolve Promise-like callback function, invoked with
 *                            `true` (complete) or `false` (error).
 */
function NotifyPolicyRequest(aLog, aResolve) {
  this._log = aLog;
  this._resolve = aResolve;
}

NotifyPolicyRequest.prototype = Object.freeze({
  /**
   * Called when the user is notified of the policy.
   */
  onUserNotifyComplete() {
    this._resolve(true);
  },

  /**
   * Called when there was an error notifying the user about the policy.
   *
   * @param error
   *        (Error) Explains what went wrong.
   */
  onUserNotifyFailed(error) {
    this._log.error("onUserNotifyFailed - " + error);
    this._resolve(false);
  },
});

export var TelemetryReportingPolicy = {
  // The current policy version number. If the version number stored in the prefs
  // is smaller than this, data upload will be disabled until the user is re-notified
  // about the policy changes.
  DEFAULT_DATAREPORTING_POLICY_VERSION: 1,
  // We set the default version as 4 to distinguish it from version numbers used
  // in the original TOU experiments and rollouts
  DEFAULT_TERMS_OF_USE_POLICY_VERSION: 4,
  /**
   * This event notifies other parts of the system that it's safe to proceed with initialization.
   * Example: newtab waits to initialize until it receives this event.
   *
   * It is dispatched when:
   *   1. The user accepts the Terms of Use (ToU), or
   *   2. The user has previously accepted the ToU, or
   *   2. The user is not eligible to see the ToU. Example local builds and non-official Linux distributions.
   */
  TELEMETRY_TOU_ACCEPTED_OR_INELIGIBLE: "telemetry-tou-accepted-or-ineligible",
  // Make this value accessible on TelemetryReportingPolicy
  OLDEST_ALLOWED_TOU_ACCEPTANCE_YEAR,

  TOU_ACCEPTED_DATE_PREF,

  /**
   * Setup the policy.
   */
  setup() {
    return TelemetryReportingPolicyImpl.setup();
  },

  /**
   * Shutdown and clear the policy.
   */
  shutdown() {
    return TelemetryReportingPolicyImpl.shutdown();
  },

  /**
   * Check if we are allowed to upload data. In order to submit data both these conditions
   * should be true:
   * - The data submission preference should be true.
   * - The datachoices infobar should have been displayed.
   *
   * @return {boolean} True if we are allowed to upload data, false otherwise.
   */
  canUpload() {
    return TelemetryReportingPolicyImpl.canUpload();
  },

  /**
   * Check if this is the first time the browser ran.
   */
  isFirstRun() {
    return TelemetryReportingPolicyImpl.isFirstRun();
  },

  /**
   * Test only method, restarts the policy.
   */
  reset() {
    return TelemetryReportingPolicyImpl.reset();
  },

  /**
   * Test only method, used to check if the policy should notify in tests.
   */
  testShouldNotify() {
    return TelemetryReportingPolicyImpl._shouldNotifyDataReportingPolicy();
  },

  /**
   * Test only method, used to check if user is notified of the legacy data reporting policy in tests.
   */
  testIsUserNotifiedOfDataReportingPolicy() {
    return TelemetryReportingPolicyImpl.isUserNotifiedOfCurrentPolicy;
  },

  /**
   * Used to check if user has accepted the TOU.
   */
  userHasAcceptedTOU() {
    return TelemetryReportingPolicyImpl.hasUserAcceptedCurrentTOU;
  },

  /**
   * Test only method, used to simulate the infobar being shown in xpcshell tests.
   */
  testInfobarShown() {
    TelemetryReportingPolicyImpl._notificationType =
      NOTIFICATION_TYPES.DATAREPORTING_POLICY_TAB_OR_INFOBAR;
    return TelemetryReportingPolicyImpl._userNotified();
  },

  /**
   * Test only method, used to trigger an update of the "first run" state.
   */
  testUpdateFirstRun() {
    TelemetryReportingPolicyImpl._isFirstRun = undefined;
    TelemetryReportingPolicyImpl.isFirstRun();
  },

  /**
   * Test only method, used simulate a notification being in-progress.
   */
  testNotificationInProgress(inProgress) {
    TelemetryReportingPolicyImpl._notificationInProgress = inProgress;
  },

  get termsOfUseAcceptedDate() {
    return TelemetryReportingPolicyImpl.termsOfUseAcceptedDate;
  },

  /**
   * Test only method, used to get TOS on-train release dates by channel.
   */
  get fullOnTrainReleaseDates() {
    return TelemetryReportingPolicyImpl.fullOnTrainReleaseDates;
  },

  get minimumPolicyVersion() {
    return TelemetryReportingPolicyImpl.minimumPolicyVersion;
  },

  async ensureUserIsNotified() {
    return TelemetryReportingPolicyImpl.ensureUserIsNotified();
  },

  willShowTOUModal() {
    return TelemetryReportingPolicyImpl._shouldShowTOU({
      ignoreInProgress: true,
    });
  },

  hasUserResolvedTermsOfUse() {
    return (
      TelemetryReportingPolicyImpl.hasUserAcceptedCurrentTOU ||
      TelemetryReportingPolicyImpl.isUserNotifiedOfCurrentPolicy
    );
  },
};

var TelemetryReportingPolicyImpl = {
  _logger: null,
  // Keep track of the notification status if user wasn't notified already.
  _notificationInProgress: false,
  // The timer used to show the datachoices notification at startup.
  _startupNotificationTimerId: null,
  // Keep track of the first session state, as the related preference
  // is flipped right after the browser starts.
  _isFirstRun: undefined,
  // Ensure notification flow is idempotent.
  _ensureUserIsNotifiedPromise: undefined,
  // Nimbus `preonboarding` feature variables.  Set in response to
  // `sessionstore-window-restored`; immutable there-after.
  _nimbusVariables: {},
  // Record whether users were notified via the terms of use modal or the data
  // reporting policy tab / infobar
  _notificationType: null,

  _observerRegistered: false,

  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(
        LOGGER_NAME,
        LOGGER_PREFIX
      );
    }

    return this._logger;
  },

  /**
   * Get the date the policy was notified.
   *
   * @return {object} A date object or null on errors.
   */
  get dataSubmissionPolicyNotifiedDate() {
    let prefString = Services.prefs.getStringPref(
      TelemetryUtils.Preferences.AcceptedPolicyDate,
      "0"
    );
    let valueInteger = parseInt(prefString, 10);

    // Bail out if we didn't store any value yet.
    if (valueInteger == 0) {
      this._log.info(
        "get dataSubmissionPolicyNotifiedDate - No date stored yet."
      );
      return null;
    }

    // If an invalid value is saved in the prefs, bail out too.
    if (Number.isNaN(valueInteger)) {
      this._log.error(
        "get dataSubmissionPolicyNotifiedDate - Invalid date stored."
      );
      return null;
    }

    // Make sure the notification date is newer then the oldest allowed date.
    let date = new Date(valueInteger);
    if (date.getFullYear() < OLDEST_ALLOWED_TELEMETRY_POLICY_ACCEPTANCE_YEAR) {
      this._log.error(
        "get dataSubmissionPolicyNotifiedDate - The stored date is too old."
      );
      return null;
    }

    return date;
  },

  /**
   * Set the date the policy was notified.
   *
   * @param {object} aDate A valid date object.
   */
  set dataSubmissionPolicyNotifiedDate(aDate) {
    this._log.trace("set dataSubmissionPolicyNotifiedDate - aDate: " + aDate);

    if (
      !aDate ||
      aDate.getFullYear() < OLDEST_ALLOWED_TELEMETRY_POLICY_ACCEPTANCE_YEAR
    ) {
      this._log.error(
        "set dataSubmissionPolicyNotifiedDate - Invalid notification date."
      );
      return;
    }

    Services.prefs.setStringPref(
      TelemetryUtils.Preferences.AcceptedPolicyDate,
      aDate.getTime().toString()
    );
  },

  /**
   * Get the date the terms of use were accepted.
   *
   * @return {object} A date object or null on errors.
   */
  get termsOfUseAcceptedDate() {
    // For consistency, we use the same method of parsing a stringified
    // timestamp as is used in the legacy data reporting policy flow
    let prefString = Services.prefs.getStringPref(TOU_ACCEPTED_DATE_PREF, "0");
    let valueInteger = parseInt(prefString, 10);

    // Bail out if we didn't store any value yet.
    if (valueInteger == 0) {
      this._log.info("get termsOfUseAcceptedDate - No date stored yet.");
      return null;
    }

    // If an invalid value is saved in the prefs, bail out too.
    if (Number.isNaN(valueInteger)) {
      this._log.error("get termsOfUseAcceptedDate - Invalid date stored.");
      return null;
    }

    // Make sure the notification date is newer then the oldest allowed date.
    let date = new Date(valueInteger);
    if (date.getFullYear() < OLDEST_ALLOWED_TOU_ACCEPTANCE_YEAR) {
      this._log.error(
        "get termsOfUseAcceptedDate - The stored date is too old."
      );
      return null;
    }

    return date;
  },

  /**
   * Set the date the policy was notified.
   *
   * @param {object} aDate A valid date object.
   */
  set termsOfUseAcceptedDate(aDate) {
    this._log.trace("set termsOfUseAcceptedDate - aDate: " + aDate);

    if (!aDate || aDate.getFullYear() < OLDEST_ALLOWED_TOU_ACCEPTANCE_YEAR) {
      this._log.error(
        "set termsOfUseAcceptedDate - Invalid notification date."
      );
      return;
    }

    Services.prefs.setStringPref(
      TOU_ACCEPTED_DATE_PREF,
      aDate.getTime().toString()
    );
  },

  /**
   * Whether submission of data is allowed.
   *
   * This is the master switch for remote server communication. If it is
   * false, we never request upload or deletion.
   */
  get dataSubmissionEnabled() {
    // Default is true because we are opt-out.
    return Services.prefs.getBoolPref(
      TelemetryUtils.Preferences.DataSubmissionEnabled,
      true
    );
  },

  get currentPolicyVersion() {
    return Services.prefs.getIntPref(
      TelemetryUtils.Preferences.CurrentPolicyVersion,
      TelemetryReportingPolicy.DEFAULT_DATAREPORTING_POLICY_VERSION
    );
  },

  /**
   * The current terms of use version.
   *
   * @returns {number}
   * The integer value stored in `TOU_CURRENT_VERSION_PREF`. If that preference
   * is unset or cannot be parsed, falls back to
   * `TelemetryReportingPolicy.DEFAULT_TERMS_OF_USE_POLICY_VERSION`.
   *
   * Falling back to the default ensures we always have a valid version for
   * comparisons.
   */
  get currentTermsOfUseVersion() {
    return Services.prefs.getIntPref(
      TOU_CURRENT_VERSION_PREF,
      TelemetryReportingPolicy.DEFAULT_TERMS_OF_USE_POLICY_VERSION
    );
  },

  /**
   * The minimum policy version which for dataSubmissionPolicyAccepted to
   * to be valid.
   */
  get minimumPolicyVersion() {
    const minPolicyVersion = Services.prefs.getIntPref(
      TelemetryUtils.Preferences.MinimumPolicyVersion,
      1
    );

    // First check if the current channel has a specific minimum policy version. If not,
    // use the general minimum policy version.
    let channel = "";
    try {
      channel = TelemetryUtils.getUpdateChannel();
    } catch (e) {
      this._log.error(
        "minimumPolicyVersion - Unable to retrieve the current channel."
      );
      return minPolicyVersion;
    }
    const channelPref =
      TelemetryUtils.Preferences.MinimumPolicyVersion + ".channel-" + channel;
    return Services.prefs.getIntPref(channelPref, minPolicyVersion);
  },

  /**
   * The minimum terms of use version the user needs to have accepted to not be
   * shown the terms of use modal.
   */
  get minimumTermsOfUseVersion() {
    return (
      this._nimbusVariables.minimumPolicyVersion ||
      Services.prefs.getIntPref(
        TOU_MINIMUM_VERSION_PREF,
        TelemetryReportingPolicy.DEFAULT_TERMS_OF_USE_POLICY_VERSION
      )
    );
  },

  get dataSubmissionPolicyAcceptedVersion() {
    return Services.prefs.getIntPref(
      TelemetryUtils.Preferences.AcceptedPolicyVersion,
      0
    );
  },

  set dataSubmissionPolicyAcceptedVersion(value) {
    Services.prefs.setIntPref(
      TelemetryUtils.Preferences.AcceptedPolicyVersion,
      value
    );
  },

  get termsOfUseAcceptedVersion() {
    return Services.prefs.getIntPref(TOU_ACCEPTED_VERSION_PREF, 0);
  },

  set termsOfUseAcceptedVersion(value) {
    Services.prefs.setIntPref(TOU_ACCEPTED_VERSION_PREF, value);
  },

  /**
   * Checks to see if the user has been notified about data submission
   *
   * @return {Bool} True if user has been notified and the notification is still valid,
   *         false otherwise.
   */
  get isUserNotifiedOfCurrentPolicy() {
    // If we don't have a sane notification date, the user was not notified yet.
    if (
      !this.dataSubmissionPolicyNotifiedDate ||
      this.dataSubmissionPolicyNotifiedDate.getTime() <= 0
    ) {
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
   * Checks to see if the user has accepted the current terms of use
   *
   * @return {Bool} True if user has accepted and the acceptance is still valid,
   *         false otherwise.
   */
  get hasUserAcceptedCurrentTOU() {
    // If we don't have a sane notification date, the user was not notified yet.
    if (!this.termsOfUseAcceptedDate) {
      return false;
    }

    // The accepted terms of use version should not be less than the minimum
    // terms of use version.
    if (this.termsOfUseAcceptedVersion < this.minimumTermsOfUseVersion) {
      return false;
    }

    // Otherwise the user has already accepted.
    return true;
  },

  /**
   * Reset a preference completely back to its built-in default value.
   *
   * Calling clearUserPref only removes the user-set override. By also clearing
   * the default branch, we ensure all overrides, both user and runtime changes,
   * are removed so the pref truly returns to the value declared in all.js.
   */
  clearPref(prefName) {
    Services.prefs.clearUserPref(prefName);
    Services.prefs.getDefaultBranch("").clearUserPref(prefName);
  },

  maybeMigrateLegacyTOUDatePref() {
    const hasTOUAcceptedDate =
      this.termsOfUseAcceptedDate && this.termsOfUseAcceptedDate.getTime() > 0;
    if (hasTOUAcceptedDate) {
      return;
    }

    Services.prefs.setStringPref(
      TOU_ACCEPTED_DATE_PREF,
      Services.prefs.getStringPref(
        TelemetryUtils.Preferences.AcceptedPolicyDate,
        "0"
      )
    );

    this.clearPref(TelemetryUtils.Preferences.AcceptedPolicyDate);
    this._log.trace(
      "maybeMigrateLegacyTOUDatePref - migrated data reporting policy accepted date to TOU accepted date."
    );
  },

  // If needed, migrate user's prefs from initial TOU experiments and rollouts
  // to a set of consistent values.
  maybeMigrateLegacyTOUVersionPref() {
    const LEGACY_ACCEPTED_TOU_VERSION =
      TelemetryReportingPolicy.DEFAULT_TERMS_OF_USE_POLICY_VERSION;
    if (this.termsOfUseAcceptedVersion) {
      return;
    }

    Services.prefs.setIntPref(
      TOU_ACCEPTED_VERSION_PREF,
      LEGACY_ACCEPTED_TOU_VERSION
    );

    this.clearPref(TelemetryUtils.Preferences.AcceptedPolicyVersion);
    // Clear legacy current and minimum versions which may have been set by TOU
    // experiments or rollouts
    this.clearPref(TelemetryUtils.Preferences.CurrentPolicyVersion);
    this.clearPref(TelemetryUtils.Preferences.MinimumPolicyVersion);
    // clear the channel override for minimumPolicyVersion:
    let channel = TelemetryUtils.getUpdateChannel();
    let channelMin =
      TelemetryUtils.Preferences.MinimumPolicyVersion + ".channel-" + channel;
    Services.prefs.clearUserPref(channelMin);
    this._log.trace(
      "maybeMigrateLegacyTOUVersionPref - migrated data reporting policy accepted version to TOU accepted version."
    );
  },

  // See comment in `acceptedTOUDuringExperimentationPhase` for more details.
  fullOnTrainReleaseDates: {
    // Note that months are expected to be zero-indexed for Date.UTC
    // May 27, 2025, 13:00 UTC when 139.0 went live on Release
    release: String(Date.UTC(2025, 4, 27, 13, 0, 0)),
    // May 2, 2025, 13:00 UTC when 139.0b3 went live on Beta and Aurora
    beta: String(Date.UTC(2025, 4, 2, 13, 0, 0)),
    aurora: String(Date.UTC(2025, 4, 2, 13, 0, 0)),
    // May 2, 2025, 11:45 UTC approximate timestamp for Nightly and Default
    // roll-out
    nightly: String(Date.UTC(2025, 4, 2, 11, 45, 0)),
    default: String(Date.UTC(2025, 4, 2, 11, 45, 0)),
    // "esr" channel unaffected
  },

  get acceptedTOUDuringExperimentationPhase() {
    let channel = TelemetryUtils.getUpdateChannel();
    let releaseDateOnChannel = this.fullOnTrainReleaseDates[channel] || "0";

    const legacyAcceptedVersion = this.dataSubmissionPolicyAcceptedVersion;
    const legacyAcceptedTime = parseInt(
      Services.prefs.getStringPref(
        TelemetryUtils.Preferences.AcceptedPolicyDate,
        "0"
      ),
      10
    );

    // Due to a bug in the patch landed in Bug 1959542, the legacy accepted
    // version pref for users who accepted the TOU via the preonboarding modal
    // in after TOU was enabled by default on-train (Bug 1952000) was recorded
    // as 2 instead of 3. "browser.preonboarding.enrolledInOnTrainRollout" always
    // being set to true when `_configureFromNimbus` ran and a user was not
    // enrolled in a nimbus experiment. So, we also need to check to see if the
    // user accepted after the full rollout went live on the relevent channel to
    // confirm that they accepted the TOU and not the legacy data reporting
    // flow.
    const legacyFullRolloutVersion = 2;
    const enrolled = Services.prefs.getBoolPref(
      "browser.preonboarding.enrolledInOnTrainRollout",
      false
    );
    // On Linux, TOU is only available for official linux builds. Users on
    // other Linux builds see the legacy data reporting flow instead (see
    // Bug 1964180).
    const TOSEnabled =
      (AppConstants.platform !== "linux" || Policy.isEligibleOnLinux()) &&
      Services.prefs.getBoolPref("browser.preonboarding.enabled", true);
    const acceptedAfterTOULandedAsDefault =
      TOSEnabled &&
      enrolled &&
      legacyAcceptedVersion >= legacyFullRolloutVersion &&
      legacyAcceptedTime >= parseInt(releaseDateOnChannel, 10);
    // Users who accepted TOU in the initial Nimbus experiments or partial
    // on-train rollouts (Bug 1952000), have an accepted version of 3.
    const acceptedInExperimentOrOnTrainRollout = Boolean(
      legacyAcceptedVersion === 3
    );

    return (
      (acceptedAfterTOULandedAsDefault ||
        acceptedInExperimentOrOnTrainRollout) &&
      this.dataSubmissionPolicyNotifiedDate
    );
  },

  /**
   *  Update terms of use prefs for users who accepted during initial Nimbus
   *  experiments or the experimental on-train rollout
   *
   *  The pref migration logic can be removed when the TOU moves to the next
   *  major version (see Bug 1971184).
   */
  updateTOUPrefsForLegacyUsers() {
    if (
      this.termsOfUseAcceptedVersion >=
      TelemetryReportingPolicy.DEFAULT_TERMS_OF_USE_POLICY_VERSION
    ) {
      return;
    }

    // If a user previously opted to bypass notification, opt to do so via the
    // new TOU bypass pref as well.
    if (
      Services.prefs.getBoolPref(
        TelemetryUtils.Preferences.BypassNotification,
        false
      )
    ) {
      Services.prefs.setBoolPref(TOU_BYPASS_NOTIFICATION_PREF, true);
    }

    // The experimentation phase was the first phase of asking users to accept
    // the TOU, the legacy data reporting flow marks users as being notified of
    // the telemetry reporting policy. So, we do only migrate pref values for
    // users who accepted TOU, not those who only accepted the legacy data
    // reporting flow as the latter still need to agree to the TOU.
    if (!this.acceptedTOUDuringExperimentationPhase) {
      this._log.trace(
        "updateTOUPrefsForLegacyUsers - did not accept TOU during initial experimentation phase, no action required."
      );
      Services.prefs.setBoolPref(TOU_PREF_MIGRATION_CHECK, true);
      return;
    }

    this.maybeMigrateLegacyTOUDatePref();
    this.maybeMigrateLegacyTOUVersionPref();

    Services.prefs.setBoolPref(TOU_PREF_MIGRATION_CHECK, true);
  },

  /**
   * Test only method, restarts the policy.
   */
  reset() {
    this.shutdown();
    this._isFirstRun = undefined;
    this._ensureUserIsNotifiedPromise = undefined;
    this._nimbusVariables = {};
    return this.setup();
  },

  /**
   * Setup the policy.
   */
  setup() {
    this._log.trace("setup");

    // Migrate the data choices infobar, if needed.
    this._migratePreferences();

    // Update TOU metrics with current state so that its sent in the next
    // metrics ping even if the values don't change.
    this._recordTOUDateTelemetry();
    this._recordTOUVersionTelemetry();

    // Watch for TOU pref changes so our metrics always stay in sync.
    Services.prefs.addObserver(TOU_ACCEPTED_DATE_PREF, this);
    Services.prefs.addObserver(TOU_ACCEPTED_VERSION_PREF, this);

    // Add the event observers.
    if (!this._observerRegistered) {
      Services.obs.addObserver(this, "sessionstore-windows-restored");
      this._observerRegistered = true;
    }
  },

  /**
   * Clean up the reporting policy.
   */
  shutdown() {
    this._log.trace("shutdown");

    this._detachObservers();

    Policy.clearShowInfobarTimeout(this._startupNotificationTimerId);
  },

  /**
   * Detach the observers that were attached during setup.
   */
  _detachObservers() {
    if (this._observerRegistered) {
      Services.obs.removeObserver(this, "sessionstore-windows-restored");
      this._observerRegistered = false;
    }
    Services.prefs.removeObserver(TOU_ACCEPTED_DATE_PREF, this);
    Services.prefs.removeObserver(TOU_ACCEPTED_VERSION_PREF, this);
  },

  /**
   * Check if we are allowed to upload data.
   * Prerequisite: data submission is enabled (this.dataSubmissionEnabled).
   *
   * If a notification is currently in progress, the user should not be allowed
   * to upload data.
   *
   * Otherwise, for upload to be allowed from a data reporting standpoint, the
   * user should not qualify to see the legacy policy notification flow and also
   * not qualify to see the Terms of Use acceptance flow.
   *
   * @return {boolean} True if we are allowed to upload data, false otherwise.
   */
  canUpload() {
    // If data submission is disabled, there's no point in showing the infobar. Just
    // forbid to upload.
    if (!this.dataSubmissionEnabled) {
      return false;
    }

    // If a notification is in progress, such as when the TOU modal is currently
    // showing and user has not yet accepted, do not allow upload. The legacy
    // flow doesn't require interaction, so the notification is only considered
    // to be in progress for the brief period between when the infobar or
    // privacy notice tab is shown and when the notification is recorded.
    if (this._notificationInProgress) {
      return false;
    }

    return !this._shouldNotifyDataReportingPolicy() && !this._shouldShowTOU();
  },

  isFirstRun() {
    if (this._isFirstRun === undefined) {
      this._isFirstRun = Services.prefs.getBoolPref(
        TelemetryUtils.Preferences.FirstRun,
        true
      );
    }
    return this._isFirstRun;
  },

  /**
   * Migrate the data policy preferences, if needed.
   */
  _migratePreferences() {
    // Current prefs are mostly the same than the old ones, except for some deprecated ones.
    for (let pref of DEPRECATED_FHR_PREFS) {
      Services.prefs.clearUserPref(pref);
    }
  },

  /**
   * Determine whether the user should be notified of telemetry policy.
   */
  _shouldNotifyDataReportingPolicy() {
    if (!this.dataSubmissionEnabled) {
      this._log.trace(
        "_shouldNotifyDataReportingPolicy - Data submission disabled by the policy."
      );
      return false;
    }

    // If user already accepted the TOU, we don't need to show the data reporting policy notification.
    if (this.hasUserAcceptedCurrentTOU) {
      this._log.trace(
        "_shouldNotifyDataReportingPolicy - TOU already accepted, no need to notify via legacy data reporting policy."
      );
      return false;
    }

    const bypassNotification = Services.prefs.getBoolPref(
      TelemetryUtils.Preferences.BypassNotification,
      false
    );
    if (this.isUserNotifiedOfCurrentPolicy || bypassNotification) {
      this._log.trace(
        "_shouldNotifyDataReportingPolicy - User already notified or bypassing the policy."
      );
      return false;
    }

    if (this._notificationInProgress) {
      this._log.trace(
        "_shouldNotifyDataReportingPolicy - User not notified, notification already in progress."
      );
      return false;
    }

    return true;
  },

  /**
   * Determine whether the user should be shown the terms of use.
   */
  _shouldShowTOU(opts = {}) {
    if (AppConstants.platform === "linux" && !Policy.isEligibleOnLinux()) {
      this._log.trace("_shouldShowTOU - Non-eligible Linux distribution.");
      return false;
    }

    // In some cases, _shouldShowTOU can be called before the Nimbus variables
    // are set. When this happens, we call _configureFromNimbus to initialize
    // these variables before evaluating them. This ensures we have accurate
    // data regarding whether preonboarding is enabled for the user. When
    // preonboarding is explicitly disabled, it should be treated the same the
    // bypassing the TOU flow via the bypass pref.
    if (
      !this._nimbusVariables ||
      (typeof this._nimbusVariables === "object" &&
        Object.keys(this._nimbusVariables).length === 0)
    ) {
      this._configureFromNimbus();
    }

    if (!this._nimbusVariables.enabled || !this._nimbusVariables.screens) {
      this._log.trace(
        "_shouldShowTOU - TOU not enabled or no screens configured."
      );
      return false;
    }

    // If the user was already notified via the legacy data-reporting flow, we
    // should not show them the new user Terms of Use modal flow.
    if (this.isUserNotifiedOfCurrentPolicy) {
      this._log.trace(
        "_shouldShowTOU - User already saw legacy datareporting flow."
      );
      return false;
    }

    const bypassNotification = Services.prefs.getBoolPref(
      TOU_BYPASS_NOTIFICATION_PREF,
      false
    );

    if (bypassNotification) {
      this._log.trace("_shouldShowTOU - User bypassing the policy.");
      return false;
    }

    if (this.hasUserAcceptedCurrentTOU) {
      this._log.trace("_shouldShowTOU - User already accepted TOU.");
      return false;
    }

    if (!opts.ignoreInProgress && this._notificationInProgress) {
      this._log.trace(
        "_shouldShowTOU - User not notified, notification already in progress."
      );
      return false;
    }

    return true;
  },

  /**
   * Show the data choices infobar if needed.
   *
   * @param {function} resolve invoked with `true` if user was notified, `false`
   * if user was not notified.
   */
  _showInfobar(resolve) {
    if (!this._shouldNotifyDataReportingPolicy()) {
      this._log.trace("_showInfobar - User already notified, nothing to do.");
      resolve(false);
      return;
    }

    this._log.trace("_showInfobar - User not notified, notifying now.");
    this._notificationInProgress = true;
    let request = new NotifyPolicyRequest(this._log, resolve);
    Observers.notify("datareporting:notify-data-policy:request", request);
  },

  /**
   * Called when the user is notified with the infobar or otherwise.
   */
  _userNotified() {
    this._log.trace("_userNotified");
    this._recordNotificationData();
    lazy.TelemetrySend.notifyCanUpload();
  },

  _recordTOUDateTelemetry() {
    const date = this.termsOfUseAcceptedDate;
    let formattedDate;
    try {
      // only set the ping if date is a real Date
      if (date instanceof Date) {
        //  PRTime/microseconds expected
        formattedDate = date.getTime() * 1000;
        Glean.termsofuse.date.set(formattedDate);
      }
    } catch (e) {
      this._log.error("Failed to record TOU Glean metrics", e);
    }
  },

  _recordTOUVersionTelemetry() {
    const version = this.termsOfUseAcceptedVersion;
    try {
      Glean.termsofuse.version.set(version);
    } catch (e) {
      this._log.error("Failed to record TOU Version Glean metric", e);
    }
  },

  /**
   * Record date and the version of the accepted policy.
   */
  _recordNotificationData() {
    this._log.trace("_recordNotificationData");
    switch (this._notificationType) {
      case NOTIFICATION_TYPES.DATAREPORTING_POLICY_TAB_OR_INFOBAR:
        this.dataSubmissionPolicyNotifiedDate = Policy.now();
        this.dataSubmissionPolicyAcceptedVersion = this.currentPolicyVersion;
        break;
      case NOTIFICATION_TYPES.TERMS_OF_SERVICE_MODAL:
        this.termsOfUseAcceptedDate = Policy.now();
        this.termsOfUseAcceptedVersion = this.currentTermsOfUseVersion;
        break;
    }
    // The user was notified and the notification data saved: the notification
    // is no longer in progress.
    this._notificationInProgress = false;
  },

  /**
   * Try to open the privacy policy in a background tab instead of showing the infobar.
   *
   * @return {Promise<boolean>} Resolves to `true` if the user was notified via
   *                            background tab, `false` if we should fallback to
   *                            an infobar.
   */
  async _openFirstRunPage() {
    if (!this._shouldNotifyDataReportingPolicy()) {
      return false;
    }

    let firstRunPolicyURL = Services.prefs.getStringPref(
      TelemetryUtils.Preferences.FirstRunURL,
      ""
    );
    if (!firstRunPolicyURL) {
      return false;
    }
    firstRunPolicyURL = Services.urlFormatter.formatURL(firstRunPolicyURL);

    const { BrowserWindowTracker } = ChromeUtils.importESModule(
      "resource:///modules/BrowserWindowTracker.sys.mjs"
    );
    let win = BrowserWindowTracker.getTopWindow({
      allowFromInactiveWorkspace: true,
    });

    if (!win) {
      this._log.info(
        "Couldn't find browser window to open first-run page. Falling back to infobar."
      );
      return false;
    }

    return new Promise(resolve => {
      // We'll consider the user notified once the privacy policy has been loaded
      // in a background tab even if that tab hasn't been selected.
      let tab;
      let progressListener = {};
      progressListener.onStateChange = (
        aBrowser,
        aWebProgress,
        aRequest,
        aStateFlags
      ) => {
        if (
          aWebProgress.isTopLevel &&
          tab &&
          tab.linkedBrowser == aBrowser &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK
        ) {
          removeListeners();

          let uri = aBrowser.documentURI;
          if (
            uri &&
            !/^about:(blank|neterror|certerror|blocked)/.test(uri.spec)
          ) {
            resolve(true);
          } else {
            this._log.info("Failed to load first-run page.");
            resolve(false);
          }
        }
      };

      let removeListeners = () => {
        win.removeEventListener("unload", removeListeners);
        win.gBrowser.removeTabsProgressListener(progressListener);
      };

      win.addEventListener("unload", removeListeners);
      win.gBrowser.addTabsProgressListener(progressListener);

      tab = win.gBrowser.addTab(firstRunPolicyURL, {
        inBackground: true,
        triggeringPrincipal:
          Services.scriptSecurityManager.getSystemPrincipal(),
      });
    });
  },

  observe(aSubject, aTopic, aData) {
    if (aTopic == "sessionstore-windows-restored") {
      this._delayedSetup();
    }
    if (aTopic === "nsPref:changed") {
      if (aData === TOU_ACCEPTED_DATE_PREF) {
        this._recordTOUDateTelemetry();
      }
      if (aData === TOU_ACCEPTED_VERSION_PREF) {
        this._recordTOUVersionTelemetry();
      }
    }
  },

  /**
   * Handle initialization/configuration that happens at
   * `sessionstore-windows-restored` time.
   */
  async _delayedSetup() {
    // We're ready to make decisions about how to notify the user.  We only read
    // the Nimbus features once, so that Nimbus features changing doesn't yield
    // inconsistent results.  We also configure the datareporting policy Gecko
    // preferences based on the Nimbus `preonboarding` feature variables.  This
    // makes sense because we don't have support for re-notifying a user
    // _during_ the Firefox process lifetime; right now, we only notify the user
    // at Firefox startup.
    this.updateTOUPrefsForLegacyUsers();
    this._configureFromNimbus();

    if (this.isFirstRun()) {
      // We're performing the first run, flip firstRun preference for subsequent runs.
      Services.prefs.setBoolPref(TelemetryUtils.Preferences.FirstRun, false);
    }

    if (!this._shouldShowTOU() && !this._shouldNotifyDataReportingPolicy()) {
      this._log.trace(
        `_delayedSetup: neither TOU or legacy data reporting policy will show, no further action required`
      );
      Services.obs.notifyObservers(
        null,
        TelemetryReportingPolicy.TELEMETRY_TOU_ACCEPTED_OR_INELIGIBLE
      );
      return;
    }

    this.ensureUserIsNotified().then(() => {
      this._log.debug("_delayedSetup: marking user notified");
      Services.obs.notifyObservers(
        null,
        TelemetryReportingPolicy.TELEMETRY_TOU_ACCEPTED_OR_INELIGIBLE
      );
      this._userNotified();
    });
  },

  async _waitForUserIsNotified() {
    // We're about to show the user the TOU modal dialog or legacy data
    // reporting flow. Make sure Glean won't initialize on shutdown, in case the
    // user never interacts with the modal or isn't notified. By default
    // `telemetry.fog.init_on_shutdown` is true, but we delay it here to avoid
    // recording data before the user makes their choice in the TOU modal or is
    // notified via the legacy data reporting flow (see Bug D239753).
    //
    // This pref will be reset to true only after the notification flow
    // completes (see TelemetryReportingPolicyImpl.ensureUserIsNotified).
    Services.prefs.setBoolPref("telemetry.fog.init_on_shutdown", false);

    if (!this._shouldShowTOU()) {
      this._log.trace(`_waitForUserIsNotified: will not show TOU`);
    } else if (await this._requestAndAwaitUserResponseViaFxMS()) {
      this._log.trace(
        `_waitForUserIsNotified: user notified via Messaging System`
      );
      this._notificationType = NOTIFICATION_TYPES.TERMS_OF_SERVICE_MODAL;
      return;
    }

    // If the user is in the legacy flow, they were not eligible for ToU.
    Services.obs.notifyObservers(
      null,
      TelemetryReportingPolicy.TELEMETRY_TOU_ACCEPTED_OR_INELIGIBLE
    );
    this._log.trace(
      `_waitForUserIsNotified: user not shown TOU modal, falling back to legacy notification`
    );

    if (!this._shouldNotifyDataReportingPolicy()) {
      this._log.trace(
        `_waitForUserIsNotified: will not show data reporting policy flow, no further action required`
      );
      return;
    }

    await this._notifyUserViaTabOrInfobar();
    this._notificationType =
      NOTIFICATION_TYPES.DATAREPORTING_POLICY_TAB_OR_INFOBAR;
  },

  /**
   * Notify user of privacy policy via background tab or (possibly after falling
   * back) via infobar.  The user is considered notified after the background
   * tab is loaded without error, or after the user has been shown (but not
   * necessarily interacted with) an infobar.
   *
   * @return {Promise<void>} Resolves after user is notified.
   */
  async _notifyUserViaTabOrInfobar() {
    if (this.isFirstRun()) {
      try {
        if (await this._openFirstRunPage()) {
          this._log.trace(
            `_notifyUserViaTabOrInfobar: user notified via browser tab`
          );
          return;
        }
      } catch (e) {
        this._log.error("Failed to open privacy policy tab: " + e);
      }
    }

    // Show the info bar.
    const delay = this.isFirstRun()
      ? NOTIFICATION_DELAY_FIRST_RUN_MSEC
      : NOTIFICATION_DELAY_NEXT_RUNS_MSEC;

    this._log.trace(
      `_notifyUserViaTabOrInfobar: notifying user via infobar after ${delay} milliseconds`
    );

    let p = new Promise(resolve => {
      this._startupNotificationTimerId = Policy.setShowInfobarTimeout(
        // Calling |canUpload| eventually shows the infobar, if needed.
        () => this._showInfobar(resolve),
        delay
      );
    });
    await p;
  },

  /**
   * If the preonboarding feature does not require interaction, resolve
   * immediately.  If the preonboarding feature does require interaction and the
   * required interaction has been completed, resolve immediately.  Otherwise,
   * wait until the required interaction is completed.
   *
   * @return Promise<void>
   * @throws {Error} when called before `sessionstore-windows-restored` notification.
   */
  async ensureUserIsNotified() {
    if (!this._ensureUserIsNotifiedPromise) {
      this._ensureUserIsNotifiedPromise = this._waitForUserIsNotified();
    }

    return this._ensureUserIsNotifiedPromise.then(() => {
      // The user has been notified and interacted with the modal.
      // Glean can now init on shutdown if necessary.
      Services.prefs.setBoolPref("telemetry.fog.init_on_shutdown", true);
    });
  },

  /**
   * Capture Nimbus configuration: record feature variables for future use and
   * set Gecko preferences based on values.
   */
  _configureFromNimbus() {
    if (AppConstants.MOZ_BUILD_APP != "browser") {
      // OnboardingMessageProvider is browser/ only
      return;
    }
    this._nimbusVariables = lazy.NimbusFeatures.preonboarding.getAllVariables();

    if (Policy.shouldEnableTOUAtRuntime()) {
      this._nimbusVariables.enabled = null;
    }

    // Only resolve the OnboardingMessageProvider lazy getter when defaults
    // are actually needed (it loads a browser-only module, which fails in
    // stripped-down environments such as xpcshell child processes launched
    // in test automation)
    if (
      this._nimbusVariables.enabled === null ||
      (this._nimbusVariables.enabled && !this._nimbusVariables.screens?.length)
    ) {
      this._log.trace(
        `_configureFromNimbus: using default preonboarding message`
      );
      this._nimbusVariables =
        lazy.OnboardingMessageProvider.getPreonboardingVariablesWithDefaults(
          this._nimbusVariables
        );
    }

    if (this._nimbusVariables.enabled) {
      if ("currentVersion" in this._nimbusVariables) {
        this._log.trace(
          `_configureFromNimbus: setting currentPolicyVersion from Nimbus feature (${this._nimbusVariables.currentVersion})`
        );
        Services.prefs.setIntPref(
          TOU_CURRENT_VERSION_PREF,
          this._nimbusVariables.currentVersion
        );
      }
      if ("minimumVersion" in this._nimbusVariables) {
        this._log.trace(
          `_configureFromNimbus: setting minimumPolicyVersion from Nimbus feature (${this._nimbusVariables.minimumVersion})`
        );
        Services.prefs.setIntPref(
          TOU_MINIMUM_VERSION_PREF,
          this._nimbusVariables.minimumVersion
        );
      }
      if ("firstRunURL" in this._nimbusVariables) {
        this._log.trace(
          `_configureFromNimbus: setting firstRunURL from Nimbus feature ('${this._nimbusVariables.firstRunURL}')`
        );
        Services.prefs.setCharPref(
          TelemetryUtils.Preferences.FirstRunURL,
          this._nimbusVariables.firstRunURL
        );
      }
    } else {
      this._log.trace(
        `_configureFromNimbus: Nimbus feature disabled, not setting preferences`
      );
    }
  },

  async _showModal(data) {
    const { BrowserWindowTracker } = ChromeUtils.importESModule(
      "resource:///modules/BrowserWindowTracker.sys.mjs"
    );
    const { SpecialMessageActions } = ChromeUtils.importESModule(
      "resource://messaging-system/lib/SpecialMessageActions.sys.mjs"
    );

    let win = BrowserWindowTracker.getTopWindow({
      allowFromInactiveWorkspace: true,
    });
    if (!win) {
      return false;
    }

    const config = {
      type: "SHOW_SPOTLIGHT",
      data: {
        content: {
          template: "multistage",
          id: data?.id || "PRE_ONBOARDING_MODAL",
          backdrop: data?.backdrop,
          screens: data.screens,
          UTMTerm: data?.UTMTerm,
          disableEscClose: data?.requireAction,
          // displayed as a window modal by default
        },
      },
    };

    SpecialMessageActions.handleAction(config, win.gBrowser.selectedBrowser);
    this._notificationInProgress = true;

    return true;
  },

  /**
   * Notify the user via the Firefox Messaging System (e.g., a modal dialog) and
   * wait for user interaction.
   *
   * User interaction is signaled by the
   * `termsofuse:interacted` observer notification.
   *
   * @return {Promise<boolean>} `true` if user was notified, `false` to fallback
   * to legacy tab/infobar notification.
   */
  async _requestAndAwaitUserResponseViaFxMS() {
    let p = lazy.BrowserUtils.promiseObserved("termsofuse:interacted");

    if (!(await Policy.showModal(this._nimbusVariables))) {
      this._log.trace(
        "_notifyUserViaModal: notification was not displayed, falling back to legacy notification"
      );
      return false;
    }

    this._log.trace(
      "_notifyUserViaModal: modal displayed, waiting for user interaction"
    );

    // On Windows, clear the jump list to limit opening new windows while the
    // modal is displayed
    if (AppConstants.platform === "win") {
      lazy.WinTaskbarJumpList.blockJumpList(p);
    }

    await p;
    this._log.trace("_notifyUserViaModal: user interacted with modal");

    return true;
  },
};
