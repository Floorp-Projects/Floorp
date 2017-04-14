/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "SessionRecorder",
];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-common/utils.js");

// We automatically prune sessions older than this.
const MAX_SESSION_AGE_MS = 7 * 24 * 60 * 60 * 1000; // 7 days.
const STARTUP_RETRY_INTERVAL_MS = 5000;

// Wait up to 5 minutes for startup measurements before giving up.
const MAX_STARTUP_TRIES = 300000 / STARTUP_RETRY_INTERVAL_MS;

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "SessionRecorder::";

/**
 * Records information about browser sessions.
 *
 * This serves as an interface to both current session information as
 * well as a history of previous sessions.
 *
 * Typically only one instance of this will be installed in an
 * application. It is typically managed by an XPCOM service. The
 * instance is instantiated at application start; onStartup is called
 * once the profile is installed; onShutdown is called during shutdown.
 *
 * We currently record state in preferences. However, this should be
 * invisible to external consumers. We could easily swap in a different
 * storage mechanism if desired.
 *
 * Please note the different semantics for storing times and dates in
 * preferences. Full dates (notably the session start time) are stored
 * as strings because preferences have a 32-bit limit on integer values
 * and milliseconds since UNIX epoch would overflow. Many times are
 * stored as integer offsets from the session start time because they
 * should not overflow 32 bits.
 *
 * Since this records history of all sessions, there is a possibility
 * for unbounded data aggregation. This is curtailed through:
 *
 *   1) An "idle-daily" observer which delete sessions older than
 *      MAX_SESSION_AGE_MS.
 *   2) The creator of this instance explicitly calling
 *      `pruneOldSessions`.
 *
 * @param branch
 *        (string) Preferences branch on which to record state.
 */
this.SessionRecorder = function(branch) {
  if (!branch) {
    throw new Error("branch argument must be defined.");
  }

  if (!branch.endsWith(".")) {
    throw new Error("branch argument must end with '.': " + branch);
  }

  this._log = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, LOGGER_PREFIX);

  this._prefs = new Preferences(branch);
  this._lastActivityWasInactive = false;
  this._activeTicks = 0;
  this.fineTotalTime = 0;
  this._started = false;
  this._timer = null;
  this._startupFieldTries = 0;

  this._os = Cc["@mozilla.org/observer-service;1"]
               .getService(Ci.nsIObserverService);

};

SessionRecorder.prototype = Object.freeze({
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  STARTUP_RETRY_INTERVAL_MS,

  get _currentIndex() {
    return this._prefs.get("currentIndex", 0);
  },

  set _currentIndex(value) {
    this._prefs.set("currentIndex", value);
  },

  get _prunedIndex() {
    return this._prefs.get("prunedIndex", 0);
  },

  set _prunedIndex(value) {
    this._prefs.set("prunedIndex", value);
  },

  get startDate() {
    return CommonUtils.getDatePref(this._prefs, "current.startTime");
  },

  set _startDate(value) {
    CommonUtils.setDatePref(this._prefs, "current.startTime", value);
  },

  get activeTicks() {
    return this._prefs.get("current.activeTicks", 0);
  },

  incrementActiveTicks() {
    this._prefs.set("current.activeTicks", ++this._activeTicks);
  },

  /**
   * Total time of this session in integer seconds.
   *
   * See also fineTotalTime for the time in milliseconds.
   */
  get totalTime() {
    return this._prefs.get("current.totalTime", 0);
  },

  updateTotalTime() {
    // We store millisecond precision internally to prevent drift from
    // repeated rounding.
    this.fineTotalTime = Date.now() - this.startDate;
    this._prefs.set("current.totalTime", Math.floor(this.fineTotalTime / 1000));
  },

  get main() {
    return this._prefs.get("current.main", -1);
  },

  set _main(value) {
    if (!Number.isInteger(value)) {
      throw new Error("main time must be an integer.");
    }

    this._prefs.set("current.main", value);
  },

  get firstPaint() {
    return this._prefs.get("current.firstPaint", -1);
  },

  set _firstPaint(value) {
    if (!Number.isInteger(value)) {
      throw new Error("firstPaint must be an integer.");
    }

    this._prefs.set("current.firstPaint", value);
  },

  get sessionRestored() {
    return this._prefs.get("current.sessionRestored", -1);
  },

  set _sessionRestored(value) {
    if (!Number.isInteger(value)) {
      throw new Error("sessionRestored must be an integer.");
    }

    this._prefs.set("current.sessionRestored", value);
  },

  getPreviousSessions() {
    let result = {};

    for (let i = this._prunedIndex; i < this._currentIndex; i++) {
      let s = this.getPreviousSession(i);
      if (!s) {
        continue;
      }

      result[i] = s;
    }

    return result;
  },

  getPreviousSession(index) {
    return this._deserialize(this._prefs.get("previous." + index));
  },

  /**
   * Prunes old, completed sessions that started earlier than the
   * specified date.
   */
  pruneOldSessions(date) {
    for (let i = this._prunedIndex; i < this._currentIndex; i++) {
      let s = this.getPreviousSession(i);
      if (!s) {
        continue;
      }

      if (s.startDate >= date) {
        continue;
      }

      this._log.debug("Pruning session #" + i + ".");
      this._prefs.reset("previous." + i);
      this._prunedIndex = i;
    }
  },

  recordStartupFields() {
    let si = this._getStartupInfo();

    if (!si.process) {
      throw new Error("Startup info not available.");
    }

    let missing = false;

    for (let field of ["main", "firstPaint", "sessionRestored"]) {
      if (!(field in si)) {
        this._log.debug("Missing startup field: " + field);
        missing = true;
        continue;
      }

      this["_" + field] = si[field].getTime() - si.process.getTime();
    }

    if (!missing || this._startupFieldTries > MAX_STARTUP_TRIES) {
      this._clearStartupTimer();
      return;
    }

    // If we have missing fields, install a timer and keep waiting for
    // data.
    this._startupFieldTries++;

    if (!this._timer) {
      this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      this._timer.initWithCallback({
        notify: this.recordStartupFields.bind(this),
      }, this.STARTUP_RETRY_INTERVAL_MS, this._timer.TYPE_REPEATING_SLACK);
    }
  },

  _clearStartupTimer() {
    if (this._timer) {
      this._timer.cancel();
      delete this._timer;
    }
  },

  /**
   * Perform functionality on application startup.
   *
   * This is typically called in a "profile-do-change" handler.
   */
  onStartup() {
    if (this._started) {
      throw new Error("onStartup has already been called.");
    }

    let si = this._getStartupInfo();
    if (!si.process) {
      throw new Error("Process information not available. Misconfigured app?");
    }

    this._started = true;

    this._os.addObserver(this, "profile-before-change", false);
    this._os.addObserver(this, "user-interaction-active", false);
    this._os.addObserver(this, "user-interaction-inactive", false);
    this._os.addObserver(this, "idle-daily", false);

    // This has the side-effect of clearing current session state.
    this._moveCurrentToPrevious();

    this._startDate = si.process;
    this._prefs.set("current.activeTicks", 0);
    this.updateTotalTime();

    this.recordStartupFields();
  },

  /**
   * Record application activity.
   */
  onActivity(active) {
    let updateActive = active && !this._lastActivityWasInactive;
    this._lastActivityWasInactive = !active;

    this.updateTotalTime();

    if (updateActive) {
      this.incrementActiveTicks();
    }
  },

  onShutdown() {
    this._log.info("Recording clean session shutdown.");
    this._prefs.set("current.clean", true);
    this.updateTotalTime();
    this._clearStartupTimer();

    this._os.removeObserver(this, "profile-before-change");
    this._os.removeObserver(this, "user-interaction-active");
    this._os.removeObserver(this, "user-interaction-inactive");
    this._os.removeObserver(this, "idle-daily");
  },

  _CURRENT_PREFS: [
    "current.startTime",
    "current.activeTicks",
    "current.totalTime",
    "current.main",
    "current.firstPaint",
    "current.sessionRestored",
    "current.clean",
  ],

  // This is meant to be called only during onStartup().
  _moveCurrentToPrevious() {
    try {
      if (!this.startDate.getTime()) {
        this._log.info("No previous session. Is this first app run?");
        return;
      }

      let clean = this._prefs.get("current.clean", false);

      let count = this._currentIndex++;
      let obj = {
        s: this.startDate.getTime(),
        a: this.activeTicks,
        t: this.totalTime,
        c: clean,
        m: this.main,
        fp: this.firstPaint,
        sr: this.sessionRestored,
      };

      this._log.debug("Recording last sessions as #" + count + ".");
      this._prefs.set("previous." + count, JSON.stringify(obj));
    } catch (ex) {
      this._log.warn("Exception when migrating last session", ex);
    } finally {
      this._log.debug("Resetting prefs from last session.");
      for (let pref of this._CURRENT_PREFS) {
        this._prefs.reset(pref);
      }
    }
  },

  _deserialize(s) {
    let o;
    try {
      o = JSON.parse(s);
    } catch (ex) {
      return null;
    }

    return {
      startDate: new Date(o.s),
      activeTicks: o.a,
      totalTime: o.t,
      clean: !!o.c,
      main: o.m,
      firstPaint: o.fp,
      sessionRestored: o.sr,
    };
  },

  // Implemented as a function to allow for monkeypatching in tests.
  _getStartupInfo() {
    return Cc["@mozilla.org/toolkit/app-startup;1"]
             .getService(Ci.nsIAppStartup)
             .getStartupInfo();
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "profile-before-change":
        this.onShutdown();
        break;

      case "user-interaction-active":
        this.onActivity(true);
        break;

      case "user-interaction-inactive":
        this.onActivity(false);
        break;

      case "idle-daily":
        this.pruneOldSessions(new Date(Date.now() - MAX_SESSION_AGE_MS));
        break;
    }
  },
});
