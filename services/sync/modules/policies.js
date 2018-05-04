/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = [
  "ErrorHandler",
  "SyncScheduler",
];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Log.jsm");
ChromeUtils.import("resource://services-sync/constants.js");
ChromeUtils.import("resource://services-sync/util.js");
ChromeUtils.import("resource://services-common/logmanager.js");
ChromeUtils.import("resource://services-common/async.js");
ChromeUtils.import("resource://services-common/utils.js");

ChromeUtils.defineModuleGetter(this, "Status",
                               "resource://services-sync/status.js");
ChromeUtils.defineModuleGetter(this, "AddonManager",
                               "resource://gre/modules/AddonManager.jsm");
ChromeUtils.defineModuleGetter(this, "fxAccounts",
                               "resource://gre/modules/FxAccounts.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "IdleService",
                                   "@mozilla.org/widget/idleservice;1",
                                   "nsIIdleService");
XPCOMUtils.defineLazyServiceGetter(this, "CaptivePortalService",
                                   "@mozilla.org/network/captive-portal-service;1",
                                   "nsICaptivePortalService");

// Get the value for an interval that's stored in preferences. To save users
// from themselves (and us from them!) the minimum time they can specify
// is 60s.
function getThrottledIntervalPreference(prefName) {
  return Math.max(Svc.Prefs.get(prefName), 60) * 1000;
}

function SyncScheduler(service) {
  this.service = service;
  this.init();
}
SyncScheduler.prototype = {
  _log: Log.repository.getLogger("Sync.SyncScheduler"),

  _fatalLoginStatus: [LOGIN_FAILED_NO_PASSPHRASE,
                      LOGIN_FAILED_INVALID_PASSPHRASE,
                      LOGIN_FAILED_LOGIN_REJECTED],

  /**
   * The nsITimer object that schedules the next sync. See scheduleNextSync().
   */
  syncTimer: null,

  setDefaults: function setDefaults() {
    this._log.trace("Setting SyncScheduler policy values to defaults.");

    this.singleDeviceInterval = getThrottledIntervalPreference("scheduler.fxa.singleDeviceInterval");
    this.idleInterval         = getThrottledIntervalPreference("scheduler.idleInterval");
    this.activeInterval       = getThrottledIntervalPreference("scheduler.activeInterval");
    this.immediateInterval    = getThrottledIntervalPreference("scheduler.immediateInterval");

    // A user is non-idle on startup by default.
    this.idle = false;

    this.hasIncomingItems = false;
    // This is the last number of clients we saw when previously updating the
    // client mode. If this != currentNumClients (obtained from prefs written
    // by the clients engine) then we need to transition to and from
    // single and multi-device mode.
    this.numClientsLastSync = 0;

    this._resyncs = 0;

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
    return this._syncInterval;
  },
  set syncInterval(value) {
    if (value != this._syncInterval) {
      Services.prefs.setIntPref("services.sync.syncInterval", value);
    }
  },

  get syncThreshold() {
    return this._syncThreshold;
  },
  set syncThreshold(value) {
    if (value != this._syncThreshold) {
      Services.prefs.setIntPref("services.sync.syncThreshold", value);
    }
  },

  get globalScore() {
    return this._globalScore;
  },
  set globalScore(value) {
    if (this._globalScore != value) {
      Services.prefs.setIntPref("services.sync.globalScore", value);
    }
  },

  // Managed by the clients engine (by way of prefs)
  get numClients() {
    return this.numDesktopClients + this.numMobileClients;
  },
  set numClients(value) {
    throw new Error("Don't set numClients - the clients engine manages it.");
  },

  get offline() {
    // Services.io.offline has slowly become fairly useless over the years - it
    // no longer attempts to track the actual network state by default, but one
    // thing stays true: if it says we're offline then we are definitely not online.
    //
    // We also ask the captive portal service if we are behind a locked captive
    // portal.
    //
    // We don't check on the NetworkLinkService however, because it gave us
    // false positives in the past in a vm environment.
    try {
      if (Services.io.offline ||
          CaptivePortalService.state == CaptivePortalService.LOCKED_PORTAL) {
        return true;
      }
    } catch (ex) {
      this._log.warn("Could not determine network status.", ex);
    }
    return false;
  },

  _initPrefGetters() {

    XPCOMUtils.defineLazyPreferenceGetter(this,
      "idleTime", "services.sync.scheduler.idleTime");
    XPCOMUtils.defineLazyPreferenceGetter(this,
      "maxResyncs", "services.sync.maxResyncs", 0);

    // The number of clients we have is maintained in preferences via the
    // clients engine, and only updated after a successsful sync.
    XPCOMUtils.defineLazyPreferenceGetter(this,
      "numDesktopClients", "services.sync.clients.devices.desktop", 0);
    XPCOMUtils.defineLazyPreferenceGetter(this,
      "numMobileClients", "services.sync.clients.devices.mobile", 0);

    // Scheduler state that seems to be read more often than it's written.
    // We also check if the value has changed before writing in the setters.
    XPCOMUtils.defineLazyPreferenceGetter(this,
      "_syncThreshold", "services.sync.syncThreshold", SINGLE_USER_THRESHOLD);
    XPCOMUtils.defineLazyPreferenceGetter(this,
      "_syncInterval", "services.sync.syncInterval", this.singleDeviceInterval);
    XPCOMUtils.defineLazyPreferenceGetter(this,
      "_globalScore", "services.sync.globalScore", 0);
  },

  init: function init() {
    this._log.manageLevelFromPref("services.sync.log.logger.service.main");
    this.setDefaults();
    this._initPrefGetters();
    Svc.Obs.add("weave:engine:score:updated", this);
    Svc.Obs.add("network:offline-status-changed", this);
    Svc.Obs.add("network:link-status-changed", this);
    Svc.Obs.add("captive-portal-detected", this);
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
      Svc.Obs.add("captive-portal-login-success", this);
      Svc.Obs.add("sleep_notification", this);
      IdleService.addIdleObserver(this, this.idleTime);
    }
  },

  // eslint-disable-next-line complexity
  observe: function observe(subject, topic, data) {
    this._log.trace("Handling " + topic);
    switch (topic) {
      case "weave:engine:score:updated":
        if (Status.login == LOGIN_SUCCEEDED) {
          CommonUtils.namedTimer(this.calculateScore, SCORE_UPDATE_DELAY, this,
                           "_scoreTimer");
        }
        break;
      case "network:link-status-changed":
        // Note: NetworkLinkService is unreliable, we get false negatives for it
        // in cases such as VMs (bug 1420802), so we don't want to use it in
        // `get offline`, but we assume that it's probably reliable if we're
        // getting status changed events. (We might be wrong about this, but
        // if that's true, then the only downside is that we won't sync as
        // promptly).
        let isOffline = this.offline;
        this._log.debug(`Network link status changed to "${data}". Offline?`,
                        isOffline);
        // Data may be one of `up`, `down`, `change`, or `unknown`. We only want
        // to sync if it's "up".
        if (data == "up" && !isOffline) {
          this._log.debug("Network link looks up. Syncing.");
          this.scheduleNextSync(0, {why: topic});
        } else if (data == "down") {
          // Unschedule pending syncs if we know we're going down. We don't do
          // this via `checkSyncStatus`, since link status isn't reflected in
          // `this.offline`.
          this.clearSyncTriggers();
        }
        break;
      case "network:offline-status-changed":
      case "captive-portal-detected":
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
        this.updateGlobalScore();
        if (this.globalScore > 0 && Status.service == STATUS_OK) {
          // The global score should be 0 after a sync. If it's not, items were
          // changed during the last sync, and we should schedule an immediate
          // follow-up sync.
          this._resyncs++;
          if (this._resyncs <= this.maxResyncs) {
            sync_interval = 0;
          } else {
            this._log.warn(`Resync attempt ${this._resyncs} exceeded ` +
                           `maximum ${this.maxResyncs}`);
            Svc.Obs.notify("weave:service:resyncs-finished");
          }
        } else {
          this._resyncs = 0;
          Svc.Obs.notify("weave:service:resyncs-finished");
        }

        this._syncErrors = 0;
        if (Status.sync == NO_SYNC_NODE_FOUND) {
          // If we don't have a Sync node, override the interval, even if we've
          // scheduled a follow-up sync.
          this._log.trace("Scheduling a sync at interval NO_SYNC_NODE_FOUND.");
          sync_interval = NO_SYNC_NODE_INTERVAL;
        }
        this.scheduleNextSync(sync_interval, {why: "schedule"});
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
        } else if (!this._fatalLoginStatus.includes(Status.login)) {
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
        if (subject.newFailed) {
          this._log.error(`Engine ${data} found ${subject.newFailed} new records that failed to apply`);
        }
        break;
      case "weave:service:setup-complete":
         Services.prefs.savePrefFile(null);
         IdleService.addIdleObserver(this, this.idleTime);
         Svc.Obs.add("wake_notification", this);
         Svc.Obs.add("captive-portal-login-success", this);
         Svc.Obs.add("sleep_notification", this);
         break;
      case "weave:service:start-over":
         this.setDefaults();
         try {
           IdleService.removeIdleObserver(this, this.idleTime);
         } catch (ex) {
           if (ex.result != Cr.NS_ERROR_FAILURE) {
             throw ex;
           }
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
        CommonUtils.namedTimer(function onBack() {
          if (this.idle) {
            this._log.trace("... and we're idle again. " +
                            "Ignoring spurious back notification.");
            return;
          }

          this._log.trace("Genuine return from idle. Syncing.");
          // Trigger a sync if we have multiple clients.
          if (this.numClients > 1) {
            this.scheduleNextSync(0, {why: topic});
          }
        }, IDLE_OBSERVER_BACK_DELAY, this, "idleDebouncerTimer");
        break;
      case "wake_notification":
        this._log.debug("Woke from sleep.");
        CommonUtils.nextTick(() => {
          // Trigger a sync if we have multiple clients. We give it 2 seconds
          // so the browser can recover from the wake and do more important
          // operations first (timers etc).
          if (this.numClients > 1) {
            if (!this.offline) {
              this._log.debug("Online, will sync in 2s.");
              this.scheduleNextSync(2000, {why: topic});
            }
          }
        });
        break;
      case "captive-portal-login-success":
        this._log.debug("Captive portal login success. Scheduling a sync.");
        CommonUtils.nextTick(() => {
          this.scheduleNextSync(3000, {why: topic});
        });
        break;
      case "sleep_notification":
        if (this.service.engineManager.get("tabs")._tracker.modified) {
          this._log.debug("Going to sleep, doing a quick sync.");
          this.scheduleNextSync(0, {engines: ["tabs"], why: "sleep"});
        }
        break;
    }
  },

  adjustSyncInterval: function adjustSyncInterval() {
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

  updateGlobalScore() {
    let engines = [this.service.clientsEngine].concat(this.service.engineManager.getEnabled());
    let globalScore = this.globalScore;
    for (let i = 0;i < engines.length;i++) {
      this._log.trace(engines[i].name + ": score: " + engines[i].score);
      globalScore += engines[i].score;
      engines[i]._tracker.resetScore();
    }
    this.globalScore = globalScore;
    this._log.trace("Global score updated: " + globalScore);
  },

  calculateScore() {
    this.updateGlobalScore();
    this.checkSyncStatus();
  },

  /**
   * Query the number of known clients to figure out what mode to be in
   */
  updateClientMode: function updateClientMode() {
    // Nothing to do if it's the same amount
    let numClients = this.numClients;
    if (numClients == this.numClientsLastSync)
      return;

    this._log.debug(`Client count: ${this.numClientsLastSync} -> ${numClients}`);
    this.numClientsLastSync = numClients;

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

    let why = "schedule";
    // Only set the wait time to 0 if we need to sync right away
    let wait;
    if (this.globalScore > this.syncThreshold) {
      this._log.debug("Global Score threshold hit, triggering sync.");
      wait = 0;
      why = "score";
    }
    this.scheduleNextSync(wait, {why});
  },

  /**
   * Call sync() if Master Password is not locked.
   *
   * Otherwise, reschedule a sync for later.
   */
  syncIfMPUnlocked(engines, why) {
    // No point if we got kicked out by the master password dialog.
    if (Status.login == MASTER_PASSWORD_LOCKED &&
        Utils.mpLocked()) {
      this._log.debug("Not initiating sync: Login status is " + Status.login);

      // If we're not syncing now, we need to schedule the next one.
      this._log.trace("Scheduling a sync at MASTER_PASSWORD_LOCKED_RETRY_INTERVAL");
      this.scheduleAtInterval(MASTER_PASSWORD_LOCKED_RETRY_INTERVAL);
      return;
    }

    if (!Async.isAppReady()) {
      this._log.debug("Not initiating sync: app is shutting down");
      return;
    }
    Services.tm.dispatchToMainThread(() => {
      // Terrible hack below: we do the fxa messages polling in the sync
      // scheduler to get free post-wake/link-state etc detection.
      fxAccounts.messages.consumeRemoteMessages().catch(e => {
        this._log.error("Error while polling for FxA messages.", e);
      });
      this.service.sync({engines, why});
    });
  },

  /**
   * Set a timer for the next sync
   */
  scheduleNextSync(interval, {engines = null, why = null} = {}) {
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
    let nextSync = this.nextSync;
    if (nextSync != 0) {
      // There's already a sync scheduled. Don't reschedule if there's already
      // a timer scheduled for sooner than requested.
      let currentInterval = nextSync - Date.now();
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
      this._log.trace(`Requested sync should happen right away. (why=${why})`);
      this.syncIfMPUnlocked(engines, why);
      return;
    }

    this._log.debug(`Next sync in ${interval} ms. (why=${why})`);
    CommonUtils.namedTimer(() => { this.syncIfMPUnlocked(engines, why); },
                           interval, this, "syncTimer");

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
    this.scheduleNextSync(interval, {why: "client-backoff-schedule"});
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
      CommonUtils.namedTimer(this.autoConnect, delay * 1000, this, "_autoTimer");
    }
  },

  autoConnect: function autoConnect() {
    if (this.service._checkSetup() == STATUS_OK && !this.service._checkSync()) {
      // Schedule a sync based on when a previous sync was scheduled.
      // scheduleNextSync() will do the right thing if that time lies in
      // the past.
      this.scheduleNextSync(this.nextSync - Date.now(), {why: "startup"});
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
        this.scheduleNextSync(null, {why: "reschedule"});
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

};

function ErrorHandler(service) {
  this.service = service;
  this.init();
}
ErrorHandler.prototype = {
  init() {
    Svc.Obs.add("weave:engine:sync:applied", this);
    Svc.Obs.add("weave:engine:sync:error", this);
    Svc.Obs.add("weave:service:login:error", this);
    Svc.Obs.add("weave:service:sync:error", this);
    Svc.Obs.add("weave:service:sync:finish", this);
    Svc.Obs.add("weave:service:start-over:finish", this);

    this.initLogs();
  },

  initLogs: function initLogs() {
    // Set the root Sync logger level based on a pref. All other logs will
    // inherit this level unless they specifically override it.
    Log.repository.getLogger("Sync").manageLevelFromPref(`services.sync.log.logger`);
    // And allow our specific log to have a custom level via a pref.
    this._log = Log.repository.getLogger("Sync.ErrorHandler");
    this._log.manageLevelFromPref("services.sync.log.logger.service.main");

    let logs = ["Sync", "Services.Common", "FirefoxAccounts", "Hawk",
                "browserwindow.syncui", "BookmarkSyncUtils", "addons.xpi",
               ];

    this._logManager = new LogManager(Svc.Prefs, logs, "sync");
  },

  observe(subject, topic, data) {
    this._log.trace("Handling " + topic);
    switch (topic) {
      case "weave:engine:sync:applied":
        if (subject.newFailed) {
          // An engine isn't able to apply one or more incoming records.
          // We don't fail hard on this, but it usually indicates a bug,
          // so for now treat it as sync error (c.f. Service._syncEngine())
          Status.engines = [data, ENGINE_APPLY_FAIL];
          this._log.debug(data + " failed to apply some records.");
        }
        break;
      case "weave:engine:sync:error": {
        let exception = subject; // exception thrown by engine's sync() method
        let engine_name = data; // engine name that threw the exception

        this.checkServerError(exception);

        Status.engines = [engine_name, exception.failureCode || ENGINE_UNKNOWN_FAIL];
        if (Async.isShutdownException(exception)) {
          this._log.debug(engine_name + " was interrupted due to the application shutting down");
        } else {
          this._log.debug(engine_name + " failed", exception);
        }
        break;
      }
      case "weave:service:login:error":
        this._log.error("Sync encountered a login error");
        this.resetFileLog();
        break;
      case "weave:service:sync:error": {
        if (Status.sync == CREDENTIALS_CHANGED) {
          this.service.logout();
        }

        let exception = subject;
        if (Async.isShutdownException(exception)) {
          // If we are shutting down we just log the fact, attempt to flush
          // the log file and get out of here!
          this._log.error("Sync was interrupted due to the application shutting down");
          this.resetFileLog();
          break;
        }

        // Not a shutdown related exception...
        this._log.error("Sync encountered an error", exception);
        this.resetFileLog();
        break;
      }
      case "weave:service:sync:finish":
        this._log.trace("Status.service is " + Status.service);

        // Check both of these status codes: in the event of a failure in one
        // engine, Status.service will be SYNC_FAILED_PARTIAL despite
        // Status.sync being SYNC_SUCCEEDED.
        // *facepalm*
        if (Status.sync == SYNC_SUCCEEDED &&
            Status.service == STATUS_OK) {
          // Great. Let's clear our mid-sync 401 note.
          this._log.trace("Clearing lastSyncReassigned.");
          Svc.Prefs.reset("lastSyncReassigned");
        }

        if (Status.service == SYNC_FAILED_PARTIAL) {
          this._log.error("Some engines did not sync correctly.");
        }
        this.resetFileLog();
        break;
      case "weave:service:start-over:finish":
        // ensure we capture any logs between the last sync and the reset completing.
        this.resetFileLog().then(() => {
          // although for privacy reasons we also delete all logs (but we allow
          // a preference to avoid this to help with debugging.)
          if (!Svc.Prefs.get("log.keepLogsOnReset", false)) {
            return this._logManager.removeAllLogs().then(() => {
              Svc.Obs.notify("weave:service:remove-file-log");
            });
          }
          return null;
        }).catch(err => {
          // So we failed to delete the logs - take the ironic option of
          // writing this error to the logs we failed to delete!
          this._log.error("Failed to delete logs on reset", err);
        });
        break;
    }
  },

  async _dumpAddons() {
    // Just dump the items that sync may be concerned with. Specifically,
    // active extensions that are not hidden.
    let addons = [];
    try {
      addons = await AddonManager.getAddonsByTypes(["extension"]);
    } catch (e) {
      this._log.warn("Failed to dump addons", e);
    }

    let relevantAddons = addons.filter(x => x.isActive && !x.hidden);
    this._log.trace("Addons installed", relevantAddons.length);
    for (let addon of relevantAddons) {
      this._log.trace(" - ${name}, version ${version}, id ${id}", addon);
    }
  },

  /**
   * Generate a log file for the sync that just completed
   * and refresh the input & output streams.
   */
  async resetFileLog() {
    // If we're writing an error log, dump extensions that may be causing problems.
    if (this._logManager.sawError) {
      await this._dumpAddons();
    }
    const logType = await this._logManager.resetFileLog();
    if (logType == this._logManager.ERROR_LOG_WRITTEN) {
      Cu.reportError("Sync encountered an error - see about:sync-log for the log file.");
    }
    Svc.Obs.notify("weave:service:reset-file-log");
  },

  /**
   * Handle HTTP response results or exceptions and set the appropriate
   * Status.* bits.
   *
   * This method also looks for "side-channel" warnings.
   */
  checkServerError(resp) {
    // In this case we were passed a resolved value of Resource#_doRequest.
    switch (resp.status) {
      case 400:
        if (resp == RESPONSE_OVER_QUOTA) {
          Status.sync = OVER_QUOTA;
        }
        break;

      case 401:
        this.service.logout();
        this._log.info("Got 401 response; resetting clusterURL.");
        this.service.clusterURL = null;

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
        this.service.scheduler.scheduleNextSync(delay, {why: "reschedule"});
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

    // In this other case we were passed a rejection value.
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
