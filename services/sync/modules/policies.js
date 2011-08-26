/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Firefox Sync.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marina Samuel <msamuel@mozilla.com>
 *  Philipp von Weitershausen <philipp@weitershausen.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


const EXPORTED_SYMBOLS = ["SyncScheduler", "ErrorHandler"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/log4moz.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/status.js");

Cu.import("resource://services-sync/main.js");    // So we can get to Service for callbacks.

let SyncScheduler = {
  _log: Log4Moz.repository.getLogger("Sync.SyncScheduler"),

  /**
   * The nsITimer object that schedules the next sync. See scheduleNextSync().
   */
  syncTimer: null,

  setDefaults: function setDefaults() {
    this._log.trace("Setting SyncScheduler policy values to defaults.");

    this.singleDeviceInterval = Svc.Prefs.get("scheduler.singleDeviceInterval") * 1000;
    this.idleInterval         = Svc.Prefs.get("scheduler.idleInterval")         * 1000;
    this.activeInterval       = Svc.Prefs.get("scheduler.activeInterval")       * 1000;
    this.immediateInterval    = Svc.Prefs.get("scheduler.immediateInterval")    * 1000;

    // A user is non-idle on startup by default.
    this.idle = false;

    this.hasIncomingItems = false;
    this.numClients = 0;

    this.nextSync = 0,
    this.syncInterval = this.singleDeviceInterval;
    this.syncThreshold = SINGLE_USER_THRESHOLD;
  },

  get globalScore() Svc.Prefs.get("globalScore", 0),
  set globalScore(value) Svc.Prefs.set("globalScore", value),

  init: function init() {
    this._log.level = Log4Moz.Level[Svc.Prefs.get("log.logger.service.main")];
    this.setDefaults();
    Svc.Obs.add("weave:engine:score:updated", this);
    Svc.Obs.add("network:offline-status-changed", this);
    Svc.Obs.add("weave:service:sync:start", this);
    Svc.Obs.add("weave:service:sync:finish", this);
    Svc.Obs.add("weave:engine:sync:finish", this);
    Svc.Obs.add("weave:service:login:error", this);
    Svc.Obs.add("weave:service:logout:finish", this);
    Svc.Obs.add("weave:service:sync:error", this);
    Svc.Obs.add("weave:service:backoff:interval", this);
    Svc.Obs.add("weave:service:ready", this);
    Svc.Obs.add("weave:engine:sync:applied", this);
    Svc.Obs.add("weave:service:setup-complete", this);
    Svc.Obs.add("weave:service:start-over", this);
				
    if (Status.checkSetup() == STATUS_OK) {
      Svc.Idle.addIdleObserver(this, Svc.Prefs.get("scheduler.idleTime"));
    }

  },

  observe: function observe(subject, topic, data) {
    switch(topic) {
      case "weave:engine:score:updated":
        Utils.namedTimer(this.calculateScore, SCORE_UPDATE_DELAY, this,
                         "_scoreTimer");
        break;
      case "network:offline-status-changed":
        // Whether online or offline, we'll reschedule syncs
        this._log.trace("Network offline status change: " + data);
        this.checkSyncStatus();
        break;
      case "weave:service:sync:start":
        // Clear out any potentially pending syncs now that we're syncing
        this.clearSyncTriggers();
        this.nextSync = 0;

        // reset backoff info, if the server tells us to continue backing off,
        // we'll handle that later
        Status.resetBackoff();

        this.globalScore = 0;
        break;
      case "weave:service:sync:finish":
        this.adjustSyncInterval();

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
      case "weave:service:login:error":
        this.clearSyncTriggers();
        
        // Try again later, just as if we threw an error... only without the
        // error count.
        if (Status.login == MASTER_PASSWORD_LOCKED) {
          this._log.debug("Couldn't log in: master password is locked.");
          this._log.trace("Scheduling a sync at MASTER_PASSWORD_LOCKED_RETRY_INTERVAL");
          this.scheduleAtInterval(MASTER_PASSWORD_LOCKED_RETRY_INTERVAL);
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
        this.handleSyncError();
        break;
      case "weave:service:backoff:interval":
        let interval = (data + Math.random() * data * 0.25) * 1000; // required backoff + up to 25%
        Status.backoffInterval = interval;
        Status.minimumNextSync = Date.now() + data;
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
        let numItems = subject.applied;
        this._log.trace("Engine " + data + " applied " + numItems + " items.");
        if (numItems) 
          this.hasIncomingItems = true;
        break;
      case "weave:service:setup-complete":
         Svc.Idle.addIdleObserver(this, Svc.Prefs.get("scheduler.idleTime"));
         break;
      case "weave:service:start-over":
         Svc.Idle.removeIdleObserver(this, Svc.Prefs.get("scheduler.idleTime"));
         SyncScheduler.setDefaults();
         break;
      case "idle":
        this._log.trace("We're idle.");
        this.idle = true;
        // Adjust the interval for future syncs. This won't actually have any
        // effect until the next pending sync (which will happen soon since we
        // were just active.)
        this.adjustSyncInterval();
        break;
      case "back":
        this._log.trace("We're no longer idle.");
        this.idle = false;
        // Trigger a sync if we have multiple clients.
        if (this.numClients > 1) {
          Utils.nextTick(Weave.Service.sync, Weave.Service);
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

  calculateScore: function calculateScore() {
    let engines = [Clients].concat(Engines.getEnabled());
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
    let {numClients} = Clients.stats;	
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
    let skip = Weave.Service._checkSync(ignore);
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

    Utils.nextTick(Weave.Service.sync, Weave.Service);
  },

  /**
   * Set a timer for the next sync
   */
  scheduleNextSync: function scheduleNextSync(interval) {
    // Figure out when to sync next if not given a interval to wait
    if (interval == null || interval == undefined) {
      // Check if we had a pending sync from last time
      if (this.nextSync != 0)
        interval = Math.min(this.syncInterval, (this.nextSync - Date.now()));
      // Use the bigger of default sync interval and backoff
      else
        interval = Math.max(this.syncInterval, Status.backoffInterval);
    }

    // Start the sync right away if we're already late
    if (interval <= 0) {
      this.syncIfMPUnlocked();
      return;
    }

    this._log.trace("Next sync in " + Math.ceil(interval / 1000) + " sec.");
    Utils.namedTimer(this.syncIfMPUnlocked, interval, this, "syncTimer");

    // Save the next sync time in-case sync is disabled (logout/offline/etc.)
    this.nextSync = Date.now() + interval;
  },


  /**
   * Incorporates the backoff/retry logic used in error handling and elective
   * non-syncing.
   */
  scheduleAtInterval: function scheduleAtInterval(minimumInterval) {
    let interval = Utils.calculateBackoff(this._syncErrors, MINIMUM_BACKOFF_INTERVAL);
    if (minimumInterval)
      interval = Math.max(minimumInterval, interval);

    let d = new Date(Date.now() + interval);
    this._log.config("Starting backoff, next sync at:" + d.toString());

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
    if (Weave.Service._checkSetup() == STATUS_OK) {
      Utils.namedTimer(this.autoConnect, delay * 1000, this, "_autoTimer");
    }
  },

  autoConnect: function autoConnect() {
    if (Weave.Service._checkSetup() == STATUS_OK && !Weave.Service._checkSync()) {
      Utils.nextTick(Weave.Service.sync, Weave.Service);
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
    this._syncErrors++;

    // Do nothing on the first couple of failures, if we're not in
    // backoff due to 5xx errors.
    if (!Status.enforceBackoff) {
      if (this._syncErrors < MAX_ERROR_COUNT_BEFORE_BACKOFF) {
        this.scheduleNextSync();
        return;
      }
      Status.enforceBackoff = true;
    }

    this.scheduleAtInterval();
  },


  /**
   * Remove any timers/observers that might trigger a sync
   */
  clearSyncTriggers: function clearSyncTriggers() {
    this._log.debug("Clearing sync triggers.");

    // Clear out any scheduled syncs
    if (this.syncTimer)
      this.syncTimer.clear();
  }

};

const LOG_PREFIX_SUCCESS = "success-";
const LOG_PREFIX_ERROR   = "error-";

let ErrorHandler = {

  init: function init() {
    Svc.Obs.add("weave:service:login:error", this);
    Svc.Obs.add("weave:service:sync:error", this);
    Svc.Obs.add("weave:service:sync:finish", this);

    this.initLogs();
  },

  initLogs: function initLogs() {
    this._log = Log4Moz.repository.getLogger("Sync.ErrorHandler");
		this._log.level = Log4Moz.Level[Svc.Prefs.get("log.logger.service.main")];

    let root = Log4Moz.repository.getLogger("Sync");
    root.level = Log4Moz.Level[Svc.Prefs.get("log.rootLogger")];

    let formatter = new Log4Moz.BasicFormatter();
    let capp = new Log4Moz.ConsoleAppender(formatter);
    capp.level = Log4Moz.Level[Svc.Prefs.get("log.appender.console")];
    root.addAppender(capp);

    let dapp = new Log4Moz.DumpAppender(formatter);
    dapp.level = Log4Moz.Level[Svc.Prefs.get("log.appender.dump")];
    root.addAppender(dapp);

    let fapp = this._logAppender = new Log4Moz.StorageStreamAppender(formatter);
    fapp.level = Log4Moz.Level[Svc.Prefs.get("log.appender.file.level")];
    root.addAppender(fapp);
  },

  observe: function observe(subject, topic, data) {
    switch(topic) {
      case "weave:service:login:error":
        if (Status.login == LOGIN_FAILED_NETWORK_ERROR &&
            !Services.io.offline) {
          this._ignorableErrorCount += 1;
        } else {
          this.resetFileLog(Svc.Prefs.get("log.appender.file.logOnError"),
                            LOG_PREFIX_ERROR);
        }
        break;
      case "weave:service:sync:error":
        switch (Status.sync) {
          case LOGIN_FAILED_NETWORK_ERROR:
            if (!Services.io.offline) {
              this._ignorableErrorCount += 1;
            }
            break;
          case CREDENTIALS_CHANGED:
            Weave.Service.logout();
            break;
          default:
            this.resetFileLog(Svc.Prefs.get("log.appender.file.logOnError"),
                              LOG_PREFIX_ERROR);
            break;
        }
        break;
      case "weave:service:sync:finish":
        this.resetFileLog(Svc.Prefs.get("log.appender.file.logOnSuccess"),
                          LOG_PREFIX_SUCCESS);
        this._ignorableErrorCount = 0;
        break;
    }
  },

  /**
   * Generate a log file for the sync that just completed
   * and refresh the input & output streams.
   * 
   * @param flushToFile
   *        the log file to be flushed/reset
   *
   * @param filenamePrefix
   *        a value of either LOG_PREFIX_SUCCESS or LOG_PREFIX_ERROR
   *        to be used as the log filename prefix
   */
  resetFileLog: function resetFileLog(flushToFile, filenamePrefix) {
    let inStream = this._logAppender.getInputStream();
    this._logAppender.reset();
    if (flushToFile && inStream) {
      try {
        let filename = filenamePrefix + Date.now() + ".txt";
        let file = FileUtils.getFile("ProfD", ["weave", "logs", filename]);
        let outStream = FileUtils.openFileOutputStream(file);
        NetUtil.asyncCopy(inStream, outStream, function () {
          Svc.Obs.notify("weave:service:reset-file-log");
        });
      } catch (ex) {
        Svc.Obs.notify("weave:service:reset-file-log");
      }
    } else {
      Svc.Obs.notify("weave:service:reset-file-log");
    }
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

  _ignorableErrorCount: 0,
  shouldIgnoreError: function shouldIgnoreError() {
    // Never show an error bar for a locked master password.
    return (Status.login == MASTER_PASSWORD_LOCKED) ||
           ([Status.login, Status.sync].indexOf(LOGIN_FAILED_NETWORK_ERROR) != -1
            && this._ignorableErrorCount < MAX_IGNORE_ERROR_COUNT);
  },

  /**
   * Handle HTTP response results or exceptions and set the appropriate
   * Status.* bits.
   */
  checkServerError: function checkServerError(resp) {
    switch (resp.status) {
      case 400:
        if (resp == RESPONSE_OVER_QUOTA) {
          Status.sync = OVER_QUOTA;
        }
        break;

      case 401:
        Weave.Service.logout();
        Status.login = LOGIN_FAILED_LOGIN_REJECTED;
        break;

      case 500:
      case 502:
      case 503:
      case 504:
        Status.enforceBackoff = true;
        if (resp.status == 503 && resp.headers["retry-after"]) {
          Svc.Obs.notify("weave:service:backoff:interval",
                         parseInt(resp.headers["retry-after"], 10));
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
        Status.sync = LOGIN_FAILED_NETWORK_ERROR;
        break;
    }
  },
};
