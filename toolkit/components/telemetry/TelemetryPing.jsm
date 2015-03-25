/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/debug.js", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/DeferredTask.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm");

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetryPing::";

const PREF_BRANCH = "toolkit.telemetry.";
const PREF_BRANCH_LOG = PREF_BRANCH + "log.";
const PREF_SERVER = PREF_BRANCH + "server";
const PREF_ENABLED = PREF_BRANCH + "enabled";
const PREF_LOG_LEVEL = PREF_BRANCH_LOG + "level";
const PREF_LOG_DUMP = PREF_BRANCH_LOG + "dump";
const PREF_CACHED_CLIENTID = PREF_BRANCH + "cachedClientID"
const PREF_FHR_UPLOAD_ENABLED = "datareporting.healthreport.uploadEnabled";

const PING_FORMAT_VERSION = 4;

// Delay before intializing telemetry (ms)
const TELEMETRY_DELAY = 60000;
// Delay before initializing telemetry if we're testing (ms)
const TELEMETRY_TEST_DELAY = 100;
// The number of days to keep pings serialised on the disk in case of failures.
const DEFAULT_RETENTION_DAYS = 14;

XPCOMUtils.defineLazyServiceGetter(this, "Telemetry",
                                   "@mozilla.org/base/telemetry;1",
                                   "nsITelemetry");
XPCOMUtils.defineLazyModuleGetter(this, "AsyncShutdown",
                                  "resource://gre/modules/AsyncShutdown.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryFile",
                                  "resource://gre/modules/TelemetryFile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryLog",
                                  "resource://gre/modules/TelemetryLog.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ThirdPartyCookieProbe",
                                  "resource://gre/modules/ThirdPartyCookieProbe.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryEnvironment",
                                  "resource://gre/modules/TelemetryEnvironment.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UpdateChannel",
                                  "resource://gre/modules/UpdateChannel.jsm");

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

this.EXPORTED_SYMBOLS = ["TelemetryPing"];

this.TelemetryPing = Object.freeze({
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
   * Adds a ping to the pending ping list by moving it to the saved pings directory
   * and adding it to the pending ping list.
   *
   * @param {String} aPingPath The path of the ping to add to the pending ping list.
   * @param {Boolean} [aRemoveOriginal] If true, deletes the ping at aPingPath after adding
   *                  it to the saved pings directory.
   * @return {Promise} Resolved when the ping is correctly moved to the saved pings directory.
   */
  addPendingPing: function(aPingPath, aRemoveOriginal) {
    return Impl.addPendingPing(aPingPath, aRemoveOriginal);
  },

  /**
   * Send payloads to the server.
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
   * @returns {Promise} A promise that resolves when the ping is sent.
   */
  send: function(aType, aPayload, aOptions = {}) {
    let options = aOptions;
    options.retentionDays = aOptions.retentionDays || DEFAULT_RETENTION_DAYS;
    options.addClientId = aOptions.addClientId || false;
    options.addEnvironment = aOptions.addEnvironment || false;

    return Impl.send(aType, aPayload, options);
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
   * @param {String} [aOptions.filePath] The path to save the ping to. Will save to default
   *                 ping location if not provided.
   *
   * @returns {Promise<Integer>} A promise that resolves with the ping id when the ping is
   *                             saved to disk.
   */
  savePing: function(aType, aPayload, aOptions = {}) {
    let options = aOptions;
    options.retentionDays = aOptions.retentionDays || DEFAULT_RETENTION_DAYS;
    options.addClientId = aOptions.addClientId || false;
    options.addEnvironment = aOptions.addEnvironment || false;
    options.overwrite = aOptions.overwrite || false;

    return Impl.savePing(aType, aPayload, options);
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
    * The AsyncShutdown.Barrier to synchronize with TelemetryPing shutdown.
    */
   get shutdown() {
    return Impl._shutdownBarrier.client;
   },
});

let Impl = {
  _initialized: false,
  _initStarted: false, // Whether we started setting up TelemetryPing.
  _log: null,
  _prevValues: {},
  // The previous build ID, if this is the first run with a new build.
  // Undefined if this is not the first run, or the previous build ID is unknown.
  _previousBuildID: undefined,
  _clientID: null,
  // A task performing delayed initialization
  _delayedInitTask: null,
  // The deferred promise resolved when the initialization task completes.
  _delayedInitTaskDeferred: null,

  _shutdownBarrier: new AsyncShutdown.Barrier("TelemetryPing: Waiting for clients."),

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
   *
   * @returns Promise<Object> A promise that resolves when the ping is completely assembled.
   */
  assemblePing: function assemblePing(aType, aPayload, aOptions = {}) {
    this._log.trace("assemblePing - Type " + aType + ", Server " + this._server +
                    ", aOptions " + JSON.stringify(aOptions));

    // Fill the common ping fields.
    let pingData = {
      type: aType,
      id: generateUUID(),
      creationDate: (new Date()).toISOString(),
      version: PING_FORMAT_VERSION,
      application: this._getApplicationSection(),
      payload: aPayload,
    };

    if (aOptions.addClientId) {
      pingData.clientId = this._clientID;
    }

    if (aOptions.addEnvironment) {
      return TelemetryEnvironment.getEnvironmentData().then(environment => {
        pingData.environment = environment;
        return pingData;
      },
      error => {
        this._log.error("assemblePing - Rejection", error);
      });
    }

    return Promise.resolve(pingData);
  },

  popPayloads: function popPayloads() {
    this._log.trace("popPayloads");
    function payloadIter() {
      let iterator = TelemetryFile.popPendingPings();
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
   * Adds a ping to the pending ping list by moving it to the saved pings directory
   * and adding it to the pending ping list.
   *
   * @param {String} aPingPath The path of the ping to add to the pending ping list.
   * @param {Boolean} [aRemoveOriginal] If true, deletes the ping at aPingPath after adding
   *                  it to the saved pings directory.
   * @return {Promise} Resolved when the ping is correctly moved to the saved pings directory.
   */
  addPendingPing: function(aPingPath, aRemoveOriginal) {
    return TelemetryFile.addPendingPing(aPingPath).then(() => {
        if (aRemoveOriginal) {
          return OS.File.remove(aPingPath);
        }
      }, error => this._log.error("addPendingPing - Unable to add the pending ping", error));
  },

  /**
   * Build a complete ping and send data to the server. Record success/send-time in
   * histograms.
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
   *
   * @returns {Promise} A promise that resolves when the ping is sent.
   */
  send: function send(aType, aPayload, aOptions) {
    this._log.trace("send - Type " + aType + ", Server " + this._server +
                    ", aOptions " + JSON.stringify(aOptions));

    return this.assemblePing(aType, aPayload, aOptions)
        .then(pingData => {
          // Once ping is assembled, send it along with the persisted ping in the backlog.
          let p = [
            // Persist the ping if sending it fails.
            this.doPing(pingData, false)
                .catch(() => TelemetryFile.savePing(pingData, true)),
            this.sendPersistedPings(),
          ];
          return Promise.all(p);
        },
        error => this._log.error("send - Rejection", error));
  },

  /**
   * Send the persisted pings to the server.
   */
  sendPersistedPings: function sendPersistedPings() {
    this._log.trace("sendPersistedPings");
    let pingsIterator = Iterator(this.popPayloads());
    let p = [data for (data in pingsIterator)].map(data => this.doPing(data, true));
    return Promise.all(p);
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
   *
   * @returns {Promise} A promise that resolves when all the pings are saved to disk.
   */
  savePendingPings: function savePendingPings(aType, aPayload, aOptions) {
    this._log.trace("savePendingPings - Type " + aType + ", Server " + this._server +
                    ", aOptions " + JSON.stringify(aOptions));

    return this.assemblePing(aType, aPayload, aOptions)
        .then(pingData => TelemetryFile.savePendingPings(pingData),
              error => this._log.error("savePendingPings - Rejection", error));
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
   * @param {String} [aOptions.filePath] The path to save the ping to. Will save to default
   *                 ping location if not provided.
   *
   * @returns {Promise} A promise that resolves with the ping id when the ping is saved to
   *                    disk.
   */
  savePing: function savePing(aType, aPayload, aOptions) {
    this._log.trace("savePing - Type " + aType + ", Server " + this._server +
                    ", aOptions " + JSON.stringify(aOptions));

    return this.assemblePing(aType, aPayload, aOptions)
      .then(pingData => {
        if ("filePath" in aOptions) {
          return TelemetryFile.savePingToFile(pingData, aOptions.filePath, aOptions.overwrite)
                              .then(() => { return pingData.id; });
        } else {
          return TelemetryFile.savePing(pingData, aOptions.overwrite)
                              .then(() => { return pingData.id; });
        }
      }, error => this._log.error("savePing - Rejection", error));
  },

  finishPingRequest: function finishPingRequest(success, startTime, ping, isPersisted) {
    this._log.trace("finishPingRequest - Success " + success + ", Persisted " + isPersisted);

    let hping = Telemetry.getHistogramById("TELEMETRY_PING");
    let hsuccess = Telemetry.getHistogramById("TELEMETRY_SUCCESS");

    hsuccess.add(success);
    hping.add(new Date() - startTime);

    if (success && isPersisted) {
      return TelemetryFile.cleanupPingFile(ping);
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
        // That's odd, we don't have a slug. Generate one so that TelemetryFile.jsm works.
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
    this._log.trace("doPing - Server " + this._server + ", Persisted " + isPersisted);
    let deferred = Promise.defer();
    let isNewPing = isNewPingFormat(ping);
    let version = isNewPing ? PING_FORMAT_VERSION : 1;
    let url = this._server + this.submissionPath(ping) + "?v=" + version;
    let request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                  .createInstance(Ci.nsIXMLHttpRequest);
    request.mozBackgroundRequest = true;
    request.open("POST", url, true);
    request.overrideMimeType("text/plain");
    request.setRequestHeader("Content-Type", "application/json; charset=UTF-8");

    let startTime = new Date();

    function handler(success) {
      let handleCompletion = event => {
        if (success) {
          deferred.resolve();
        } else {
          deferred.reject(event);
        }
      };

      return function(event) {
        this.finishPingRequest(success, startTime, ping, isPersisted)
          .then(() => handleCompletion(event),
                error => {
                  this._log.error("doPing - Request Success " + success + ", Error " +
                                  error);
                  handleCompletion(event);
                });
      };
    }
    request.addEventListener("error", handler(false).bind(this), false);
    request.addEventListener("load", handler(true).bind(this), false);

    // If that's a legacy ping format, just send its payload.
    let networkPayload = isNewPing ? ping : ping.payload;
    request.setRequestHeader("Content-Encoding", "gzip");
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";
    let utf8Payload = converter.ConvertFromUnicode(JSON.stringify(networkPayload));
    utf8Payload += converter.Finish();
    let payloadStream = Cc["@mozilla.org/io/string-input-stream;1"]
                        .createInstance(Ci.nsIStringInputStream);
    payloadStream.data = this.gzipCompressString(utf8Payload);
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
   */
  enableTelemetryRecording: function enableTelemetryRecording(testing) {

#ifdef MOZILLA_OFFICIAL
    if (!Telemetry.canSend && !testing) {
      // We can't send data; no point in initializing observers etc.
      // Only do this for official builds so that e.g. developer builds
      // still enable Telemetry based on prefs.
      Telemetry.canRecord = false;
      this._log.config("enableTelemetryRecording - Can't send data, disabling Telemetry recording.");
      return false;
    }
#endif

    let enabled = Preferences.get(PREF_ENABLED, false);
    this._server = Preferences.get(PREF_SERVER, undefined);
    if (!enabled) {
      // Turn off local telemetry if telemetry is disabled.
      // This may change once about:telemetry is added.
      Telemetry.canRecord = false;
      this._log.config("enableTelemetryRecording - Telemetry is disabled, turning off Telemetry recording.");
      return false;
    }

    return true;
  },

  /**
   * Initializes telemetry within a timer. If there is no PREF_SERVER set, don't turn on telemetry.
   *
   * This delayed initialization means TelemetryPing init can be in the following states:
   * 1) setupTelemetry was never called
   * or it was called and
   *   2) _delayedInitTask was scheduled, but didn't run yet.
   *   3) _delayedInitTask is currently running.
   *   4) _delayedInitTask finished running and is nulled out.
   */
  setupTelemetry: function setupTelemetry(testing) {
    this._initStarted = true;
    if (testing && !this._log) {
      this._log = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, LOGGER_PREFIX);
    }

    this._log.trace("setupTelemetry");

    if (this._delayedInitTask) {
      this._log.error("setupTelemetry - init task already running");
      return this._delayedInitTaskDeferred.promise;
    }

    if (this._initialized && !testing) {
      this._log.error("setupTelemetry - already initialized");
      return Promise.resolve();
    }

    // Initialize some probes that are kept in their own modules
    this._thirdPartyCookies = new ThirdPartyCookieProbe();
    this._thirdPartyCookies.init();

    if (!this.enableTelemetryRecording(testing)) {
      this._log.config("setupTelemetry - Telemetry recording is disabled, skipping Telemetry setup.");
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

        yield TelemetryEnvironment.init();

        yield TelemetryFile.loadSavedPings();
        // If we have any TelemetryPings lying around, we'll be aggressive
        // and try to send them all off ASAP.
        if (TelemetryFile.pingsOverdue > 0) {
          this._log.trace("setupChromeProcess - Sending " + TelemetryFile.pingsOverdue +
                          " overdue pings now.");
          // It doesn't really matter what we pass to this.send as a reason,
          // since it's never sent to the server. All that this.send does with
          // the reason is check to make sure it's not a test-ping.
          yield this.sendPersistedPings();
        }

        if ("@mozilla.org/datareporting/service;1" in Cc) {
          let drs = Cc["@mozilla.org/datareporting/service;1"]
                      .getService(Ci.nsISupports)
                      .wrappedJSObject;
          this._clientID = yield drs.getClientID();
          // Update cached client id.
          Preferences.set(PREF_CACHED_CLIENTID, this._clientID);
        } else {
          // Nuke potentially cached client id.
          Preferences.reset(PREF_CACHED_CLIENTID);
        }

        Telemetry.asyncFetchTelemetryData(function () {});
        this._delayedInitTaskDeferred.resolve();
      } catch (e) {
        this._delayedInitTaskDeferred.reject(e);
      } finally {
        this._delayedInitTask = null;
        this._delayedInitTaskDeferred = null;
      }
    }.bind(this), testing ? TELEMETRY_TEST_DELAY : TELEMETRY_DELAY);

    AsyncShutdown.sendTelemetry.addBlocker("TelemetryPing: shutting down",
                                           () => this.shutdown(),
                                           () => this._getState());

    this._delayedInitTask.arm();
    return this._delayedInitTaskDeferred.promise;
  },

  shutdown: function() {
    this._log.trace("shutdown");

    let cleanup = () => {
      if (!this._initialized) {
        return;
      }
      let reset = () => {
        this._initialized = false;
        this._initStarted = false;
      };
      return this._shutdownBarrier.wait().then(
               () => TelemetryEnvironment.shutdown().then(reset, reset));
    };

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
      return cleanup();
    }

    // This handles 2) and 3).
    return this._delayedInitTask.finalize().then(cleanup);
  },

  /**
   * This observer drives telemetry.
   */
  observe: function (aSubject, aTopic, aData) {
    // The logger might still be not available at this point.
    if (!this._log) {
      // If we don't have a logger, we need to make sure |Log.repository.getLogger()| is
      // called before |getLoggerWithMessagePrefix|. Otherwise logging won't work.
      configureLogging();
      this._log = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, LOGGER_PREFIX);
    }

    this._log.trace("observe - " + aTopic + " notified.");

    switch (aTopic) {
    case "profile-after-change":
      // profile-after-change is only registered for chrome processes.
      return this.setupTelemetry();
    case "app-startup":
      // app-startup is only registered for content processes.
      Services.obs.addObserver(this, "content-child-shutdown", false);
      break;
    case "content-child-shutdown":
      // content-child-shutdown is only registered for content processes.
      Services.obs.removeObserver(this, "content-child-shutdown");
      Preferences.ignore(PREF_BRANCH_LOG, configureLogging);
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
    };
  },
};
