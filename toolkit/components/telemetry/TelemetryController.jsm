/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const myScope = this;

ChromeUtils.import("resource://gre/modules/Log.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/DeferredTask.jsm", this);
ChromeUtils.import("resource://gre/modules/Timer.jsm");
ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

const Utils = TelemetryUtils;

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetryController::";

const PREF_BRANCH_LOG = "toolkit.telemetry.log.";

// Whether the FHR/Telemetry unification features are enabled.
// Changing this pref requires a restart.
const IS_UNIFIED_TELEMETRY = Services.prefs.getBoolPref(TelemetryUtils.Preferences.Unified, false);

const PING_FORMAT_VERSION = 4;

// Delay before intializing telemetry (ms)
const TELEMETRY_DELAY = Services.prefs.getIntPref("toolkit.telemetry.initDelay", 60) * 1000;
// Delay before initializing telemetry if we're testing (ms)
const TELEMETRY_TEST_DELAY = 1;

// How long to wait (ms) before sending the new profile ping on the first
// run of a new profile.
const NEWPROFILE_PING_DEFAULT_DELAY = 30 * 60 * 1000;

// Ping types.
const PING_TYPE_MAIN = "main";
const PING_TYPE_DELETION = "deletion";

// Session ping reasons.
const REASON_GATHER_PAYLOAD = "gather-payload";
const REASON_GATHER_SUBSESSION_PAYLOAD = "gather-subsession-payload";

XPCOMUtils.defineLazyServiceGetter(this, "Telemetry",
                                   "@mozilla.org/base/telemetry;1",
                                   "nsITelemetry");

XPCOMUtils.defineLazyModuleGetters(this, {
  ClientID: "resource://gre/modules/ClientID.jsm",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  TelemetryStorage: "resource://gre/modules/TelemetryStorage.jsm",
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.jsm",
  UpdateUtils: "resource://gre/modules/UpdateUtils.jsm",
  TelemetryArchive: "resource://gre/modules/TelemetryArchive.jsm",
  TelemetrySession: "resource://gre/modules/TelemetrySession.jsm",
  TelemetrySend: "resource://gre/modules/TelemetrySend.jsm",
  TelemetryReportingPolicy: "resource://gre/modules/TelemetryReportingPolicy.jsm",
  TelemetryModules: "resource://gre/modules/TelemetryModules.jsm",
  UpdatePing: "resource://gre/modules/UpdatePing.jsm",
  TelemetryHealthPing: "resource://gre/modules/TelemetryHealthPing.jsm",
  TelemetryEventPing: "resource://gre/modules/TelemetryEventPing.jsm",
  OS: "resource://gre/modules/osfile.jsm",
});

/**
 * Setup Telemetry logging. This function also gets called when loggin related
 * preferences change.
 */
var gLogger = null;
var gLogAppenderDump = null;
function configureLogging() {
  if (!gLogger) {
    gLogger = Log.repository.getLogger(LOGGER_NAME);

    // Log messages need to go to the browser console.
    let consoleAppender = new Log.ConsoleAppender(new Log.BasicFormatter());
    gLogger.addAppender(consoleAppender);

    Services.prefs.addObserver(PREF_BRANCH_LOG, configureLogging);
  }

  // Make sure the logger keeps up with the logging level preference.
  gLogger.level = Log.Level[Services.prefs.getStringPref(TelemetryUtils.Preferences.LogLevel, "Warn")];

  // If enabled in the preferences, add a dump appender.
  let logDumping = Services.prefs.getBoolPref(TelemetryUtils.Preferences.LogDump, false);
  if (logDumping != !!gLogAppenderDump) {
    if (logDumping) {
      gLogAppenderDump = new Log.DumpAppender(new Log.BasicFormatter());
      gLogger.addAppender(gLogAppenderDump);
    } else {
      gLogger.removeAppender(gLogAppenderDump);
      gLogAppenderDump = null;
    }
  }
}

/**
 * This is a policy object used to override behavior for testing.
 */
var Policy = {
  now: () => new Date(),
  generatePingId: () => Utils.generateUUID(),
  getCachedClientID: () => ClientID.getCachedClientID(),
};

var EXPORTED_SYMBOLS = ["TelemetryController"];

var TelemetryController = Object.freeze({
  /**
   * Used only for testing purposes.
   */
  testAssemblePing(aType, aPayload, aOptions) {
    return Impl.assemblePing(aType, aPayload, aOptions);
  },

  /**
   * Used only for testing purposes.
   */
  testInitLogging() {
    configureLogging();
  },

  /**
   * Used only for testing purposes.
   */
  testReset() {
    return Impl.reset();
  },

  /**
   * Used only for testing purposes.
   */
  testSetup() {
    return Impl.setupTelemetry(true);
  },

  /**
   * Used only for testing purposes.
   */
  testShutdown() {
    return Impl.shutdown();
  },

  /**
   * Used only for testing purposes.
   */
  testSetupContent() {
    return Impl.setupContentTelemetry(true);
  },

  /**
   * Used only for testing purposes.
   */
  testPromiseJsProbeRegistration() {
    return Promise.resolve(Impl._probeRegistrationPromise);
  },

  /**
   * Send a notification.
   */
  observe(aSubject, aTopic, aData) {
    return Impl.observe(aSubject, aTopic, aData);
  },

  /**
   * Submit ping payloads to Telemetry. This will assemble a complete ping, adding
   * environment data, client id and some general info.
   * Depending on configuration, the ping will be sent to the server (immediately or later)
   * and archived locally.
   *
   * To identify the different pings and to be able to query them pings have a type.
   * A type is a string identifier that should be unique to the type ping that is being submitted,
   * it should only contain alphanumeric characters and '-' for separation, i.e. satisfy:
   * /^[a-z0-9][a-z0-9-]+[a-z0-9]$/i
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} [aOptions] Options object.
   * @param {Boolean} [aOptions.addClientId=false] true if the ping should contain the client
   *                  id, false otherwise.
   * @param {Boolean} [aOptions.addEnvironment=false] true if the ping should contain the
   *                  environment data.
   * @param {Object}  [aOptions.overrideEnvironment=null] set to override the environment data.
   * @param {Boolean} [aOptions.usePingSender=false] if true, send the ping using the PingSender.
   * @returns {Promise} Test-only - a promise that resolves with the ping id once the ping is stored or sent.
   */
  submitExternalPing(aType, aPayload, aOptions = {}) {
    aOptions.addClientId = aOptions.addClientId || false;
    aOptions.addEnvironment = aOptions.addEnvironment || false;
    aOptions.usePingSender = aOptions.usePingSender || false;

    return Impl.submitExternalPing(aType, aPayload, aOptions);
  },

  /**
   * Get the current session ping data as it would be sent out or stored.
   *
   * @param {bool} aSubsession Whether to get subsession data. Optional, defaults to false.
   * @return {object} The current ping data if Telemetry is enabled, null otherwise.
   */
  getCurrentPingData(aSubsession = false) {
    return Impl.getCurrentPingData(aSubsession);
  },

  /**
   * Save a ping to disk.
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} [aOptions] Options object.
   * @param {Boolean} [aOptions.addClientId=false] true if the ping should contain the client
   *                  id, false otherwise.
   * @param {Boolean} [aOptions.addEnvironment=false] true if the ping should contain the
   *                  environment data.
   * @param {Boolean} [aOptions.overwrite=false] true overwrites a ping with the same name,
   *                  if found.
   * @param {Object}  [aOptions.overrideEnvironment=null] set to override the environment data.
   *
   * @returns {Promise} A promise that resolves with the ping id when the ping is saved to
   *                    disk.
   */
  addPendingPing(aType, aPayload, aOptions = {}) {
    let options = aOptions;
    options.addClientId = aOptions.addClientId || false;
    options.addEnvironment = aOptions.addEnvironment || false;
    options.overwrite = aOptions.overwrite || false;

    return Impl.addPendingPing(aType, aPayload, options);
  },

  /**
   * Check if we have an aborted-session ping from a previous session.
   * If so, submit and then remove it.
   *
   * @return {Promise} Promise that is resolved when the ping is saved.
   */
  checkAbortedSessionPing() {
    return Impl.checkAbortedSessionPing();
  },

  /**
   * Save an aborted-session ping to disk without adding it to the pending pings.
   *
   * @param {Object} aPayload The ping payload data.
   * @return {Promise} Promise that is resolved when the ping is saved.
   */
  saveAbortedSessionPing(aPayload) {
    return Impl.saveAbortedSessionPing(aPayload);
  },

  /**
   * Remove the aborted-session ping if any exists.
   *
   * @return {Promise} Promise that is resolved when the ping was removed.
   */
  removeAbortedSessionPing() {
    return Impl.removeAbortedSessionPing();
  },

  /**
   * Allows waiting for TelemetryControllers delayed initialization to complete.
   * The returned promise is guaranteed to resolve before TelemetryController is shutting down.
   * @return {Promise} Resolved when delayed TelemetryController initialization completed.
   */
  promiseInitialized() {
    return Impl.promiseInitialized();
  },
});

var Impl = {
  _initialized: false,
  _initStarted: false, // Whether we started setting up TelemetryController.
  _shuttingDown: false, // Whether the browser is shutting down.
  _shutDown: false, // Whether the browser has shut down.
  _logger: null,
  _prevValues: {},
  // The previous build ID, if this is the first run with a new build.
  // Undefined if this is not the first run, or the previous build ID is unknown.
  _previousBuildID: undefined,
  _clientID: null,
  // A task performing delayed initialization
  _delayedInitTask: null,
  // The deferred promise resolved when the initialization task completes.
  _delayedInitTaskDeferred: null,

  // This is a public barrier Telemetry clients can use to add blockers to the shutdown
  // of TelemetryController.
  // After this barrier, clients can not submit Telemetry pings anymore.
  _shutdownBarrier: new AsyncShutdown.Barrier("TelemetryController: Waiting for clients."),
  // This is a private barrier blocked by pending async ping activity (sending & saving).
  _connectionsBarrier: new AsyncShutdown.Barrier("TelemetryController: Waiting for pending ping activity"),
  // This is true when running in the test infrastructure.
  _testMode: false,
  // The task performing the delayed sending of the "new-profile" ping.
  _delayedNewPingTask: null,
  // The promise used to wait for the JS probe registration (dynamic builtin).
  _probeRegistrationPromise: null,

  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, LOGGER_PREFIX);
    }

    return this._logger;
  },

  /**
   * Get the data for the "application" section of the ping.
   */
  _getApplicationSection() {
    // Querying architecture and update channel can throw. Make sure to recover and null
    // those fields.
    let arch = null;
    try {
      arch = Services.sysinfo.get("arch");
    } catch (e) {
      this._log.trace("_getApplicationSection - Unable to get system architecture.", e);
    }

    let updateChannel = null;
    try {
      updateChannel = UpdateUtils.getUpdateChannel(false);
    } catch (e) {
      this._log.trace("_getApplicationSection - Unable to get update channel.", e);
    }

    return {
      architecture: arch,
      buildId: Services.appinfo.appBuildID,
      name: Services.appinfo.name,
      version: Services.appinfo.version,
      displayVersion: AppConstants.MOZ_APP_VERSION_DISPLAY,
      vendor: Services.appinfo.vendor,
      platformVersion: Services.appinfo.platformVersion,
      xpcomAbi: Services.appinfo.XPCOMABI,
      channel: updateChannel,
    };
  },

  /**
   * Assemble a complete ping following the common ping format specification.
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} aOptions Options object.
   * @param {Boolean} aOptions.addClientId true if the ping should contain the client
   *                  id, false otherwise.
   * @param {Boolean} aOptions.addEnvironment true if the ping should contain the
   *                  environment data.
   * @param {Object}  [aOptions.overrideEnvironment=null] set to override the environment data.
   *
   * @returns {Object} An object that contains the assembled ping data.
   */
  assemblePing: function assemblePing(aType, aPayload, aOptions = {}) {
    this._log.trace("assemblePing - Type " + aType + ", aOptions " + JSON.stringify(aOptions));

    // Clone the payload data so we don't race against unexpected changes in subobjects that are
    // still referenced by other code.
    // We can't trust all callers to do this properly on their own.
    let payload = Cu.cloneInto(aPayload, myScope);

    // Fill the common ping fields.
    let pingData = {
      type: aType,
      id: Policy.generatePingId(),
      creationDate: (Policy.now()).toISOString(),
      version: PING_FORMAT_VERSION,
      application: this._getApplicationSection(),
      payload,
    };

    if (aOptions.addClientId) {
      pingData.clientId = this._clientID;
    }

    if (aOptions.addEnvironment) {
      pingData.environment = aOptions.overrideEnvironment || TelemetryEnvironment.currentEnvironment;
    }

    return pingData;
  },

  /**
   * Track any pending ping send and save tasks through the promise passed here.
   * This is needed to block shutdown on any outstanding ping activity.
   */
  _trackPendingPingTask(aPromise) {
    this._connectionsBarrier.client.addBlocker("Waiting for ping task", aPromise);
  },

  /**
   * Internal function to assemble a complete ping, adding environment data, client id
   * and some general info. This waits on the client id to be loaded/generated if it's
   * not yet available. Note that this function is synchronous unless we need to load
   * the client id.
   * Depending on configuration, the ping will be sent to the server (immediately or later)
   * and archived locally.
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} [aOptions] Options object.
   * @param {Boolean} [aOptions.addClientId=false] true if the ping should contain the client
   *                  id, false otherwise.
   * @param {Boolean} [aOptions.addEnvironment=false] true if the ping should contain the
   *                  environment data.
   * @param {Object}  [aOptions.overrideEnvironment=null] set to override the environment data.
   * @param {Boolean} [aOptions.usePingSender=false] if true, send the ping using the PingSender.
   * @returns {Promise} Test-only - a promise that is resolved with the ping id once the ping is stored or sent.
   */
  async _submitPingLogic(aType, aPayload, aOptions) {
    // Make sure to have a clientId if we need one. This cover the case of submitting
    // a ping early during startup, before Telemetry is initialized, if no client id was
    // cached.
    if (!this._clientID && aOptions.addClientId) {
      Telemetry.getHistogramById("TELEMETRY_PING_SUBMISSION_WAITING_CLIENTID").add();
      // We can safely call |getClientID| here and during initialization: we would still
      // spawn and return one single loading task.
      this._clientID = await ClientID.getClientID();
    }

    const pingData = this.assemblePing(aType, aPayload, aOptions);
    this._log.trace("submitExternalPing - ping assembled, id: " + pingData.id);

    // Always persist the pings if we are allowed to. We should not yield on any of the
    // following operations to keep this function synchronous for the majority of the calls.
    let archivePromise = TelemetryArchive.promiseArchivePing(pingData)
      .catch(e => this._log.error("submitExternalPing - Failed to archive ping " + pingData.id, e));
    let p = [ archivePromise ];

    p.push(TelemetrySend.submitPing(pingData, {usePingSender: aOptions.usePingSender}));

    return Promise.all(p).then(() => pingData.id);
  },

  /**
   * Submit ping payloads to Telemetry.
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} [aOptions] Options object.
   * @param {Boolean} [aOptions.addClientId=false] true if the ping should contain the client
   *                  id, false otherwise.
   * @param {Boolean} [aOptions.addEnvironment=false] true if the ping should contain the
   *                  environment data.
   * @param {Object}  [aOptions.overrideEnvironment=null] set to override the environment data.
   * @param {Boolean} [aOptions.usePingSender=false] if true, send the ping using the PingSender.
   * @returns {Promise} Test-only - a promise that is resolved with the ping id once the ping is stored or sent.
   */
  submitExternalPing: function send(aType, aPayload, aOptions) {
    this._log.trace("submitExternalPing - type: " + aType + ", aOptions: " + JSON.stringify(aOptions));

    // Reject pings sent after shutdown.
    if (this._shutDown) {
      const errorMessage = "submitExternalPing - Submission is not allowed after shutdown, discarding ping of type: " + aType;
      this._log.error(errorMessage);
      return Promise.reject(new Error(errorMessage));
    }

    // Enforce the type string to only contain sane characters.
    const typeUuid = /^[a-z0-9][a-z0-9-]+[a-z0-9]$/i;
    if (!typeUuid.test(aType)) {
      this._log.error("submitExternalPing - invalid ping type: " + aType);
      let histogram = Telemetry.getKeyedHistogramById("TELEMETRY_INVALID_PING_TYPE_SUBMITTED");
      histogram.add(aType, 1);
      return Promise.reject(new Error("Invalid type string submitted."));
    }
    // Enforce that the payload is an object.
    if (aPayload === null || typeof aPayload !== "object" || Array.isArray(aPayload)) {
      this._log.error("submitExternalPing - invalid payload type: " + typeof aPayload);
      let histogram = Telemetry.getHistogramById("TELEMETRY_INVALID_PAYLOAD_SUBMITTED");
      histogram.add(1);
      return Promise.reject(new Error("Invalid payload type submitted."));
    }

    let promise = this._submitPingLogic(aType, aPayload, aOptions);
    this._trackPendingPingTask(promise);
    return promise;
  },

  /**
   * Save a ping to disk.
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} aOptions Options object.
   * @param {Boolean} aOptions.addClientId true if the ping should contain the client id,
   *                  false otherwise.
   * @param {Boolean} aOptions.addEnvironment true if the ping should contain the
   *                  environment data.
   * @param {Boolean} aOptions.overwrite true overwrites a ping with the same name, if found.
   * @param {Object}  [aOptions.overrideEnvironment=null] set to override the environment data.
   *
   * @returns {Promise} A promise that resolves with the ping id when the ping is saved to
   *                    disk.
   */
  addPendingPing: function addPendingPing(aType, aPayload, aOptions) {
    this._log.trace("addPendingPing - Type " + aType + ", aOptions " + JSON.stringify(aOptions));

    let pingData = this.assemblePing(aType, aPayload, aOptions);

    let savePromise = TelemetryStorage.savePendingPing(pingData);
    let archivePromise = TelemetryArchive.promiseArchivePing(pingData).catch(e => {
      this._log.error("addPendingPing - Failed to archive ping " + pingData.id, e);
    });

    // Wait for both the archiving and ping persistence to complete.
    let promises = [
      savePromise,
      archivePromise,
    ];
    return Promise.all(promises).then(() => pingData.id);
  },

  /**
   * Check whether we have an aborted-session ping. If so add it to the pending pings and archive it.
   *
   * @return {Promise} Promise that is resolved when the ping is submitted and archived.
   */
  async checkAbortedSessionPing() {
    let ping = await TelemetryStorage.loadAbortedSessionPing();
    this._log.trace("checkAbortedSessionPing - found aborted-session ping: " + !!ping);
    if (!ping) {
      return;
    }

    try {
      await TelemetryStorage.addPendingPing(ping);
      await TelemetryArchive.promiseArchivePing(ping);
    } catch (e) {
      this._log.error("checkAbortedSessionPing - Unable to add the pending ping", e);
    } finally {
      await TelemetryStorage.removeAbortedSessionPing();
    }
  },

  /**
   * Save an aborted-session ping to disk without adding it to the pending pings.
   *
   * @param {Object} aPayload The ping payload data.
   * @return {Promise} Promise that is resolved when the ping is saved.
   */
  saveAbortedSessionPing(aPayload) {
    this._log.trace("saveAbortedSessionPing");
    const options = {addClientId: true, addEnvironment: true};
    const pingData = this.assemblePing(PING_TYPE_MAIN, aPayload, options);
    return TelemetryStorage.saveAbortedSessionPing(pingData);
  },

  removeAbortedSessionPing() {
    return TelemetryStorage.removeAbortedSessionPing();
  },

  /**
   * Perform telemetry initialization for either chrome or content process.
   * @return {Boolean} True if Telemetry is allowed to record at least base (FHR) data,
   *                   false otherwise.
   */
  enableTelemetryRecording: function enableTelemetryRecording() {
    // The thumbnail service also runs in a content process, even with e10s off.
    // We need to check if e10s is on so we don't submit child payloads for it.
    // We still need xpcshell child tests to work, so we skip this if test mode is enabled.
    if (Utils.isContentProcess && !this._testMode && !Services.appinfo.browserTabsRemoteAutostart) {
      this._log.config("enableTelemetryRecording - not enabling Telemetry for non-e10s child process");
      Telemetry.canRecordBase = false;
      Telemetry.canRecordExtended = false;
      return false;
    }

    // Configure base Telemetry recording.
    // Unified Telemetry makes it opt-out. If extended Telemetry is enabled, base recording
    // is always on as well.
    if (IS_UNIFIED_TELEMETRY) {
      TelemetryUtils.setTelemetryRecordingFlags();
    } else {
      // We're not on unified Telemetry, stick to the old behaviour for
      // supporting Fennec.
      Telemetry.canRecordBase = Telemetry.canRecordExtended = Utils.isTelemetryEnabled;
    }

    this._log.config("enableTelemetryRecording - canRecordBase:" + Telemetry.canRecordBase +
                     ", canRecordExtended: " + Telemetry.canRecordExtended);

    return Telemetry.canRecordBase;
  },

  /**
   * This triggers basic telemetry initialization and schedules a full initialized for later
   * for performance reasons.
   *
   * This delayed initialization means TelemetryController init can be in the following states:
   * 1) setupTelemetry was never called
   * or it was called and
   *   2) _delayedInitTask was scheduled, but didn't run yet.
   *   3) _delayedInitTask is currently running.
   *   4) _delayedInitTask finished running and is nulled out.
   *
   * @return {Promise} Resolved when TelemetryController and TelemetrySession are fully
   *                   initialized. This is only used in tests.
   */
  setupTelemetry: function setupTelemetry(testing) {
    this._initStarted = true;
    this._shuttingDown = false;
    this._shutDown = false;
    this._testMode = testing;

    this._log.trace("setupTelemetry");

    if (this._delayedInitTask) {
      this._log.error("setupTelemetry - init task already running");
      return this._delayedInitTaskDeferred.promise;
    }

    if (this._initialized && !this._testMode) {
      this._log.error("setupTelemetry - already initialized");
      return Promise.resolve();
    }

    // Enable adding scalars in artifact builds and build faster modes.
    // The function is async: we intentionally don't wait for it to complete
    // as we don't want to delay startup.
    this._probeRegistrationPromise = this.registerJsProbes();

    // This will trigger displaying the datachoices infobar.
    TelemetryReportingPolicy.setup();

    if (!this.enableTelemetryRecording()) {
      this._log.config("setupChromeProcess - Telemetry recording is disabled, skipping Chrome process setup.");
      return Promise.resolve();
    }

    this._attachObservers();

    // Perform a lightweight, early initialization for the component, just registering
    // a few observers and initializing the session.
    TelemetrySession.earlyInit(this._testMode);

    // Annotate crash reports so that we get pings for startup crashes
    TelemetrySend.earlyInit();

    // For very short session durations, we may never load the client
    // id from disk.
    // We try to cache it in prefs to avoid this, even though this may
    // lead to some stale client ids.
    this._clientID = ClientID.getCachedClientID();

    // Init the update ping telemetry as early as possible. This won't have
    // an impact on startup.
    UpdatePing.earlyInit();

    // Delay full telemetry initialization to give the browser time to
    // run various late initializers. Otherwise our gathered memory
    // footprint and other numbers would be too optimistic.
    this._delayedInitTaskDeferred = PromiseUtils.defer();
    this._delayedInitTask = new DeferredTask(async () => {
      try {
        // TODO: This should probably happen after all the delayed init here.
        this._initialized = true;
        TelemetryEnvironment.delayedInit();

        // Load the ClientID.
        this._clientID = await ClientID.getClientID();

        await TelemetrySend.setup(this._testMode);

        // Perform TelemetrySession delayed init.
        await TelemetrySession.delayedInit();

        if (Services.prefs.getBoolPref(TelemetryUtils.Preferences.NewProfilePingEnabled, false) &&
            !TelemetrySession.newProfilePingSent) {
          // Kick off the scheduling of the new-profile ping.
          this.scheduleNewProfilePing();
        }

        // Purge the pings archive by removing outdated pings. We don't wait for
        // this task to complete, but TelemetryStorage blocks on it during
        // shutdown.
        TelemetryStorage.runCleanPingArchiveTask();

        // Now that FHR/healthreporter is gone, make sure to remove FHR's DB from
        // the profile directory. This is a temporary measure that we should drop
        // in the future.
        TelemetryStorage.removeFHRDatabase();

        // The init sequence is forced to run on shutdown for short sessions and
        // we don't want to start TelemetryModules as the timer registration will fail.
        if (!this._shuttingDown) {
          // Report the modules loaded in the Firefox process.
          TelemetryModules.start();
        }

        TelemetryEventPing.startup();

        this._delayedInitTaskDeferred.resolve();
      } catch (e) {
        this._delayedInitTaskDeferred.reject(e);
      } finally {
        this._delayedInitTask = null;
      }
    }, this._testMode ? TELEMETRY_TEST_DELAY : TELEMETRY_DELAY,
       this._testMode ? 0 : undefined);

    AsyncShutdown.sendTelemetry.addBlocker("TelemetryController: shutting down",
                                           () => this.shutdown(),
                                           () => this._getState());

    this._delayedInitTask.arm();
    return this._delayedInitTaskDeferred.promise;
  },

  /**
   * This triggers basic telemetry initialization for content processes.
   * @param {Boolean} [testing=false] True if we are in test mode, false otherwise.
   */
  setupContentTelemetry(testing = false) {
    this._testMode = testing;

    // We call |enableTelemetryRecording| here to make sure that Telemetry.canRecord* flags
    // are in sync between chrome and content processes.
    if (!this.enableTelemetryRecording()) {
      this._log.trace("setupContentTelemetry - Content process recording disabled.");
      return;
    }
    TelemetrySession.setupContent(testing);
  },

  // Do proper shutdown waiting and cleanup.
  async _cleanupOnShutdown() {
    if (!this._initialized) {
      return;
    }

    Services.prefs.removeObserver(PREF_BRANCH_LOG, configureLogging);
    this._detachObservers();

    // Now do an orderly shutdown.
    try {
      if (this._delayedNewPingTask) {
        await this._delayedNewPingTask.finalize();
      }

      UpdatePing.shutdown();

      TelemetryEventPing.shutdown();

      // Stop the datachoices infobar display.
      TelemetryReportingPolicy.shutdown();
      TelemetryEnvironment.shutdown();

      // Stop any ping sending.
      await TelemetrySend.shutdown();

      // Send latest data.
      await TelemetryHealthPing.shutdown();

      await TelemetrySession.shutdown();

      // First wait for clients processing shutdown.
      await this._shutdownBarrier.wait();

      // ... and wait for any outstanding async ping activity.
      await this._connectionsBarrier.wait();

      // Perform final shutdown operations.
      await TelemetryStorage.shutdown();
    } finally {
      // Reset state.
      this._initialized = false;
      this._initStarted = false;
      this._shutDown = true;
    }
  },

  shutdown() {
    this._log.trace("shutdown");

    this._shuttingDown = true;

    // We can be in one the following states here:
    // 1) setupTelemetry was never called
    // or it was called and
    //   2) _delayedInitTask was scheduled, but didn't run yet.
    //   3) _delayedInitTask is running now.
    //   4) _delayedInitTask finished running already.

    // This handles 1).
    if (!this._initStarted) {
      this._shutDown = true;
      return Promise.resolve();
    }

    // This handles 4).
    if (!this._delayedInitTask) {
      // We already ran the delayed initialization.
      return this._cleanupOnShutdown();
    }

    // This handles 2) and 3).
    return this._delayedInitTask.finalize().then(() => this._cleanupOnShutdown());
  },

  /**
   * This observer drives telemetry.
   */
  observe(aSubject, aTopic, aData) {
    // The logger might still be not available at this point.
    if (aTopic == "profile-after-change" || aTopic == "app-startup") {
      // If we don't have a logger, we need to make sure |Log.repository.getLogger()| is
      // called before |getLoggerWithMessagePrefix|. Otherwise logging won't work.
      configureLogging();
    }

    this._log.trace("observe - " + aTopic + " notified.");

    switch (aTopic) {
    case "profile-after-change":
      // profile-after-change is only registered for chrome processes.
      return this.setupTelemetry();
    case "app-startup":
      // app-startup is only registered for content processes.
      return this.setupContentTelemetry();
    case "nsPref:changed":
      if (aData == TelemetryUtils.Preferences.FhrUploadEnabled) {
        return this._onUploadPrefChange();
      }
    }
    return undefined;
  },

  /**
   * Get an object describing the current state of this module for AsyncShutdown diagnostics.
   */
  _getState() {
    return {
      initialized: this._initialized,
      initStarted: this._initStarted,
      haveDelayedInitTask: !!this._delayedInitTask,
      shutdownBarrier: this._shutdownBarrier.state,
      connectionsBarrier: this._connectionsBarrier.state,
      sendModule: TelemetrySend.getShutdownState(),
      haveDelayedNewProfileTask: !!this._delayedNewPingTask,
    };
  },

  /**
   * Called whenever the FHR Upload preference changes (e.g. when user disables FHR from
   * the preferences panel), this triggers sending the deletion ping.
   */
  _onUploadPrefChange() {
    const uploadEnabled = Services.prefs.getBoolPref(TelemetryUtils.Preferences.FhrUploadEnabled, false);
    if (uploadEnabled) {
      // There's nothing we should do if we are enabling upload.
      return;
    }

    let p = (async () => {
      try {
        // Clear the current pings.
        await TelemetrySend.clearCurrentPings();

        // Remove all the pending pings, but not the deletion ping.
        await TelemetryStorage.runRemovePendingPingsTask();
      } catch (e) {
        this._log.error("_onUploadPrefChange - error clearing pending pings", e);
      } finally {
        // Always send the deletion ping.
        this._log.trace("_onUploadPrefChange - Sending deletion ping.");
        this.submitExternalPing(PING_TYPE_DELETION, {}, { addClientId: true });
      }
    })();

    this._shutdownBarrier.client.addBlocker(
      "TelemetryController: removing pending pings after data upload was disabled", p);
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsISupportsWeakReference]),

  _attachObservers() {
    if (IS_UNIFIED_TELEMETRY) {
      // Watch the FHR upload setting to trigger deletion pings.
      Services.prefs.addObserver(TelemetryUtils.Preferences.FhrUploadEnabled, this, true);
    }
  },

  /**
   * Remove the preference observer to avoid leaks.
   */
  _detachObservers() {
    if (IS_UNIFIED_TELEMETRY) {
      Services.prefs.removeObserver(TelemetryUtils.Preferences.FhrUploadEnabled, this);
    }
  },

  /**
   * Allows waiting for TelemetryControllers delayed initialization to complete.
   * This will complete before TelemetryController is shutting down.
   * @return {Promise} Resolved when delayed TelemetryController initialization completed.
   */
  promiseInitialized() {
    return this._delayedInitTaskDeferred.promise;
  },

  getCurrentPingData(aSubsession) {
    this._log.trace("getCurrentPingData - subsession: " + aSubsession);

    // Telemetry is disabled, don't gather any data.
    if (!Telemetry.canRecordBase) {
      return null;
    }

    const reason = aSubsession ? REASON_GATHER_SUBSESSION_PAYLOAD : REASON_GATHER_PAYLOAD;
    const type = PING_TYPE_MAIN;
    const payload = TelemetrySession.getPayload(reason);
    const options = { addClientId: true, addEnvironment: true };
    const ping = this.assemblePing(type, payload, options);

    return ping;
  },

  async reset() {
    this._clientID = null;
    this._detachObservers();

    let sessionReset = TelemetrySession.testReset();

    this._connectionsBarrier = new AsyncShutdown.Barrier(
      "TelemetryController: Waiting for pending ping activity"
    );
    this._shutdownBarrier = new AsyncShutdown.Barrier(
      "TelemetryController: Waiting for clients."
    );

    // We need to kick of the controller setup first for tests that check the
    // cached client id.
    let controllerSetup = this.setupTelemetry(true);

    await sessionReset;
    await TelemetrySend.reset();
    await TelemetryStorage.reset();
    await TelemetryEnvironment.testReset();

    await controllerSetup;
  },

  /**
   * Schedule sending the "new-profile" ping.
   */
  scheduleNewProfilePing() {
    this._log.trace("scheduleNewProfilePing");

    const sendDelay =
      Services.prefs.getIntPref(TelemetryUtils.Preferences.NewProfilePingDelay, NEWPROFILE_PING_DEFAULT_DELAY);

    this._delayedNewPingTask = new DeferredTask(async () => {
      try {
        await this.sendNewProfilePing();
      } finally {
        this._delayedNewPingTask = null;
      }
    }, sendDelay);

    this._delayedNewPingTask.arm();
  },

  /**
   * Generate and send the new-profile ping
   */
  async sendNewProfilePing() {
    this._log.trace("sendNewProfilePing - shutting down: " + this._shuttingDown);

    // Generate the payload.
    const payload = {
      "reason": this._shuttingDown ? "shutdown" : "startup",
    };

    // Generate and send the "new-profile" ping. This uses the
    // pingsender if we're shutting down.
    let options = {
      addClientId: true,
      addEnvironment: true,
      usePingSender: this._shuttingDown,
    };
    // TODO: we need to be smarter about when to send the ping (and save the
    // state to file). |requestIdleCallback| is currently only accessible
    // through DOM. See bug 1361996.
    await TelemetryController.submitExternalPing("new-profile", payload, options)
                             .then(() => TelemetrySession.markNewProfilePingSent(),
                                   e => this._log.error("sendNewProfilePing - failed to submit new-profile ping", e));
  },

  /**
   * Register 'dynamic builtin' probes from the JSON definition files.
   * This is needed to support adding new probes in developer builds
   * without rebuilding the whole codebase.
   *
   * This is not meant to be used outside of local developer builds.
   */
  async registerJsProbes() {
    // We don't support this outside of developer builds.
    if (AppConstants.MOZILLA_OFFICIAL && !this._testMode) {
      return;
    }

    this._log.trace("registerJsProbes - registering builtin JS probes");

    await this.registerScalarProbes();
    await this.registerEventProbes();
  },

  _loadProbeDefinitions(filename) {
    let probeFile = Services.dirsvc.get("GreD", Ci.nsIFile);
    probeFile.append(filename);
    if (!probeFile.exists()) {
      this._log.trace(`loadProbeDefinitions - no builtin JS probe file ${filename}`);
      return null;
    }

    return OS.File.read(probeFile.path, { encoding: "utf-8" });
  },

  async registerScalarProbes() {
    this._log.trace("registerScalarProbes - registering scalar builtin JS probes");

    // Load the scalar probes JSON file.
    const scalarProbeFilename = "ScalarArtifactDefinitions.json";
    let scalarJSProbes = {};
    try {
      let fileContent = await this._loadProbeDefinitions(scalarProbeFilename);
      scalarJSProbes = JSON.parse(fileContent, (property, value) => {
        // Fixup the "kind" property: it's a string, and we need the constant
        // coming from nsITelemetry.
        if (property !== "kind" || typeof value != "string") {
          return value;
        }

        let newValue;
        switch (value) {
          case "nsITelemetry::SCALAR_TYPE_COUNT":
            newValue = Telemetry.SCALAR_TYPE_COUNT;
            break;
          case "nsITelemetry::SCALAR_TYPE_BOOLEAN":
            newValue = Telemetry.SCALAR_TYPE_BOOLEAN;
            break;
          case "nsITelemetry::SCALAR_TYPE_STRING":
            newValue = Telemetry.SCALAR_TYPE_STRING;
            break;
        }
        return newValue;
      });
    } catch (ex) {
      this._log.error(`registerScalarProbes - there was an error loading ${scalarProbeFilename}`,
                      ex);
    }

    // Register the builtin probes.
    for (let category in scalarJSProbes) {
      Telemetry.registerBuiltinScalars(category, scalarJSProbes[category]);
    }
  },

  async registerEventProbes() {
    this._log.trace("registerEventProbes - registering builtin JS Event probes");

    // Load the event probes JSON file.
    const eventProbeFilename = "EventArtifactDefinitions.json";
    let eventJSProbes = {};
    try {
      let fileContent = await this._loadProbeDefinitions(eventProbeFilename);
      eventJSProbes = JSON.parse(fileContent);
    } catch (ex) {
      this._log.error(`registerEventProbes - there was an error loading ${eventProbeFilename}`,
                      ex);
    }

    // Register the builtin probes.
    for (let category in eventJSProbes) {
      Telemetry.registerBuiltinEvents(category, eventJSProbes[category]);
    }
  },
};
