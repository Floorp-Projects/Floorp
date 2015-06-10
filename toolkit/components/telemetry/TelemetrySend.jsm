/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module is responsible for uploading pings to the server and persisting
 * pings that can't be send now.
 * Those pending pings are persisted on disk and sent at the next opportunity,
 * newest first.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "TelemetrySend",
];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/Log.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/PromiseUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/TelemetryUtils.jsm", this);
Cu.import("resource://gre/modules/Timer.jsm", this);

XPCOMUtils.defineLazyModuleGetter(this, "AsyncShutdown",
                                  "resource://gre/modules/AsyncShutdown.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryStorage",
                                  "resource://gre/modules/TelemetryStorage.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "Telemetry",
                                   "@mozilla.org/base/telemetry;1",
                                   "nsITelemetry");

const Utils = TelemetryUtils;

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetrySend::";

const PREF_BRANCH = "toolkit.telemetry.";
const PREF_SERVER = PREF_BRANCH + "server";
const PREF_UNIFIED = PREF_BRANCH + "unified";
const PREF_TELEMETRY_ENABLED = PREF_BRANCH + "enabled";
const PREF_FHR_UPLOAD_ENABLED = "datareporting.healthreport.uploadEnabled";

const TOPIC_IDLE_DAILY = "idle-daily";
const TOPIC_QUIT_APPLICATION = "quit-application";

// Whether the FHR/Telemetry unification features are enabled.
// Changing this pref requires a restart.
const IS_UNIFIED_TELEMETRY = Preferences.get(PREF_UNIFIED, false);

const PING_FORMAT_VERSION = 4;

// We try to spread "midnight" pings out over this interval.
const MIDNIGHT_FUZZING_INTERVAL_MS = 60 * 60 * 1000;
// We delay sending "midnight" pings on this client by this interval.
const MIDNIGHT_FUZZING_DELAY_MS = Math.random() * MIDNIGHT_FUZZING_INTERVAL_MS;

// Timeout after which we consider a ping submission failed.
const PING_SUBMIT_TIMEOUT_MS = 2 * 60 * 1000;

// Files that have been lying around for longer than MAX_PING_FILE_AGE are
// deleted without being loaded.
const MAX_PING_FILE_AGE = 14 * 24 * 60 * 60 * 1000; // 2 weeks

// Files that are older than OVERDUE_PING_FILE_AGE, but younger than
// MAX_PING_FILE_AGE indicate that we need to send all of our pings ASAP.
const OVERDUE_PING_FILE_AGE = 7 * 24 * 60 * 60 * 1000; // 1 week

// Maximum number of pings to save.
const MAX_LRU_PINGS = 50;

/**
 * This is a policy object used to override behavior within this module.
 * Tests override properties on this object to allow for control of behavior
 * that would otherwise be very hard to cover.
 */
let Policy = {
  now: () => new Date(),
  midnightPingFuzzingDelay: () => MIDNIGHT_FUZZING_DELAY_MS,
  setPingSendTimeout: (callback, delayMs) => setTimeout(callback, delayMs),
  clearPingSendTimeout: (id) => clearTimeout(id),
};

/**
 * Determine if the ping has the new v4 ping format or the legacy v2 one or earlier.
 */
function isV4PingFormat(aPing) {
  return ("id" in aPing) && ("application" in aPing) &&
         ("version" in aPing) && (aPing.version >= 2);
}

function tomorrow(date) {
  let d = new Date(date);
  d.setDate(d.getDate() + 1);
  return d;
}

/**
 * @return {String} This returns a string with the gzip compressed data.
 */
function gzipCompressString(string) {
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
}

this.TelemetrySend = {
  /**
   * Maximum age in ms of a pending ping file before it gets evicted.
   */
  get MAX_PING_FILE_AGE() {
    return MAX_PING_FILE_AGE;
  },

  /**
   * Age in ms of a pending ping to be considered overdue.
   */
  get OVERDUE_PING_FILE_AGE() {
    return OVERDUE_PING_FILE_AGE;
  },

  /**
   * The maximum number of pending pings we keep in the backlog.
   */
  get MAX_LRU_PINGS() {
    return MAX_LRU_PINGS;
  },

  /**
   * Initializes this module.
   *
   * @param {Boolean} testing Whether this is run in a test. This changes some behavior
   * to enable proper testing.
   * @return {Promise} Resolved when setup is finished.
   */
  setup: function(testing = false) {
    return TelemetrySendImpl.setup(testing);
  },

  /**
   * Shutdown this module - this will cancel any pending ping tasks and wait for
   * outstanding async activity like network and disk I/O.
   *
   * @return {Promise} Promise that is resolved when shutdown is finished.
   */
  shutdown: function() {
    return TelemetrySendImpl.shutdown();
  },

  /**
   * Submit a ping for sending. This will:
   * - send the ping right away if possible or
   * - save the ping to disk and send it at the next opportunity
   *
   * @param {Object} ping The ping data to send, must be serializable to JSON.
   * @return {Promise} A promise that is resolved when the ping is sent or saved.
   */
  submitPing: function(ping) {
    return TelemetrySendImpl.submitPing(ping);
  },

  /**
   * Count of pending pings that were discarded at startup due to being too old.
   */
  get discardedPingsCount() {
    return TelemetrySendImpl.discardedPingsCount;
  },

  /**
   * Count of pending pings that were found to be overdue at startup.
   */
  get overduePingsCount() {
    return TelemetrySendImpl.overduePingsCount;
  },

  /**
   * Only used in tests. Used to reset the module data to emulate a restart.
   */
  reset: function() {
    return TelemetrySendImpl.reset();
  },

  /**
   * Only used in tests.
   */
  setServer: function(server) {
    return TelemetrySendImpl.setServer(server);
  },
};

let TelemetrySendImpl = {
  _sendingEnabled: false,
  _logger: null,
  // Timer for scheduled ping sends.
  _pingSendTimer: null,
  // This tracks all pending ping requests to the server.
  _pendingPingRequests: new Map(),
  // This is a private barrier blocked by pending async ping activity (sending & saving).
  _connectionsBarrier: new AsyncShutdown.Barrier("TelemetrySend: Waiting for pending ping activity"),
  // This is true when running in the test infrastructure.
  _testMode: false,

  // Count of pending pings we discarded for age on startup.
  _discardedPingsCount: 0,
  // Count of pending pings we evicted for being over the limit on startup.
  _evictedPingsCount: 0,
  // Count of pending pings that were overdue.
  _overduePingCount: 0,

  OBSERVER_TOPICS: [
    TOPIC_IDLE_DAILY,
  ],

  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, LOGGER_PREFIX);
    }

    return this._logger;
  },

  get discardedPingsCount() {
    return this._discardedPingsCount;
  },

  get overduePingsCount() {
    return this._overduePingCount;
  },

  setup: Task.async(function*(testing) {
    this._log.trace("setup");

    this._testMode = testing;
    this._sendingEnabled = true;

    this._discardedPingsCount = 0;
    this._evictedPingsCount = 0;

    Services.obs.addObserver(this, TOPIC_IDLE_DAILY, false);

    this._server = Preferences.get(PREF_SERVER, undefined);

    // If any pings were submitted before the delayed init finished
    // we will send them now.
    yield this._sendPersistedPings();

    // Check the pending pings on disk now.
    yield this._checkPendingPings();
  }),

  _checkPendingPings: Task.async(function*() {
    // Scan the pending pings - that gives us a list sorted by last modified, descending.
    let infos = yield TelemetryStorage.loadPendingPingList();
    this._log.info("_checkPendingPings - pending ping count: " + infos.length);
    if (infos.length == 0) {
      this._log.trace("_checkPendingPings - no pending pings");
      return;
    }

    // Remove old pings that we haven't been able to send yet.
    const now = new Date();
    const tooOld = (info) => (now.getTime() - info.lastModificationDate) > MAX_PING_FILE_AGE;

    const oldPings = infos.filter((info) => tooOld(info));
    infos = infos.filter((info) => !tooOld(info));
    this._log.info("_checkPendingPings - clearing out " + oldPings.length + " old pings");

    for (let info of oldPings) {
      try {
        yield TelemetryStorage.removePendingPing(info.id);
        ++this._discardedPingsCount;
      } catch(ex) {
        this._log.error("_checkPendingPings - failed to remove old ping", ex);
      }
    }

    // Keep only the last MAX_LRU_PINGS entries to avoid that the backlog overgrows.
    const shouldEvict = infos.splice(MAX_LRU_PINGS, infos.length);
    let evictedCount = 0;
    this._log.info("_checkPendingPings - evicting " + shouldEvict.length + " pings to " +
                   "avoid overgrowing the backlog");

    for (let info of shouldEvict) {
      try {
        yield TelemetryStorage.removePendingPing(info.id);
        ++this._evictedPingsCount;
      } catch(ex) {
        this._log.error("_checkPendingPings - failed to evict ping", ex);
      }
    }

    Services.telemetry.getHistogramById('TELEMETRY_FILES_EVICTED')
                      .add(evictedCount);

    // Check for overdue pings.
    const overduePings = infos.filter((info) =>
      (now.getTime() - info.lastModificationDate) > OVERDUE_PING_FILE_AGE);
    this._overduePingCount = overduePings.length;


    if (overduePings.length > 0) {
      this._log.trace("_checkForOverduePings - Have " + overduePings.length +
                       " overdue pending pings, sending " + infos.length +
                       " pings now.");
      yield this._sendPersistedPings();
    }
   }),

  shutdown: Task.async(function*() {
    for (let topic of this.OBSERVER_TOPICS) {
      Services.obs.removeObserver(this, topic);
    }

    // We can't send anymore now.
    this._sendingEnabled = false;

    // Clear scheduled ping sends.
    this._clearPingSendTimer();
    // Cancel any outgoing requests.
    yield this._cancelOutgoingRequests();
    // ... and wait for any outstanding async ping activity.
    yield this._connectionsBarrier.wait();
  }),

  reset: function() {
    this._log.trace("reset");

    this._overduePingCount = 0;
    this._discardedPingsCount = 0;
    this._evictedPingsCount = 0;

    const histograms = [
      "TELEMETRY_SUCCESS",
      "TELEMETRY_FILES_EVICTED",
      "TELEMETRY_SEND",
      "TELEMETRY_PING",
    ];

    histograms.forEach(h => Telemetry.getHistogramById(h).clear());
  },

  observe: function(subject, topic, data) {
    switch(topic) {
    case TOPIC_IDLE_DAILY:
      this._sendPersistedPings();
      break;
    }
  },

  submitPing: function(ping) {
    if (!this._canSend()) {
      this._log.trace("submitPing - Telemetry is not allowed to send pings.");
      return Promise.resolve();
    }

    // Check if we can send pings now.
    const now = Policy.now();
    const nextPingSendTime = this._getNextPingSendTime(now);
    const throttled = (nextPingSendTime > now.getTime());

    // We can't send pings now, schedule a later send.
    if (throttled) {
      this._log.trace("submitPing - throttled, delaying ping send to " + new Date(nextPingSendTime));
      this._reschedulePingSendTimer(nextPingSendTime);
    }

    if (!this._sendingEnabled || throttled) {
      // Sending is disabled or throttled, add this to the pending pings.
      this._log.trace("submitPing - ping is pending, sendingEnabled: " + this._sendingEnabled +
                      ", throttled: " + throttled);
      return TelemetryStorage.savePendingPing(ping);
    }

    // Try to send the ping, persist it if sending it fails.
    this._log.trace("submitPing - already initialized, ping will be sent");
    let ps = [];
    ps.push(this._doPing(ping, ping.id, false)
                .catch((ex) => {
                  this._log.info("submitPing - ping not sent, saving to disk", ex);
                  TelemetryStorage.savePendingPing(ping);
                }));
    ps.push(this._sendPersistedPings());

    return Promise.all([for (p of ps) p.catch((ex) => {
      this._log.error("submitPing - ping activity had an error", ex);
    })]);
  },

  /**
   * Only used in tests.
   */
  setServer: function (server) {
    this._log.trace("setServer", server);
    this._server = server;
  },

  _cancelOutgoingRequests: function() {
    // Abort any pending ping XHRs.
    for (let [url, request] of this._pendingPingRequests) {
      this._log.trace("_cancelOutgoingRequests - aborting ping request for " + url);
      try {
        request.abort();
      } catch (e) {
        this._log.error("_cancelOutgoingRequests - failed to abort request to " + url, e);
      }
    }
    this._pendingPingRequests.clear();
  },

  /**
   * This helper calculates the next time that we can send pings at.
   * Currently this mostly redistributes ping sends from midnight until one hour after
   * to avoid submission spikes around local midnight for daily pings.
   *
   * @param now Date The current time.
   * @return Number The next time (ms from UNIX epoch) when we can send pings.
   */
  _getNextPingSendTime: function(now) {
    // 1. First we check if the time is between 0am and 1am. If it's not, we send
    // immediately.
    // 2. If we confirmed the time is indeed between 0am and 1am in step 1, we disallow
    // sending before (midnight + fuzzing delay), which is a random time between 0am-1am
    // (decided at startup).

    const midnight = Utils.truncateToDays(now);
    // Don't delay pings if we are not within the fuzzing interval.
    if ((now.getTime() - midnight.getTime()) > MIDNIGHT_FUZZING_INTERVAL_MS) {
      return now.getTime();
    }

    // Delay ping send if we are within the midnight fuzzing range.
    // We spread those ping sends out between |midnight| and |midnight + midnightPingFuzzingDelay|.
    return midnight.getTime() + Policy.midnightPingFuzzingDelay();
  },

  /**
   * Send the persisted pings to the server.
   *
   * @return Promise A promise that is resolved when all pings finished sending or failed.
   */
  _sendPersistedPings: Task.async(function*() {
    this._log.trace("_sendPersistedPings - Can send: " + this._canSend());

    if (TelemetryStorage.pendingPingCount < 1) {
      this._log.trace("_sendPersistedPings - no pings to send");
      return Promise.resolve();
    }

    if (!this._canSend()) {
      this._log.trace("_sendPersistedPings - Telemetry is not allowed to send pings.");
      return Promise.resolve();
    }

    // Check if we can send pings now - otherwise schedule a later send.
    const now = Policy.now();
    const nextPingSendTime = this._getNextPingSendTime(now);
    if (nextPingSendTime > now.getTime()) {
      this._log.trace("_sendPersistedPings - delaying ping send to " + new Date(nextPingSendTime));
      this._reschedulePingSendTimer(nextPingSendTime);
      return Promise.resolve();
    }

    // We can send now.
    const pendingPings = TelemetryStorage.getPendingPingList();
    this._log.trace("_sendPersistedPings - sending " + pendingPings.length + " pings");
    let pingSendPromises = [];
    for (let ping of pendingPings) {
      let p = ping;
      pingSendPromises.push(
        TelemetryStorage.loadPendingPing(p.id)
          .then((data) => this._doPing(data, p.id, true)
          .catch(e => this._log.error("_sendPersistedPings - _doPing rejected", e))));
    }

    let promise = Promise.all(pingSendPromises);
    this._trackPendingPingTask(promise);
    yield promise;
  }),

  _onPingRequestFinished: function(success, startTime, id, isPersisted) {
    this._log.trace("_onPingRequestFinished - success: " + success + ", persisted: " + isPersisted);

    Telemetry.getHistogramById("TELEMETRY_SEND").add(new Date() - startTime);
    let hping = Telemetry.getHistogramById("TELEMETRY_PING");
    let hsuccess = Telemetry.getHistogramById("TELEMETRY_SUCCESS");

    hsuccess.add(success);
    hping.add(new Date() - startTime);

    if (success && isPersisted) {
      return TelemetryStorage.removePendingPing(id);
    } else {
      return Promise.resolve();
    }
  },

  _getSubmissionPath: function(ping) {
    // The new ping format contains an "application" section, the old one doesn't.
    let pathComponents;
    if (isV4PingFormat(ping)) {
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
        ping.slug = Utils.generateUUID();
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

  _doPing: function(ping, id, isPersisted) {
    if (!this._canSend()) {
      // We can't send the pings to the server, so don't try to.
      this._log.trace("_doPing - Sending is disabled.");
      return Promise.resolve();
    }

    this._log.trace("_doPing - server: " + this._server + ", persisted: " + isPersisted +
                    ", id: " + id);
    const isNewPing = isV4PingFormat(ping);
    const version = isNewPing ? PING_FORMAT_VERSION : 1;
    const url = this._server + this._getSubmissionPath(ping) + "?v=" + version;

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
      this._onPingRequestFinished(success, startTime, id, isPersisted)
        .then(() => onCompletion(),
              (error) => {
                this._log.error("_doPing - request success: " + success + ", error" + error);
                onCompletion();
              });
    };

    let errorhandler = (event) => {
      this._log.error("_doPing - error making request to " + url + ": " + event.type);
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
        this._log.info("_doPing - successfully loaded, status: " + status);
        success = true;
      } else if (statusClass === 400) {
        // 4XX means that something with the request was broken.
        this._log.error("_doPing - error submitting to " + url + ", status: " + status
                        + " - ping request broken?");
        // TODO: we should handle this better, but for now we should avoid resubmitting
        // broken requests by pretending success.
        success = true;
      } else if (statusClass === 500) {
        // 5XX means there was a server-side error and we should try again later.
        this._log.error("_doPing - error submitting to " + url + ", status: " + status
                        + " - server error, should retry later");
      } else {
        // We received an unexpected status codes.
        this._log.error("_doPing - error submitting to " + url + ", status: " + status
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
    payloadStream.data = gzipCompressString(utf8Payload);
    Telemetry.getHistogramById("TELEMETRY_COMPRESS").add(new Date() - startTime);
    startTime = new Date();
    request.send(payloadStream);

    return deferred.promise;
  },

  /**
   * Check if pings can be sent to the server. If FHR is not allowed to upload,
   * pings are not sent to the server (Telemetry is a sub-feature of FHR).
   * If unified telemetry is off, don't send pings if Telemetry is disabled.
   *
   * @return {Boolean} True if pings can be send to the servers, false otherwise.
   */
  _canSend: function() {
    // We only send pings from official builds, but allow overriding this for tests.
    if (!Telemetry.isOfficialTelemetry && !this._testMode) {
      return false;
    }

    // With unified Telemetry, the FHR upload setting controls whether we can send pings.
    // The Telemetry pref enables sending extended data sets instead.
    if (IS_UNIFIED_TELEMETRY) {
      return Preferences.get(PREF_FHR_UPLOAD_ENABLED, false);
    }

    // Without unified Telemetry, the Telemetry enabled pref controls ping sending.
    return Preferences.get(PREF_TELEMETRY_ENABLED, false);
  },

  _reschedulePingSendTimer: function(timestamp) {
    this._clearPingSendTimer();
    const interval = timestamp - Policy.now();
    this._pingSendTimer = Policy.setPingSendTimeout(() => this._sendPersistedPings(), interval);
  },

  _clearPingSendTimer: function() {
    if (this._pingSendTimer) {
      Policy.clearPingSendTimeout(this._pingSendTimer);
      this._pingSendTimer = null;
    }
  },

  /**
   * Track any pending ping send and save tasks through the promise passed here.
   * This is needed to block shutdown on any outstanding ping activity.
   */
  _trackPendingPingTask: function (promise) {
    this._connectionsBarrier.client.addBlocker("Waiting for ping task", promise);
  },
};
