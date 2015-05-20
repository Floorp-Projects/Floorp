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
// The number of days to keep pings serialised on the disk in case of failures.
const DEFAULT_RETENTION_DAYS = 14;
// Timeout after which we consider a ping submission failed.
const PING_SUBMIT_TIMEOUT_MS = 2 * 60 * 1000;

// We treat pings before midnight as happening "at midnight" with this tolerance.
const MIDNIGHT_TOLERANCE_MS = 15 * 60 * 1000;
// For midnight fuzzing we want to affect pings around midnight with this tolerance.
const MIDNIGHT_TOLERANCE_FUZZ_MS = 5 * 60 * 1000;
// We try to spread "midnight" pings out over this interval.
const MIDNIGHT_FUZZING_INTERVAL_MS = 60 * 60 * 1000;
const MIDNIGHT_FUZZING_DELAY_MS = Math.random() * MIDNIGHT_FUZZING_INTERVAL_MS;

// Ping types.
const PING_TYPE_MAIN = "main";

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

function generateUUID() {
  let str = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator).generateUUID().toString();
  // strip {}
  return str.substring(1, str.length - 1);
}

/**
 * Determine if the ping has new ping format or a legacy one.
 */
function isNewPingFormat(aPing) {
  return ("id" in aPing) && ("application" in aPing) &&
         ("version" in aPing) && (aPing.version >= 2);
}

function tomorrow(date) {
  let d = new Date(date);
  d.setDate(d.getDate() + 1);
  return d;
}

/**
 * This is a policy object used to override behavior for testing.
 */
let Policy = {
  now: () => new Date(),
  midnightPingFuzzingDelay: () => MIDNIGHT_FUZZING_DELAY_MS,
  setPingSendTimeout: (callback, delayMs) => setTimeout(callback, delayMs),
  clearPingSendTimeout: (id) => clearTimeout(id),
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
    return this.setup();
  },
  /**
   * Used only for testing purposes.
   */
  setup: function() {
    return Impl.setupTelemetry(true);
  },

  /**
   * Send a notification.
   */
  observe: function (aSubject, aTopic, aData) {
    return Impl.observe(aSubject, aTopic, aData);
  },

  /**
   * Sets a server to send pings to.
   */
  setServer: function(aServer) {
    return Impl.setServer(aServer);
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
   * @returns {Promise} A promise that resolves with the ping id once the ping is stored or sent.
   */
  submitExternalPing: function(aType, aPayload, aOptions = {}) {
    aOptions.addClientId = aOptions.addClientId || false;
    aOptions.addEnvironment = aOptions.addEnvironment || false;

    return Impl.submitExternalPing(aType, aPayload, aOptions);
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
   * Add the ping to the pending ping list and save all pending pings.
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} [aOptions] Options object.
   * @param {Number} [aOptions.retentionDays=14] The number of days to keep the ping on disk
   *                 if sending fails.
   * @param {Boolean} [aOptions.addClientId=false] true if the ping should contain the client
   *                  id, false otherwise.
   * @param {Boolean} [aOptions.addEnvironment=false] true if the ping should contain the
   *                  environment data.
   * @param {Object}  [aOptions.overrideEnvironment=null] set to override the environment data.
   * @returns {Promise} A promise that resolves when the pings are saved.
   */
  savePendingPings: function(aType, aPayload, aOptions = {}) {
    let options = aOptions;
    options.retentionDays = aOptions.retentionDays || DEFAULT_RETENTION_DAYS;
    options.addClientId = aOptions.addClientId || false;
    options.addEnvironment = aOptions.addEnvironment || false;

    return Impl.savePendingPings(aType, aPayload, options);
  },

  /**
   * Save a ping to disk.
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} [aOptions] Options object.
   * @param {Number} [aOptions.retentionDays=14] The number of days to keep the ping on disk
   *                 if sending fails.
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
    options.retentionDays = aOptions.retentionDays || DEFAULT_RETENTION_DAYS;
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
   * @param {Number} [aOptions.retentionDays=14] The number of days to keep the ping on disk
   *                 if sending fails.
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
    options.retentionDays = aOptions.retentionDays || DEFAULT_RETENTION_DAYS;
    options.addClientId = aOptions.addClientId || false;
    options.addEnvironment = aOptions.addEnvironment || false;
    options.overwrite = aOptions.overwrite || false;

    return Impl.savePing(aType, aPayload, aFilePath, options);
  },

  /**
   * Send the persisted pings to the server.
   *
   * @return Promise A promise that is resolved when all pings finished sending or failed.
   */
  sendPersistedPings: function() {
    return Impl.sendPersistedPings();
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
  // Timer for scheduled ping sends.
  _pingSendTimer: null,

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

  // This tracks all pending ping requests to the server.
  _pendingPingRequests: new Map(),

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
      updateChannel = UpdateChannel.get();
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
    this._log.trace("assemblePing - Type " + aType + ", Server " + this._server +
                    ", aOptions " + JSON.stringify(aOptions));

    // Clone the payload data so we don't race against unexpected changes in subobjects that are
    // still referenced by other code.
    // We can't trust all callers to do this properly on their own.
    let payload = Cu.cloneInto(aPayload, myScope);

    // Fill the common ping fields.
    let pingData = {
      type: aType,
      id: generateUUID(),
      creationDate: (Policy.now()).toISOString(),
      version: PING_FORMAT_VERSION,
      application: this._getApplicationSection(),
      payload: aPayload,
    };

    if (aOptions.addClientId) {
      pingData.clientId = this._clientID;
    }

    if (aOptions.addEnvironment) {
      pingData.environment = aOptions.overrideEnvironment || TelemetryEnvironment.currentEnvironment;
    }

    return pingData;
  },

  popPayloads: function popPayloads() {
    this._log.trace("popPayloads");
    function payloadIter() {
      let iterator = TelemetryStorage.popPendingPings();
      for (let data of iterator) {
        yield data;
      }
    }

    let payloadIterWithThis = payloadIter.bind(this);
    return { __iterator__: payloadIterWithThis };
  },

  /**
   * Only used in tests.
   */
  setServer: function (aServer) {
    this._server = aServer;
  },

  /**
   * Track any pending ping send and save tasks through the promise passed here.
   * This is needed to block shutdown on any outstanding ping activity.
   */
  _trackPendingPingTask: function (aPromise) {
    this._connectionsBarrier.client.addBlocker("Waiting for ping task", aPromise);
  },

  /**
   * This helper calculates the next time that we can send pings at.
   * Currently this mostly redistributes ping sends around midnight to avoid submission
   * spikes around local midnight for daily pings.
   *
   * @param now Date The current time.
   * @return Number The next time (ms from UNIX epoch) when we can send pings.
   */
  _getNextPingSendTime: function(now) {
    const midnight = TelemetryUtils.getNearestMidnight(now, MIDNIGHT_FUZZING_INTERVAL_MS);

    // Don't delay ping if we are not close to midnight.
    if (!midnight) {
      return now.getTime();
    }

    // Delay ping send if we are within the midnight fuzzing range.
    // This is from: |midnight - MIDNIGHT_TOLERANCE_FUZZ_MS|
    // to: |midnight + MIDNIGHT_FUZZING_INTERVAL_MS|
    const midnightRangeStart = midnight.getTime() - MIDNIGHT_TOLERANCE_FUZZ_MS;
    if (now.getTime() >= midnightRangeStart) {
      // We spread those ping sends out between |midnight| and |midnight + midnightPingFuzzingDelay|.
      return midnight.getTime() + Policy.midnightPingFuzzingDelay();
    }

    return now.getTime();
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
   * @returns {Promise} A promise that is resolved with the ping id once the ping is stored or sent.
   */
  submitExternalPing: function send(aType, aPayload, aOptions) {
    this._log.trace("submitExternalPing - type: " + aType + ", server: " + this._server +
                    ", aOptions: " + JSON.stringify(aOptions));

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

    // Check if we can send pings now.
    const now = Policy.now();
    const nextPingSendTime = this._getNextPingSendTime(now);
    const throttled = (nextPingSendTime > now.getTime());

    // We can't send pings now, schedule a later send.
    if (throttled) {
      this._log.trace("submitExternalPing - throttled, delaying ping send to " + new Date(nextPingSendTime));
      this._reschedulePingSendTimer(nextPingSendTime);
    }

    if (!this._initialized || throttled) {
      // We can't send because we are still initializing or throttled, add this to the pending pings.
      this._log.trace("submitExternalPing - ping is pending, initialized: " + this._initialized +
                      ", throttled: " + throttled);
      p.push(TelemetryStorage.addPendingPing(pingData));
    } else {
      // Try to send the ping, persist it if sending it fails.
      this._log.trace("submitExternalPing - already initialized, ping will be sent");
      p.push(this.doPing(pingData, false)
                 .catch(() => TelemetryStorage.savePing(pingData, true)));
      p.push(this.sendPersistedPings());
    }

    let promise = Promise.all(p);
    this._trackPendingPingTask(promise);
    return promise.then(() => pingData.id);
  },

  /**
   * Send the persisted pings to the server.
   *
   * @return Promise A promise that is resolved when all pings finished sending or failed.
   */
  sendPersistedPings: function sendPersistedPings() {
    this._log.trace("sendPersistedPings - Can send: " + this._canSend());
    if (!this._canSend()) {
      this._log.trace("sendPersistedPings - Telemetry is not allowed to send pings.");
      return Promise.resolve();
    }

    // Check if we can send pings now - otherwise schedule a later send.
    const now = Policy.now();
    const nextPingSendTime = this._getNextPingSendTime(now);
    if (nextPingSendTime > now.getTime()) {
      this._log.trace("sendPersistedPings - delaying ping send to " + new Date(nextPingSendTime));
      this._reschedulePingSendTimer(nextPingSendTime);
      return Promise.resolve();
    }

    // We can send now.
    let pingsIterator = Iterator(this.popPayloads());
    let p = [for (data of pingsIterator) this.doPing(data, true).catch((e) => {
      this._log.error("sendPersistedPings - doPing rejected", e);
    })];

    let promise = Promise.all(p);
    this._trackPendingPingTask(promise);
    return promise;
  },

  /**
   * Saves all the pending pings, plus the passed one, to disk.
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} aOptions Options object.
   * @param {Number} aOptions.retentionDays The number of days to keep the ping on disk
   *                 if sending fails.
   * @param {Boolean} aOptions.addClientId true if the ping should contain the client id,
   *                  false otherwise.
   * @param {Boolean} aOptions.addEnvironment true if the ping should contain the
   *                  environment data.
   * @param {Object}  [aOptions.overrideEnvironment=null] set to override the environment data.
   *
   * @returns {Promise} A promise that resolves when all the pings are saved to disk.
   */
  savePendingPings: function savePendingPings(aType, aPayload, aOptions) {
    this._log.trace("savePendingPings - Type " + aType + ", Server " + this._server +
                    ", aOptions " + JSON.stringify(aOptions));

    let pingData = this.assemblePing(aType, aPayload, aOptions);
    return TelemetryStorage.savePendingPings(pingData);
  },

  /**
   * Save a ping to disk.
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} aOptions Options object.
   * @param {Number} aOptions.retentionDays The number of days to keep the ping on disk
   *                 if sending fails.
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
    this._log.trace("addPendingPing - Type " + aType + ", Server " + this._server +
                    ", aOptions " + JSON.stringify(aOptions));

    let pingData = this.assemblePing(aType, aPayload, aOptions);

    let savePromise = TelemetryStorage.savePing(pingData, aOptions.overwrite);
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
   * @param {Number} aOptions.retentionDays The number of days to keep the ping on disk
   *                 if sending fails.
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
    this._log.trace("savePing - Type " + aType + ", Server " + this._server +
                    ", File Path " + aFilePath + ", aOptions " + JSON.stringify(aOptions));
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

  onPingRequestFinished: function(success, startTime, ping, isPersisted) {
    this._log.trace("onPingRequestFinished - success: " + success + ", persisted: " + isPersisted);

    Telemetry.getHistogramById("TELEMETRY_SEND").add(new Date() - startTime);
    let hping = Telemetry.getHistogramById("TELEMETRY_PING");
    let hsuccess = Telemetry.getHistogramById("TELEMETRY_SUCCESS");

    hsuccess.add(success);
    hping.add(new Date() - startTime);

    if (success && isPersisted) {
      return TelemetryStorage.cleanupPingFile(ping);
    } else {
      return Promise.resolve();
    }
  },

  submissionPath: function submissionPath(ping) {
    // The new ping format contains an "application" section, the old one doesn't.
    let pathComponents;
    if (isNewPingFormat(ping)) {
      // We insert the Ping id in the URL to simplify server handling of duplicated
      // pings.
      let app = ping.application;
      pathComponents = [
        ping.id, ping.type, app.name, app.version, app.channel, app.buildId
      ];
    } else {
      // This is a ping in the old format.
      if (!("slug" in ping)) {
        // That's odd, we don't have a slug. Generate one so that TelemetryStorage.jsm works.
        ping.slug = generateUUID();
      }

      // Do we have enough info to build a submission URL?
      let payload = ("payload" in ping) ? ping.payload : null;
      if (payload && ("info" in payload)) {
        let info = ping.payload.info;
        pathComponents = [ ping.slug, info.reason, info.appName, info.appVersion,
                           info.appUpdateChannel, info.appBuildID ];
      } else {
        // Only use the UUID as the slug.
        pathComponents = [ ping.slug ];
      }
    }

    let slug = pathComponents.join("/");
    return "/submit/telemetry/" + slug;
  },

  doPing: function doPing(ping, isPersisted) {
    if (!this._canSend()) {
      // We can't send the pings to the server, so don't try to.
      this._log.trace("doPing - Sending is disabled.");
      return Promise.resolve();
    }

    this._log.trace("doPing - Server " + this._server + ", Persisted " + isPersisted);
    const isNewPing = isNewPingFormat(ping);
    const version = isNewPing ? PING_FORMAT_VERSION : 1;
    const url = this._server + this.submissionPath(ping) + "?v=" + version;

    let request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                  .createInstance(Ci.nsIXMLHttpRequest);
    request.mozBackgroundRequest = true;
    request.timeout = PING_SUBMIT_TIMEOUT_MS;

    request.open("POST", url, true);
    request.overrideMimeType("text/plain");
    request.setRequestHeader("Content-Type", "application/json; charset=UTF-8");

    this._pendingPingRequests.set(url, request);

    let startTime = new Date();
    let deferred = PromiseUtils.defer();

    let onRequestFinished = (success, event) => {
      let onCompletion = () => {
        if (success) {
          deferred.resolve();
        } else {
          deferred.reject(event);
        }
      };

      this._pendingPingRequests.delete(url);
      this.onPingRequestFinished(success, startTime, ping, isPersisted)
        .then(() => onCompletion(),
              (error) => {
                this._log.error("doPing - request success: " + success + ", error" + error);
                onCompletion();
              });
    };

    let errorhandler = (event) => {
      this._log.error("doPing - error making request to " + url + ": " + event.type);
      onRequestFinished(false, event);
    };
    request.onerror = errorhandler;
    request.ontimeout = errorhandler;
    request.onabort = errorhandler;

    request.onload = (event) => {
      let status = request.status;
      let statusClass = status - (status % 100);
      let success = false;

      if (statusClass === 200) {
        // We can treat all 2XX as success.
        this._log.info("doPing - successfully loaded, status: " + status);
        success = true;
      } else if (statusClass === 400) {
        // 4XX means that something with the request was broken.
        this._log.error("doPing - error submitting to " + url + ", status: " + status
                        + " - ping request broken?");
        // TODO: we should handle this better, but for now we should avoid resubmitting
        // broken requests by pretending success.
        success = true;
      } else if (statusClass === 500) {
        // 5XX means there was a server-side error and we should try again later.
        this._log.error("doPing - error submitting to " + url + ", status: " + status
                        + " - server error, should retry later");
      } else {
        // We received an unexpected status codes.
        this._log.error("doPing - error submitting to " + url + ", status: " + status
                        + ", type: " + event.type);
      }

      onRequestFinished(success, event);
    };

    // If that's a legacy ping format, just send its payload.
    let networkPayload = isNewPing ? ping : ping.payload;
    request.setRequestHeader("Content-Encoding", "gzip");
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";
    startTime = new Date();
    let utf8Payload = converter.ConvertFromUnicode(JSON.stringify(networkPayload));
    utf8Payload += converter.Finish();
    Telemetry.getHistogramById("TELEMETRY_STRINGIFY").add(new Date() - startTime);
    let payloadStream = Cc["@mozilla.org/io/string-input-stream;1"]
                        .createInstance(Ci.nsIStringInputStream);
    startTime = new Date();
    payloadStream.data = this.gzipCompressString(utf8Payload);
    Telemetry.getHistogramById("TELEMETRY_COMPRESS").add(new Date() - startTime);
    startTime = new Date();
    request.send(payloadStream);

    return deferred.promise;
  },

  gzipCompressString: function gzipCompressString(string) {
    let observer = {
      buffer: "",
      onStreamComplete: function(loader, context, status, length, result) {
        this.buffer = String.fromCharCode.apply(this, result);
      }
    };

    let scs = Cc["@mozilla.org/streamConverters;1"]
              .getService(Ci.nsIStreamConverterService);
    let listener = Cc["@mozilla.org/network/stream-loader;1"]
                  .createInstance(Ci.nsIStreamLoader);
    listener.init(observer);
    let converter = scs.asyncConvertData("uncompressed", "gzip",
                                         listener, null);
    let stringStream = Cc["@mozilla.org/io/string-input-stream;1"]
                       .createInstance(Ci.nsIStringInputStream);
    stringStream.data = string;
    converter.onStartRequest(null, null);
    converter.onDataAvailable(null, null, stringStream, 0, string.length);
    converter.onStopRequest(null, null, null);
    return observer.buffer;
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
    if (!Telemetry.isOfficialTelemetry && !this._testMode) {
      // We can't send data; no point in initializing observers etc.
      // Only do this for official builds so that e.g. developer builds
      // still enable Telemetry based on prefs.
      Telemetry.canRecordExtended = false;
      this._log.config("enableTelemetryRecording - Can't send data, disabling extended Telemetry recording.");
    }
#endif

    this._server = Preferences.get(PREF_SERVER, undefined);
    if (!enabled || !Telemetry.canRecordBase) {
      // Turn off extended telemetry recording if disabled by preferences or if base/telemetry
      // telemetry recording is off.
      Telemetry.canRecordExtended = false;
      this._log.config("enableTelemetryRecording - Disabling extended Telemetry recording.");
    }

    return Telemetry.canRecordBase;
  },

  /**
   * Initializes telemetry within a timer. If there is no PREF_SERVER set, don't turn on telemetry.
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
        this._initialized = true;

        // If any pings were submitted before the delayed init finished
        // we will send them now.
        yield this.sendPersistedPings();

        // Load pending pings from disk.
        yield TelemetryStorage.loadSavedPings();
        // If we have any overdue pings lying around, we'll be aggressive
        // and try to send them all off ASAP.
        if (TelemetryStorage.pingsOverdue > 0) {
          this._log.trace("setupChromeProcess - Sending " + TelemetryStorage.pingsOverdue +
                          " overdue pings now.");
          yield this.sendPersistedPings();
        }

        // Load the ClientID and update the cache.
        this._clientID = yield ClientID.getClientID();
        Preferences.set(PREF_CACHED_CLIENTID, this._clientID);

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

  // Do proper shutdown waiting and cleanup.
  _cleanupOnShutdown: Task.async(function*() {
    if (!this._initialized) {
      return;
    }

    Preferences.ignore(PREF_BRANCH_LOG, configureLogging);

    // Abort any pending ping XHRs.
    for (let [url, request] of this._pendingPingRequests) {
      this._log.trace("_cleanupOnShutdown - aborting ping request for " + url);
      try {
        request.abort();
      } catch (e) {
        this._log.error("_cleanupOnShutdown - failed to abort request to " + url, e);
      }
    }
    this._pendingPingRequests.clear();

    // Now do an orderly shutdown.
    try {
      // First wait for clients processing shutdown.
      yield this._shutdownBarrier.wait();

      // Then clear scheduled ping sends...
      this._clearPingSendTimer();

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
      // app-startup is only registered for content processes. We call
      // |enableTelemetryRecording| here to make sure that Telemetry.canRecord* flags
      // are in sync between chrome and content processes.
      if (!this.enableTelemetryRecording()) {
        this._log.trace("observe - Content process recording disabled.");
        return;
      }
      break;
    }
  },

  get clientID() {
    return this._clientID;
  },

  /**
   * Check if pings can be sent to the server. If FHR is not allowed to upload,
   * pings are not sent to the server (Telemetry is a sub-feature of FHR).
   * If unified telemetry is off, don't send pings if Telemetry is disabled.
   * @return {Boolean} True if pings can be send to the servers, false otherwise.
   */
  _canSend: function() {
    return (Telemetry.isOfficialTelemetry || this._testMode) &&
           Preferences.get(PREF_FHR_UPLOAD_ENABLED, false) &&
           (IS_UNIFIED_TELEMETRY || Preferences.get(PREF_ENABLED));
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

  _reschedulePingSendTimer: function(timestamp) {
    this._clearPingSendTimer();
    const interval = timestamp - Policy.now();
    this._pingSendTimer = Policy.setPingSendTimeout(() => this.sendPersistedPings(), interval);
  },

  _clearPingSendTimer: function() {
    if (this._pingSendTimer) {
      Policy.clearPingSendTimeout(this._pingSendTimer);
      this._pingSendTimer = null;
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
