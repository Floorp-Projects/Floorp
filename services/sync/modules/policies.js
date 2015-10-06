/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [
  "ErrorHandler",
  "SyncScheduler",
];

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-common/logmanager.js");

XPCOMUtils.defineLazyModuleGetter(this, "Status",
                                  "resource://services-sync/status.js");

this.SyncScheduler = function SyncScheduler(service) {
  this.service = service;
  this.init();
}
SyncScheduler.prototype = {
  _log: Log.repository.getLogger("Sync.SyncScheduler"),

  _fatalLoginStatus: [LOGIN_FAILED_NO_USERNAME,
                      LOGIN_FAILED_NO_PASSWORD,
                      LOGIN_FAILED_NO_PASSPHRASE,
                      LOGIN_FAILED_INVALID_PASSPHRASE,
                      LOGIN_FAILED_LOGIN_REJECTED],

  /**
   * The nsITimer object that schedules the next sync. See scheduleNextSync().
   */
  syncTimer: null,

  setDefaults: function setDefaults() {
    this._log.trace("Setting SyncScheduler policy values to defaults.");

    let service = Cc["@mozilla.org/weave/service;1"]
                    .getService(Ci.nsISupports)
                    .wrappedJSObject;

    let part = service.fxAccountsEnabled ? "fxa" : "sync11";
    let prefSDInterval = "scheduler." + part + ".singleDeviceInterval";
    this.singleDeviceInterval = Svc.Prefs.get(prefSDInterval) * 1000;

    this.idleInterval         = Svc.Prefs.get("scheduler.idleInterval")         * 1000;
    this.activeInterval       = Svc.Prefs.get("scheduler.activeInterval")       * 1000;
    this.immediateInterval    = Svc.Prefs.get("scheduler.immediateInterval")    * 1000;
    this.eolInterval          = Svc.Prefs.get("scheduler.eolInterval")          * 1000;

    // A user is non-idle on startup by default.
    this.idle = false;

    this.hasIncomingItems = false;

    this.clearSyncTriggers();
  },

  // nextSync is in milliseconds, but prefs can't hold that much
  get nextSync() {
    return Svc.Prefs.get("nextSync", 0) * 1000;
  },
  set nextSync(value) {
    Svc.Prefs.set("nextSync", Math.floor(value / 1000));
  },

  get syncInterval() {
    return Svc.Prefs.get("syncInterval", this.singleDeviceInterval);
  },
  set syncInterval(value) {
    Svc.Prefs.set("syncInterval", value);
  },

  get syncThreshold() {
    return Svc.Prefs.get("syncThreshold", SINGLE_USER_THRESHOLD);
  },
  set syncThreshold(value) {
    Svc.Prefs.set("syncThreshold", value);
  },

  get globalScore() {
    return Svc.Prefs.get("globalScore", 0);
  },
  set globalScore(value) {
    Svc.Prefs.set("globalScore", value);
  },

  get numClients() {
    return Svc.Prefs.get("numClients", 0);
  },
  set numClients(value) {
    Svc.Prefs.set("numClients", value);
  },

  init: function init() {
    this._log.level = Log.Level[Svc.Prefs.get("log.logger.service.main")];
    this.setDefaults();
    Svc.Obs.add("weave:engine:score:updated", this);
    Svc.Obs.add("network:offline-status-changed", this);
    Svc.Obs.add("weave:service:sync:start", this);
    Svc.Obs.add("weave:service:sync:finish", this);
    Svc.Obs.add("weave:engine:sync:finish", this);
    Svc.Obs.add("weave:engine:sync:error", this);
    Svc.Obs.add("weave:service:login:error", this);
    Svc.Obs.add("weave:service:logout:finish", this);
    Svc.Obs.add("weave:service:sync:error", this);
    Svc.Obs.add("weave:service:backoff:interval", this);
    Svc.Obs.add("weave:service:ready", this);
    Svc.Obs.add("weave:engine:sync:applied", this);
    Svc.Obs.add("weave:service:setup-complete", this);
    Svc.Obs.add("weave:service:start-over", this);
    Svc.Obs.add("FxA:hawk:backoff:interval", this);

    if (Status.checkSetup() == STATUS_OK) {
      Svc.Obs.add("wake_notification", this);
      Svc.Idle.addIdleObserver(this, Svc.Prefs.get("scheduler.idleTime"));
    }
  },

  observe: function observe(subject, topic, data) {
    this._log.trace("Handling " + topic);
    switch(topic) {
      case "weave:engine:score:updated":
        if (Status.login == LOGIN_SUCCEEDED) {
          Utils.namedTimer(this.calculateScore, SCORE_UPDATE_DELAY, this,
                           "_scoreTimer");
        }
        break;
      case "network:offline-status-changed":
        // Whether online or offline, we'll reschedule syncs
        this._log.trace("Network offline status change: " + data);
        this.checkSyncStatus();
        break;
      case "weave:service:sync:start":
        // Clear out any potentially pending syncs now that we're syncing
        this.clearSyncTriggers();

        // reset backoff info, if the server tells us to continue backing off,
        // we'll handle that later
        Status.resetBackoff();

        this.globalScore = 0;
        break;
      case "weave:service:sync:finish":
        this.nextSync = 0;
        this.adjustSyncInterval();

        if (Status.service == SYNC_FAILED_PARTIAL && this.requiresBackoff) {
          this.requiresBackoff = false;
          this.handleSyncError();
          return;
        }

        let sync_interval;
        this._syncErrors = 0;
        if (Status.sync == NO_SYNC_NODE_FOUND) {
          this._log.trace("Scheduling a sync at interval NO_SYNC_NODE_FOUND.");
          sync_interval = NO_SYNC_NODE_INTERVAL;
        }
        this.scheduleNextSync(sync_interval);
        break;
      case "weave:engine:sync:finish":
        if (data == "clients") {
          // Update the client mode because it might change what we sync.
          this.updateClientMode();
        }
        break;
      case "weave:engine:sync:error":
        // `subject` is the exception thrown by an engine's sync() method.
        let exception = subject;
        if (exception.status >= 500 && exception.status <= 504) {
          this.requiresBackoff = true;
        }
        break;
      case "weave:service:login:error":
        this.clearSyncTriggers();

        if (Status.login == MASTER_PASSWORD_LOCKED) {
          // Try again later, just as if we threw an error... only without the
          // error count.
          this._log.debug("Couldn't log in: master password is locked.");
          this._log.trace("Scheduling a sync at MASTER_PASSWORD_LOCKED_RETRY_INTERVAL");
          this.scheduleAtInterval(MASTER_PASSWORD_LOCKED_RETRY_INTERVAL);
        } else if (this._fatalLoginStatus.indexOf(Status.login) == -1) {
          // Not a fatal login error, just an intermittent network or server
          // issue. Keep on syncin'.
          this.checkSyncStatus();
        }
        break;
      case "weave:service:logout:finish":
        // Start or cancel the sync timer depending on if
        // logged in or logged out
        this.checkSyncStatus();
        break;
      case "weave:service:sync:error":
        // There may be multiple clients but if the sync fails, client mode
        // should still be updated so that the next sync has a correct interval.
        this.updateClientMode();
        this.adjustSyncInterval();
        this.nextSync = 0;
        this.handleSyncError();
        break;
      case "FxA:hawk:backoff:interval":
      case "weave:service:backoff:interval":
        let requested_interval = subject * 1000;
        this._log.debug("Got backoff notification: " + requested_interval + "ms");
        // Leave up to 25% more time for the back off.
        let interval = requested_interval * (1 + Math.random() * 0.25);
        Status.backoffInterval = interval;
        Status.minimumNextSync = Date.now() + requested_interval;
        this._log.debug("Fuzzed minimum next sync: " + Status.minimumNextSync);
        break;
      case "weave:service:ready":
        // Applications can specify this preference if they want autoconnect
        // to happen after a fixed delay.
        let delay = Svc.Prefs.get("autoconnectDelay");
        if (delay) {
          this.delayedAutoConnect(delay);
        }
        break;
      case "weave:engine:sync:applied":
        let numItems = subject.succeeded;
        this._log.trace("Engine " + data + " successfully applied " + numItems +
                        " items.");
        if (numItems) {
          this.hasIncomingItems = true;
        }
        break;
      case "weave:service:setup-complete":
         Services.prefs.savePrefFile(null);
         Svc.Idle.addIdleObserver(this, Svc.Prefs.get("scheduler.idleTime"));
         Svc.Obs.add("wake_notification", this);
         break;
      case "weave:service:start-over":
         this.setDefaults();
         try {
           Svc.Idle.removeIdleObserver(this, Svc.Prefs.get("scheduler.idleTime"));
         } catch (ex if (ex.result == Cr.NS_ERROR_FAILURE)) {
           // In all likelihood we didn't have an idle observer registered yet.
           // It's all good.
         }
         break;
      case "idle":
        this._log.trace("We're idle.");
        this.idle = true;
        // Adjust the interval for future syncs. This won't actually have any
        // effect until the next pending sync (which will happen soon since we
        // were just active.)
        this.adjustSyncInterval();
        break;
      case "active":
        this._log.trace("Received notification that we're back from idle.");
        this.idle = false;
        Utils.namedTimer(function onBack() {
          if (this.idle) {
            this._log.trace("... and we're idle again. " +
                            "Ignoring spurious back notification.");
            return;
          }

          this._log.trace("Genuine return from idle. Syncing.");
          // Trigger a sync if we have multiple clients.
          if (this.numClients > 1) {
            this.scheduleNextSync(0);
          }
        }, IDLE_OBSERVER_BACK_DELAY, this, "idleDebouncerTimer");
        break;
      case "wake_notification":
        this._log.debug("Woke from sleep.");
        Utils.nextTick(() => {
          // Trigger a sync if we have multiple clients.
          if (this.numClients > 1) {
            this._log.debug("More than 1 client. Syncing.");
            this.scheduleNextSync(0);
          }
        });
        break;
    }
  },

  adjustSyncInterval: function adjustSyncInterval() {
    if (Status.eol) {
      this._log.debug("Server status is EOL; using eolInterval.");
      this.syncInterval = this.eolInterval;
      return;
    }

    if (this.numClients <= 1) {
      this._log.trace("Adjusting syncInterval to singleDeviceInterval.");
      this.syncInterval = this.singleDeviceInterval;
      return;
    }

    // Only MULTI_DEVICE clients will enter this if statement
    // since SINGLE_USER clients will be handled above.
    if (this.idle) {
      this._log.trace("Adjusting syncInterval to idleInterval.");
      this.syncInterval = this.idleInterval;
      return;
    }

    if (this.hasIncomingItems) {
      this._log.trace("Adjusting syncInterval to immediateInterval.");
      this.hasIncomingItems = false;
      this.syncInterval = this.immediateInterval;
    } else {
      this._log.trace("Adjusting syncInterval to activeInterval.");
      this.syncInterval = this.activeInterval;
    }
  },

  calculateScore: function calculateScore() {
    let engines = [this.service.clientsEngine].concat(this.service.engineManager.getEnabled());
    for (let i = 0;i < engines.length;i++) {
      this._log.trace(engines[i].name + ": score: " + engines[i].score);
      this.globalScore += engines[i].score;
      engines[i]._tracker.resetScore();
    }

    this._log.trace("Global score updated: " + this.globalScore);
    this.checkSyncStatus();
  },

  /**
   * Process the locally stored clients list to figure out what mode to be in
   */
  updateClientMode: function updateClientMode() {
    // Nothing to do if it's the same amount
    let numClients = this.service.clientsEngine.stats.numClients;
    if (this.numClients == numClients)
      return;

    this._log.debug("Client count: " + this.numClients + " -> " + numClients);
    this.numClients = numClients;

    if (numClients <= 1) {
      this._log.trace("Adjusting syncThreshold to SINGLE_USER_THRESHOLD");
      this.syncThreshold = SINGLE_USER_THRESHOLD;
    } else {
      this._log.trace("Adjusting syncThreshold to MULTI_DEVICE_THRESHOLD");
      this.syncThreshold = MULTI_DEVICE_THRESHOLD;
    }
    this.adjustSyncInterval();
  },

  /**
   * Check if we should be syncing and schedule the next sync, if it's not scheduled
   */
  checkSyncStatus: function checkSyncStatus() {
    // Should we be syncing now, if not, cancel any sync timers and return
    // if we're in backoff, we'll schedule the next sync.
    let ignore = [kSyncBackoffNotMet, kSyncMasterPasswordLocked];
    let skip = this.service._checkSync(ignore);
    this._log.trace("_checkSync returned \"" + skip + "\".");
    if (skip) {
      this.clearSyncTriggers();
      return;
    }

    // Only set the wait time to 0 if we need to sync right away
    let wait;
    if (this.globalScore > this.syncThreshold) {
      this._log.debug("Global Score threshold hit, triggering sync.");
      wait = 0;
    }
    this.scheduleNextSync(wait);
  },

  /**
   * Call sync() if Master Password is not locked.
   *
   * Otherwise, reschedule a sync for later.
   */
  syncIfMPUnlocked: function syncIfMPUnlocked() {
    // No point if we got kicked out by the master password dialog.
    if (Status.login == MASTER_PASSWORD_LOCKED &&
        Utils.mpLocked()) {
      this._log.debug("Not initiating sync: Login status is " + Status.login);

      // If we're not syncing now, we need to schedule the next one.
      this._log.trace("Scheduling a sync at MASTER_PASSWORD_LOCKED_RETRY_INTERVAL");
      this.scheduleAtInterval(MASTER_PASSWORD_LOCKED_RETRY_INTERVAL);
      return;
    }

    Utils.nextTick(this.service.sync, this.service);
  },

  /**
   * Set a timer for the next sync
   */
  scheduleNextSync: function scheduleNextSync(interval) {
    // If no interval was specified, use the current sync interval.
    if (interval == null) {
      interval = this.syncInterval;
    }

    // Ensure the interval is set to no less than the backoff.
    if (Status.backoffInterval && interval < Status.backoffInterval) {
      this._log.trace("Requested interval " + interval +
                      " ms is smaller than the backoff interval. " +
                      "Using backoff interval " +
                      Status.backoffInterval + " ms instead.");
      interval = Status.backoffInterval;
    }

    if (this.nextSync != 0) {
      // There's already a sync scheduled. Don't reschedule if there's already
      // a timer scheduled for sooner than requested.
      let currentInterval = this.nextSync - Date.now();
      this._log.trace("There's already a sync scheduled in " +
                      currentInterval + " ms.");
      if (currentInterval < interval && this.syncTimer) {
        this._log.trace("Ignoring scheduling request for next sync in " +
                        interval + " ms.");
        return;
      }
    }

    // Start the sync right away if we're already late.
    if (interval <= 0) {
      this._log.trace("Requested sync should happen right away.");
      this.syncIfMPUnlocked();
      return;
    }

    this._log.debug("Next sync in " + interval + " ms.");
    Utils.namedTimer(this.syncIfMPUnlocked, interval, this, "syncTimer");

    // Save the next sync time in-case sync is disabled (logout/offline/etc.)
    this.nextSync = Date.now() + interval;
  },


  /**
   * Incorporates the backoff/retry logic used in error handling and elective
   * non-syncing.
   */
  scheduleAtInterval: function scheduleAtInterval(minimumInterval) {
    let interval = Utils.calculateBackoff(this._syncErrors,
                                          MINIMUM_BACKOFF_INTERVAL,
                                          Status.backoffInterval);
    if (minimumInterval) {
      interval = Math.max(minimumInterval, interval);
    }

    this._log.debug("Starting client-initiated backoff. Next sync in " +
                    interval + " ms.");
    this.scheduleNextSync(interval);
  },

 /**
  * Automatically start syncing after the given delay (in seconds).
  *
  * Applications can define the `services.sync.autoconnectDelay` preference
  * to have this called automatically during start-up with the pref value as
  * the argument. Alternatively, they can call it themselves to control when
  * Sync should first start to sync.
  */
  delayedAutoConnect: function delayedAutoConnect(delay) {
    if (this.service._checkSetup() == STATUS_OK) {
      Utils.namedTimer(this.autoConnect, delay * 1000, this, "_autoTimer");
    }
  },

  autoConnect: function autoConnect() {
    if (this.service._checkSetup() == STATUS_OK && !this.service._checkSync()) {
      // Schedule a sync based on when a previous sync was scheduled.
      // scheduleNextSync() will do the right thing if that time lies in
      // the past.
      this.scheduleNextSync(this.nextSync - Date.now());
    }

    // Once autoConnect is called we no longer need _autoTimer.
    if (this._autoTimer) {
      this._autoTimer.clear();
    }
  },

  _syncErrors: 0,
  /**
   * Deal with sync errors appropriately
   */
  handleSyncError: function handleSyncError() {
    this._log.trace("In handleSyncError. Error count: " + this._syncErrors);
    this._syncErrors++;

    // Do nothing on the first couple of failures, if we're not in
    // backoff due to 5xx errors.
    if (!Status.enforceBackoff) {
      if (this._syncErrors < MAX_ERROR_COUNT_BEFORE_BACKOFF) {
        this.scheduleNextSync();
        return;
      }
      this._log.debug("Sync error count has exceeded " +
                      MAX_ERROR_COUNT_BEFORE_BACKOFF + "; enforcing backoff.");
      Status.enforceBackoff = true;
    }

    this.scheduleAtInterval();
  },


  /**
   * Remove any timers/observers that might trigger a sync
   */
  clearSyncTriggers: function clearSyncTriggers() {
    this._log.debug("Clearing sync triggers and the global score.");
    this.globalScore = this.nextSync = 0;

    // Clear out any scheduled syncs
    if (this.syncTimer)
      this.syncTimer.clear();
  },

  /**
   * Prevent new syncs from starting.  This is used by the FxA migration code
   * where we can't afford to have a sync start partway through the migration.
   * To handle the edge-case of a sync starting and not stopping, we store
   * this state in a pref, so on the next startup we remain blocked (and thus
   * sync will never start) so the migration can complete.
   *
   * As a safety measure, we only block for some period of time, and after
   * that it will automatically unblock.  This ensures that if things go
   * really pear-shaped and we never end up calling unblockSync() we haven't
   * completely broken the world.
   */
  blockSync: function(until = null) {
    if (!until) {
      until = Date.now() + DEFAULT_BLOCK_PERIOD;
    }
    // until is specified in ms, but Prefs can't hold that much
    Svc.Prefs.set("scheduler.blocked-until", Math.floor(until / 1000));
  },

  unblockSync: function() {
    Svc.Prefs.reset("scheduler.blocked-until");
    // the migration code should be ready to roll, so resume normal operations.
    this.checkSyncStatus();
  },

  get isBlocked() {
    let until = Svc.Prefs.get("scheduler.blocked-until");
    if (until === undefined) {
      return false;
    }
    if (until <= Math.floor(Date.now() / 1000)) {
      // we were previously blocked but the time has expired.
      Svc.Prefs.reset("scheduler.blocked-until");
      return false;
    }
    // we remain blocked.
    return true;
  },
};

this.ErrorHandler = function ErrorHandler(service) {
  this.service = service;
  this.init();
}
ErrorHandler.prototype = {
  MINIMUM_ALERT_INTERVAL_MSEC: 604800000,   // One week.

  /**
   * Flag that turns on error reporting for all errors, incl. network errors.
   */
  dontIgnoreErrors: false,

  /**
   * Flag that indicates if we have already reported a prolonged failure.
   * Once set, we don't report it again, meaning this error is only reported
   * one per run.
   */
  didReportProlongedError: false,

  init: function init() {
    Svc.Obs.add("weave:engine:sync:applied", this);
    Svc.Obs.add("weave:engine:sync:error", this);
    Svc.Obs.add("weave:service:login:error", this);
    Svc.Obs.add("weave:service:sync:error", this);
    Svc.Obs.add("weave:service:sync:finish", this);

    this.initLogs();
  },

  initLogs: function initLogs() {
    this._log = Log.repository.getLogger("Sync.ErrorHandler");
    this._log.level = Log.Level[Svc.Prefs.get("log.logger.service.main")];

    let root = Log.repository.getLogger("Sync");
    root.level = Log.Level[Svc.Prefs.get("log.rootLogger")];

    let logs = ["Sync", "FirefoxAccounts", "Hawk", "Common.TokenServerClient",
                "Sync.SyncMigration", "browserwindow.syncui"];

    this._logManager = new LogManager(Svc.Prefs, logs, "sync");
  },

  observe: function observe(subject, topic, data) {
    this._log.trace("Handling " + topic);
    switch(topic) {
      case "weave:engine:sync:applied":
        if (subject.newFailed) {
          // An engine isn't able to apply one or more incoming records.
          // We don't fail hard on this, but it usually indicates a bug,
          // so for now treat it as sync error (c.f. Service._syncEngine())
          Status.engines = [data, ENGINE_APPLY_FAIL];
          this._log.debug(data + " failed to apply some records.");
        }
        break;
      case "weave:engine:sync:error":
        let exception = subject;  // exception thrown by engine's sync() method
        let engine_name = data;   // engine name that threw the exception

        this.checkServerError(exception, "engines/" + engine_name);

        Status.engines = [engine_name, exception.failureCode || ENGINE_UNKNOWN_FAIL];
        this._log.debug(engine_name + " failed: " + Utils.exceptionStr(exception));

        Services.telemetry.getKeyedHistogramById("WEAVE_ENGINE_SYNC_ERRORS")
                          .add(engine_name);
        break;
      case "weave:service:login:error":
        this._log.error("Sync encountered a login error");
        this.resetFileLog();

        if (this.shouldReportError()) {
          this.notifyOnNextTick("weave:ui:login:error");
        } else {
          this.notifyOnNextTick("weave:ui:clear-error");
        }

        this.dontIgnoreErrors = false;
        break;
      case "weave:service:sync:error":
        if (Status.sync == CREDENTIALS_CHANGED) {
          this.service.logout();
        }

        this._log.error("Sync encountered an error");
        this.resetFileLog();

        if (this.shouldReportError()) {
          this.notifyOnNextTick("weave:ui:sync:error");
        } else {
          this.notifyOnNextTick("weave:ui:sync:finish");
        }

        this.dontIgnoreErrors = false;
        break;
      case "weave:service:sync:finish":
        this._log.trace("Status.service is " + Status.service);

        // Check both of these status codes: in the event of a failure in one
        // engine, Status.service will be SYNC_FAILED_PARTIAL despite
        // Status.sync being SYNC_SUCCEEDED.
        // *facepalm*
        if (Status.sync    == SYNC_SUCCEEDED &&
            Status.service == STATUS_OK) {
          // Great. Let's clear our mid-sync 401 note.
          this._log.trace("Clearing lastSyncReassigned.");
          Svc.Prefs.reset("lastSyncReassigned");
        }

        if (Status.service == SYNC_FAILED_PARTIAL) {
          this._log.error("Some engines did not sync correctly.");
          this.resetFileLog();

          if (this.shouldReportError()) {
            this.dontIgnoreErrors = false;
            this.notifyOnNextTick("weave:ui:sync:error");
            break;
          }
        } else {
          this.resetFileLog();
        }
        this.dontIgnoreErrors = false;
        this.notifyOnNextTick("weave:ui:sync:finish");
        break;
    }
  },

  notifyOnNextTick: function notifyOnNextTick(topic) {
    Utils.nextTick(function() {
      this._log.trace("Notifying " + topic +
                      ". Status.login is " + Status.login +
                      ". Status.sync is " + Status.sync);
      Svc.Obs.notify(topic);
    }, this);
  },

  /**
   * Trigger a sync and don't muffle any errors, particularly network errors.
   */
  syncAndReportErrors: function syncAndReportErrors() {
    this._log.debug("Beginning user-triggered sync.");

    this.dontIgnoreErrors = true;
    Utils.nextTick(this.service.sync, this.service);
  },

  /**
   * Generate a log file for the sync that just completed
   * and refresh the input & output streams.
   */
  resetFileLog: function resetFileLog() {
    let onComplete = logType => {
      Svc.Obs.notify("weave:service:reset-file-log");
      this._log.trace("Notified: " + Date.now());
      if (logType == this._logManager.ERROR_LOG_WRITTEN) {
        Cu.reportError("Sync encountered an error - see about:sync-log for the log file.");
      }
    };
    // Note we do not return the promise here - the caller doesn't need to wait
    // for this to complete.
    this._logManager.resetFileLog().then(onComplete, onComplete);
  },

  /**
   * Translates server error codes to meaningful strings.
   *
   * @param code
   *        server error code as an integer
   */
  errorStr: function errorStr(code) {
    switch (code.toString()) {
    case "1":
      return "illegal-method";
    case "2":
      return "invalid-captcha";
    case "3":
      return "invalid-username";
    case "4":
      return "cannot-overwrite-resource";
    case "5":
      return "userid-mismatch";
    case "6":
      return "json-parse-failure";
    case "7":
      return "invalid-password";
    case "8":
      return "invalid-record";
    case "9":
      return "weak-password";
    default:
      return "generic-server-error";
    }
  },

  // A function to indicate if Sync errors should be "reported" - which in this
  // context really means "should be notify observers of an error" - but note
  // that since bug 1180587, no one is going to surface an error to the user.
  shouldReportError: function shouldReportError() {
    if (Status.login == MASTER_PASSWORD_LOCKED) {
      this._log.trace("shouldReportError: false (master password locked).");
      return false;
    }

    if (this.dontIgnoreErrors) {
      return true;
    }

    if (Status.login == LOGIN_FAILED_LOGIN_REJECTED) {
      // An explicit LOGIN_REJECTED state is always reported (bug 1081158)
      this._log.trace("shouldReportError: true (login was rejected)");
      return true;
    }

    let lastSync = Svc.Prefs.get("lastSync");
    if (lastSync && ((Date.now() - Date.parse(lastSync)) >
        Svc.Prefs.get("errorhandler.networkFailureReportTimeout") * 1000)) {
      Status.sync = PROLONGED_SYNC_FAILURE;
      if (this.didReportProlongedError) {
        this._log.trace("shouldReportError: false (prolonged sync failure, but" +
                        " we've already reported it).");
        return false;
      }
      this._log.trace("shouldReportError: true (first prolonged sync failure).");
      this.didReportProlongedError = true;
      return true;
    }

    // We got a 401 mid-sync. Wait for the next sync before actually handling
    // an error. This assumes that we'll get a 401 again on a login fetch in
    // order to report the error.
    if (!this.service.clusterURL) {
      this._log.trace("shouldReportError: false (no cluster URL; " +
                      "possible node reassignment).");
      return false;
    }


    let result = ([Status.login, Status.sync].indexOf(SERVER_MAINTENANCE) == -1 &&
                  [Status.login, Status.sync].indexOf(LOGIN_FAILED_NETWORK_ERROR) == -1);
    this._log.trace("shouldReportError: ${result} due to login=${login}, sync=${sync}",
                    {result, login: Status.login, sync: Status.sync});
    return result;
  },

  get currentAlertMode() {
    return Svc.Prefs.get("errorhandler.alert.mode");
  },

  set currentAlertMode(str) {
    return Svc.Prefs.set("errorhandler.alert.mode", str);
  },

  get earliestNextAlert() {
    return Svc.Prefs.get("errorhandler.alert.earliestNext", 0) * 1000;
  },

  set earliestNextAlert(msec) {
    return Svc.Prefs.set("errorhandler.alert.earliestNext", msec / 1000);
  },

  clearServerAlerts: function () {
    // If we have any outstanding alerts, apparently they're no longer relevant.
    Svc.Prefs.resetBranch("errorhandler.alert");
  },

  /**
   * X-Weave-Alert headers can include a JSON object:
   *
   *   {
   *    "code":    // One of "hard-eol", "soft-eol".
   *    "url":     // For "Learn more" link.
   *    "message": // Logged in Sync logs.
   *   }
   */
  handleServerAlert: function (xwa) {
    if (!xwa.code) {
      this._log.warn("Got structured X-Weave-Alert, but no alert code.");
      return;
    }

    switch (xwa.code) {
      // Gently and occasionally notify the user that this service will be
      // shutting down.
      case "soft-eol":
        // Fall through.

      // Tell the user that this service has shut down, and drop our syncing
      // frequency dramatically.
      case "hard-eol":
        // Note that both of these alerts should be subservient to future "sign
        // in with your Firefox Account" storage alerts.
        if ((this.currentAlertMode != xwa.code) ||
            (this.earliestNextAlert < Date.now())) {
          Utils.nextTick(function() {
            Svc.Obs.notify("weave:eol", xwa);
          }, this);
          this._log.error("X-Weave-Alert: " + xwa.code + ": " + xwa.message);
          this.earliestNextAlert = Date.now() + this.MINIMUM_ALERT_INTERVAL_MSEC;
          this.currentAlertMode = xwa.code;
        }
        break;
      default:
        this._log.debug("Got unexpected X-Weave-Alert code: " + xwa.code);
    }
  },

  /**
   * Handle HTTP response results or exceptions and set the appropriate
   * Status.* bits.
   *
   * This method also looks for "side-channel" warnings.
   */
  checkServerError: function (resp, cause) {
    switch (resp.status) {
      case 200:
      case 404:
      case 513:
        let xwa = resp.headers['x-weave-alert'];

        // Only process machine-readable alerts.
        if (!xwa || !xwa.startsWith("{")) {
          this.clearServerAlerts();
          return;
        }

        try {
          xwa = JSON.parse(xwa);
        } catch (ex) {
          this._log.warn("Malformed X-Weave-Alert from server: " + xwa);
          return;
        }

        this.handleServerAlert(xwa);
        break;

      case 400:
        if (resp == RESPONSE_OVER_QUOTA) {
          Status.sync = OVER_QUOTA;
        }
        break;

      case 401:
        Services.telemetry.getKeyedHistogramById(
          "WEAVE_STORAGE_AUTH_ERRORS").add(cause);

        this.service.logout();
        this._log.info("Got 401 response; resetting clusterURL.");
        Svc.Prefs.reset("clusterURL");

        let delay = 0;
        if (Svc.Prefs.get("lastSyncReassigned")) {
          // We got a 401 in the middle of the previous sync, and we just got
          // another. Login must have succeeded in order for us to get here, so
          // the password should be correct.
          // This is likely to be an intermittent server issue, so back off and
          // give it time to recover.
          this._log.warn("Last sync also failed for 401. Delaying next sync.");
          delay = MINIMUM_BACKOFF_INTERVAL;
        } else {
          this._log.debug("New mid-sync 401 failure. Making a note.");
          Svc.Prefs.set("lastSyncReassigned", true);
        }
        this._log.info("Attempting to schedule another sync.");
        this.service.scheduler.scheduleNextSync(delay);
        break;

      case 500:
      case 502:
      case 503:
      case 504:
        Status.enforceBackoff = true;
        if (resp.status == 503 && resp.headers["retry-after"]) {
          let retryAfter = resp.headers["retry-after"];
          this._log.debug("Got Retry-After: " + retryAfter);
          if (this.service.isLoggedIn) {
            Status.sync = SERVER_MAINTENANCE;
          } else {
            Status.login = SERVER_MAINTENANCE;
          }
          Svc.Obs.notify("weave:service:backoff:interval",
                         parseInt(retryAfter, 10));
        }
        break;
    }

    switch (resp.result) {
      case Cr.NS_ERROR_UNKNOWN_HOST:
      case Cr.NS_ERROR_CONNECTION_REFUSED:
      case Cr.NS_ERROR_NET_TIMEOUT:
      case Cr.NS_ERROR_NET_RESET:
      case Cr.NS_ERROR_NET_INTERRUPT:
      case Cr.NS_ERROR_PROXY_CONNECTION_REFUSED:
        // The constant says it's about login, but in fact it just
        // indicates general network error.
        if (this.service.isLoggedIn) {
          Status.sync = LOGIN_FAILED_NETWORK_ERROR;
        } else {
          Status.login = LOGIN_FAILED_NETWORK_ERROR;
        }
        break;
    }
  },
};
