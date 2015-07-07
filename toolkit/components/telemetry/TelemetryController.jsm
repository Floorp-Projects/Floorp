/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;
const myScope = this;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/debug.js", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/PromiseUtils.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/DeferredTask.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/TelemetryUtils.jsm", this);

const Utils = TelemetryUtils;

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetryController::";

const PREF_BRANCH = "toolkit.telemetry.";
const PREF_BRANCH_LOG = PREF_BRANCH + "log.";
const PREF_SERVER = PREF_BRANCH + "server";
const PREF_ENABLED = PREF_BRANCH + "enabled";
const PREF_LOG_LEVEL = PREF_BRANCH_LOG + "level";
const PREF_LOG_DUMP = PREF_BRANCH_LOG + "dump";
const PREF_CACHED_CLIENTID = PREF_BRANCH + "cachedClientID";
const PREF_FHR_ENABLED = "datareporting.healthreport.service.enabled";
const PREF_FHR_UPLOAD_ENABLED = "datareporting.healthreport.uploadEnabled";
const PREF_SESSIONS_BRANCH = "datareporting.sessions.";
const PREF_UNIFIED = PREF_BRANCH + "unified";

// Whether the FHR/Telemetry unification features are enabled.
// Changing this pref requires a restart.
const IS_UNIFIED_TELEMETRY = Preferences.get(PREF_UNIFIED, false);

const PING_FORMAT_VERSION = 4;

// Delay before intializing telemetry (ms)
const TELEMETRY_DELAY = 60000;
// Delay before initializing telemetry if we're testing (ms)
const TELEMETRY_TEST_DELAY = 100;

// Ping types.
const PING_TYPE_MAIN = "main";
const PING_TYPE_DELETION = "deletion";

// Session ping reasons.
const REASON_GATHER_PAYLOAD = "gather-payload";
const REASON_GATHER_SUBSESSION_PAYLOAD = "gather-subsession-payload";

XPCOMUtils.defineLazyModuleGetter(this, "ClientID",
                                  "resource://gre/modules/ClientID.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "Telemetry",
                                   "@mozilla.org/base/telemetry;1",
                                   "nsITelemetry");
XPCOMUtils.defineLazyModuleGetter(this, "AsyncShutdown",
                                  "resource://gre/modules/AsyncShutdown.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryStorage",
                                  "resource://gre/modules/TelemetryStorage.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ThirdPartyCookieProbe",
                                  "resource://gre/modules/ThirdPartyCookieProbe.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryEnvironment",
                                  "resource://gre/modules/TelemetryEnvironment.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SessionRecorder",
                                  "resource://gre/modules/SessionRecorder.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UpdateChannel",
                                  "resource://gre/modules/UpdateChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryArchive",
                                  "resource://gre/modules/TelemetryArchive.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetrySession",
                                  "resource://gre/modules/TelemetrySession.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetrySend",
                                  "resource://gre/modules/TelemetrySend.jsm");

/**
 * Setup Telemetry logging. This function also gets called when loggin related
 * preferences change.
 */
let gLogger = null;
let gLogAppenderDump = null;
function configureLogging() {
  if (!gLogger) {
    gLogger = Log.repository.getLogger(LOGGER_NAME);

    // Log messages need to go to the browser console.
    let consoleAppender = new Log.ConsoleAppender(new Log.BasicFormatter());
    gLogger.addAppender(consoleAppender);

    Preferences.observe(PREF_BRANCH_LOG, configureLogging);
  }

  // Make sure the logger keeps up with the logging level preference.
  gLogger.level = Log.Level[Preferences.get(PREF_LOG_LEVEL, "Warn")];

  // If enabled in the preferences, add a dump appender.
  let logDumping = Preferences.get(PREF_LOG_DUMP, false);
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
let Policy = {
  now: () => new Date(),
}

this.EXPORTED_SYMBOLS = ["TelemetryController"];

this.TelemetryController = Object.freeze({
  Constants: Object.freeze({
    PREF_ENABLED: PREF_ENABLED,
    PREF_LOG_LEVEL: PREF_LOG_LEVEL,
    PREF_LOG_DUMP: PREF_LOG_DUMP,
    PREF_SERVER: PREF_SERVER,
  }),
  /**
   * Used only for testing purposes.
   */
  initLogging: function() {
    configureLogging();
  },
  /**
   * Used only for testing purposes.
   */
  reset: function() {
    Impl._clientID = null;
    Impl._detachObservers();
    TelemetryStorage.reset();
    TelemetrySend.reset();

    return this.setup();
  },
  /**
   * Used only for testing purposes.
   */
  setup: function() {
    return Impl.setupTelemetry(true);
  },

  /**
   * Used only for testing purposes.
   */
  setupContent: function() {
    return Impl.setupContentTelemetry(true);
  },

  /**
   * Send a notification.
   */
  observe: function (aSubject, aTopic, aData) {
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
   * @returns {Promise} Test-only - a promise that resolves with the ping id once the ping is stored or sent.
   */
  submitExternalPing: function(aType, aPayload, aOptions = {}) {
    aOptions.addClientId = aOptions.addClientId || false;
    aOptions.addEnvironment = aOptions.addEnvironment || false;

    const testOnly = Impl.submitExternalPing(aType, aPayload, aOptions);
    return testOnly;
  },

  /**
   * Get the current session ping data as it would be sent out or stored.
   *
   * @param {bool} aSubsession Whether to get subsession data. Optional, defaults to false.
   * @return {object} The current ping data in object form.
   */
  getCurrentPingData: function(aSubsession = false) {
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
  addPendingPing: function(aType, aPayload, aOptions = {}) {
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
  checkAbortedSessionPing: function() {
    return Impl.checkAbortedSessionPing();
  },

  /**
   * Save an aborted-session ping to disk without adding it to the pending pings.
   *
   * @param {Object} aPayload The ping payload data.
   * @return {Promise} Promise that is resolved when the ping is saved.
   */
  saveAbortedSessionPing: function(aPayload) {
    return Impl.saveAbortedSessionPing(aPayload);
  },

  /**
   * Remove the aborted-session ping if any exists.
   *
   * @return {Promise} Promise that is resolved when the ping was removed.
   */
  removeAbortedSessionPing: function() {
    return Impl.removeAbortedSessionPing();
  },

  /**
   * Write a ping to a specified location on the disk. Does not add the ping to the
   * pending pings.
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {String} aFilePath The path to save the ping to.
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
  savePing: function(aType, aPayload, aFilePath, aOptions = {}) {
    let options = aOptions;
    options.addClientId = aOptions.addClientId || false;
    options.addEnvironment = aOptions.addEnvironment || false;
    options.overwrite = aOptions.overwrite || false;

    return Impl.savePing(aType, aPayload, aFilePath, options);
  },

  /**
   * The client id send with the telemetry ping.
   *
   * @return The client id as string, or null.
   */
  get clientID() {
    return Impl.clientID;
  },

  /**
   * The AsyncShutdown.Barrier to synchronize with TelemetryController shutdown.
   */
  get shutdown() {
    return Impl._shutdownBarrier.client;
  },

  /**
   * The session recorder instance managed by Telemetry.
   * @return {Object} The active SessionRecorder instance or null if not available.
   */
  getSessionRecorder: function() {
    return Impl._sessionRecorder;
  },

  /**
   * Allows waiting for TelemetryControllers delayed initialization to complete.
   * The returned promise is guaranteed to resolve before TelemetryController is shutting down.
   * @return {Promise} Resolved when delayed TelemetryController initialization completed.
   */
  promiseInitialized: function() {
    return Impl.promiseInitialized();
  },
});

let Impl = {
  _initialized: false,
  _initStarted: false, // Whether we started setting up TelemetryController.
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

  // The session recorder, shared with FHR and the Data Reporting Service.
  _sessionRecorder: null,
  // This is a public barrier Telemetry clients can use to add blockers to the shutdown
  // of TelemetryController.
  // After this barrier, clients can not submit Telemetry pings anymore.
  _shutdownBarrier: new AsyncShutdown.Barrier("TelemetryController: Waiting for clients."),
  // This is a private barrier blocked by pending async ping activity (sending & saving).
  _connectionsBarrier: new AsyncShutdown.Barrier("TelemetryController: Waiting for pending ping activity"),
  // This is true when running in the test infrastructure.
  _testMode: false,

  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, LOGGER_PREFIX);
    }

    return this._logger;
  },

  /**
   * Get the data for the "application" section of the ping.
   */
  _getApplicationSection: function() {
    // Querying architecture and update channel can throw. Make sure to recover and null
    // those fields.
    let arch = null;
    try {
      arch = Services.sysinfo.get("arch");
    } catch (e) {
      this._log.trace("assemblePing - Unable to get system architecture.", e);
    }

    let updateChannel = null;
    try {
      updateChannel = UpdateChannel.get(false);
    } catch (e) {
      this._log.trace("assemblePing - Unable to get update channel.", e);
    }

    return {
      architecture: arch,
      buildId: Services.appinfo.appBuildID,
      name: Services.appinfo.name,
      version: Services.appinfo.version,
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
   * @returns Promise<Object> A promise that resolves when the ping is completely assembled.
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
      id: Utils.generateUUID(),
      creationDate: (Policy.now()).toISOString(),
      version: PING_FORMAT_VERSION,
      application: this._getApplicationSection(),
      payload: payload,
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
  _trackPendingPingTask: function (aPromise) {
    this._connectionsBarrier.client.addBlocker("Waiting for ping task", aPromise);
  },

  /**
   * Submit ping payloads to Telemetry. This will assemble a complete ping, adding
   * environment data, client id and some general info.
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
   * @returns {Promise} Test-only - a promise that is resolved with the ping id once the ping is stored or sent.
   */
  submitExternalPing: function send(aType, aPayload, aOptions) {
    this._log.trace("submitExternalPing - type: " + aType + ", aOptions: " + JSON.stringify(aOptions));

    // Enforce the type string to only contain sane characters.
    const typeUuid = /^[a-z0-9][a-z0-9-]+[a-z0-9]$/i;
    if (!typeUuid.test(aType)) {
      this._log.error("submitExternalPing - invalid ping type: " + aType);
      let histogram = Telemetry.getKeyedHistogramById("TELEMETRY_INVALID_PING_TYPE_SUBMITTED");
      histogram.add(aType, 1);
      return Promise.reject(new Error("Invalid type string submitted."));
    }

    const pingData = this.assemblePing(aType, aPayload, aOptions);
    this._log.trace("submitExternalPing - ping assembled, id: " + pingData.id);

    // Always persist the pings if we are allowed to.
    let archivePromise = TelemetryArchive.promiseArchivePing(pingData)
      .catch(e => this._log.error("submitExternalPing - Failed to archive ping " + pingData.id, e));
    let p = [ archivePromise ];

    p.push(TelemetrySend.submitPing(pingData));

    let promise = Promise.all(p);
    this._trackPendingPingTask(promise);
    return promise.then(() => pingData.id);
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
   * Write a ping to a specified location on the disk. Does not add the ping to the
   * pending pings.
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {String} aFilePath The path to save the ping to.
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
  savePing: function savePing(aType, aPayload, aFilePath, aOptions) {
    this._log.trace("savePing - Type " + aType + ", File Path " + aFilePath +
                    ", aOptions " + JSON.stringify(aOptions));
    let pingData = this.assemblePing(aType, aPayload, aOptions);
    return TelemetryStorage.savePingToFile(pingData, aFilePath, aOptions.overwrite)
                        .then(() => pingData.id);
  },

  /**
   * Check whether we have an aborted-session ping. If so add it to the pending pings and archive it.
   *
   * @return {Promise} Promise that is resolved when the ping is submitted and archived.
   */
  checkAbortedSessionPing: Task.async(function*() {
    let ping = yield TelemetryStorage.loadAbortedSessionPing();
    this._log.trace("checkAbortedSessionPing - found aborted-session ping: " + !!ping);
    if (!ping) {
      return;
    }

    try {
      yield TelemetryStorage.addPendingPing(ping);
      yield TelemetryArchive.promiseArchivePing(ping);
    } catch (e) {
      this._log.error("checkAbortedSessionPing - Unable to add the pending ping", e);
    } finally {
      yield TelemetryStorage.removeAbortedSessionPing();
    }
  }),

  /**
   * Save an aborted-session ping to disk without adding it to the pending pings.
   *
   * @param {Object} aPayload The ping payload data.
   * @return {Promise} Promise that is resolved when the ping is saved.
   */
  saveAbortedSessionPing: function(aPayload) {
    this._log.trace("saveAbortedSessionPing");
    const options = {addClientId: true, addEnvironment: true};
    const pingData = this.assemblePing(PING_TYPE_MAIN, aPayload, options);
    return TelemetryStorage.saveAbortedSessionPing(pingData);
  },

  removeAbortedSessionPing: function() {
    return TelemetryStorage.removeAbortedSessionPing();
  },

  /**
   * Perform telemetry initialization for either chrome or content process.
   * @return {Boolean} True if Telemetry is allowed to record at least base (FHR) data,
   *                   false otherwise.
   */
  enableTelemetryRecording: function enableTelemetryRecording() {
    const enabled = Preferences.get(PREF_ENABLED, false);

    // Enable base Telemetry recording, if needed.
    Telemetry.canRecordBase = enabled || IS_UNIFIED_TELEMETRY;

#ifdef MOZILLA_OFFICIAL
    // Enable extended telemetry if:
    //  * the telemetry preference is set and
    //  * this is an official build or we are in test-mode
    // We only do the latter check for official builds so that e.g. developer builds
    // still enable Telemetry based on prefs.
    Telemetry.canRecordExtended = enabled && (Telemetry.isOfficialTelemetry || this._testMode);
#else
    // Turn off extended telemetry recording if disabled by preferences or if base/telemetry
    // telemetry recording is off.
    Telemetry.canRecordExtended = enabled;
#endif

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
   */
  setupTelemetry: function setupTelemetry(testing) {
    this._initStarted = true;
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

    // Only initialize the session recorder if FHR is enabled.
    // TODO: move this after the |enableTelemetryRecording| block and drop the
    // PREF_FHR_ENABLED check once we permanently switch over to unified Telemetry.
    if (!this._sessionRecorder &&
        (Preferences.get(PREF_FHR_ENABLED, true) || IS_UNIFIED_TELEMETRY)) {
      this._sessionRecorder = new SessionRecorder(PREF_SESSIONS_BRANCH);
      this._sessionRecorder.onStartup();
    }

    if (!this.enableTelemetryRecording()) {
      this._log.config("setupChromeProcess - Telemetry recording is disabled, skipping Chrome process setup.");
      return Promise.resolve();
    }

    this._attachObservers();

    // For very short session durations, we may never load the client
    // id from disk.
    // We try to cache it in prefs to avoid this, even though this may
    // lead to some stale client ids.
    this._clientID = Preferences.get(PREF_CACHED_CLIENTID, null);

    // Delay full telemetry initialization to give the browser time to
    // run various late initializers. Otherwise our gathered memory
    // footprint and other numbers would be too optimistic.
    this._delayedInitTaskDeferred = Promise.defer();
    this._delayedInitTask = new DeferredTask(function* () {
      try {
        // TODO: This should probably happen after all the delayed init here.
        this._initialized = true;

        yield TelemetrySend.setup(this._testMode);

        // Load the ClientID and update the cache.
        this._clientID = yield ClientID.getClientID();
        Preferences.set(PREF_CACHED_CLIENTID, this._clientID);

        // Purge the pings archive by removing outdated pings. We don't wait for this
        // task to complete, but TelemetryStorage blocks on it during shutdown.
        TelemetryStorage.runCleanPingArchiveTask();

        Telemetry.asyncFetchTelemetryData(function () {});
        this._delayedInitTaskDeferred.resolve();
      } catch (e) {
        this._delayedInitTaskDeferred.reject(e);
      } finally {
        this._delayedInitTask = null;
      }
    }.bind(this), this._testMode ? TELEMETRY_TEST_DELAY : TELEMETRY_DELAY);

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
  setupContentTelemetry: function (testing = false) {
    this._testMode = testing;

    // We call |enableTelemetryRecording| here to make sure that Telemetry.canRecord* flags
    // are in sync between chrome and content processes.
    if (!this.enableTelemetryRecording()) {
      this._log.trace("setupContentTelemetry - Content process recording disabled.");
      return;
    }
  },

  // Do proper shutdown waiting and cleanup.
  _cleanupOnShutdown: Task.async(function*() {
    if (!this._initialized) {
      return;
    }

    Preferences.ignore(PREF_BRANCH_LOG, configureLogging);
    this._detachObservers();

    // Now do an orderly shutdown.
    try {
      // Stop any ping sending.
      yield TelemetrySend.shutdown();

      // First wait for clients processing shutdown.
      yield this._shutdownBarrier.wait();

      // ... and wait for any outstanding async ping activity.
      yield this._connectionsBarrier.wait();

      // Perform final shutdown operations.
      yield TelemetryStorage.shutdown();
    } finally {
      // Reset state.
      this._initialized = false;
      this._initStarted = false;
    }
  }),

  shutdown: function() {
    this._log.trace("shutdown");

    // We can be in one the following states here:
    // 1) setupTelemetry was never called
    // or it was called and
    //   2) _delayedInitTask was scheduled, but didn't run yet.
    //   3) _delayedInitTask is running now.
    //   4) _delayedInitTask finished running already.

    // This handles 1).
    if (!this._initStarted) {
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
  observe: function (aSubject, aTopic, aData) {
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
      break;
    }
  },

  get clientID() {
    return this._clientID;
  },

  /**
   * Get an object describing the current state of this module for AsyncShutdown diagnostics.
   */
  _getState: function() {
    return {
      initialized: this._initialized,
      initStarted: this._initStarted,
      haveDelayedInitTask: !!this._delayedInitTask,
      shutdownBarrier: this._shutdownBarrier.state,
      connectionsBarrier: this._connectionsBarrier.state,
    };
  },

  /**
   * Called whenever the FHR Upload preference changes (e.g. when user disables FHR from
   * the preferences panel), this triggers sending the deletion ping.
   */
  _onUploadPrefChange: function() {
    const uploadEnabled = Preferences.get(PREF_FHR_UPLOAD_ENABLED, false);
    if (uploadEnabled) {
      // There's nothing we should do if we are enabling upload.
      return;
    }
    // Send the deletion ping.
    this._log.trace("_onUploadPrefChange - Sending deletion ping.");
    this.submitExternalPing(PING_TYPE_DELETION, {}, { addClientId: true });
  },

  _attachObservers: function() {
    if (IS_UNIFIED_TELEMETRY) {
      // Watch the FHR upload setting to trigger deletion pings.
      Preferences.observe(PREF_FHR_UPLOAD_ENABLED, this._onUploadPrefChange, this);
    }
  },

  /**
   * Remove the preference observer to avoid leaks.
   */
  _detachObservers: function() {
    if (IS_UNIFIED_TELEMETRY) {
      Preferences.ignore(PREF_FHR_UPLOAD_ENABLED, this._onUploadPrefChange, this);
    }
  },

  /**
   * Allows waiting for TelemetryControllers delayed initialization to complete.
   * This will complete before TelemetryController is shutting down.
   * @return {Promise} Resolved when delayed TelemetryController initialization completed.
   */
  promiseInitialized: function() {
    return this._delayedInitTaskDeferred.promise;
  },

  getCurrentPingData: function(aSubsession) {
    this._log.trace("getCurrentPingData - subsession: " + aSubsession)

    const reason = aSubsession ? REASON_GATHER_SUBSESSION_PAYLOAD : REASON_GATHER_PAYLOAD;
    const type = PING_TYPE_MAIN;
    const payload = TelemetrySession.getPayload(reason);
    const options = { addClientId: true, addEnvironment: true };
    const ping = this.assemblePing(type, payload, options);

    return ping;
  },
};
