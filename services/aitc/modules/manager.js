/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["AitcManager"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Webapps.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

Cu.import("resource://services-aitc/client.js");
Cu.import("resource://services-aitc/browserid.js");
Cu.import("resource://services-aitc/storage.js");
Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-common/preferences.js");
Cu.import("resource://services-common/tokenserverclient.js");
Cu.import("resource://services-common/utils.js");

const PREFS = new Preferences("services.aitc.");
const TOKEN_TIMEOUT = 240000; // 4 minutes
const DASHBOARD_URL = PREFS.get("dashboard.url");
const MARKETPLACE_URL = PREFS.get("marketplace.url");

/**
 * The constructor for the manager takes a callback, which will be invoked when
 * the manager is ready (construction is asynchronous). *DO NOT* call any
 * methods on this object until the callback has been invoked, doing so will
 * lead to undefined behaviour.
 */
function AitcManager(cb) {
  this._client = null;
  this._getTimer = null;
  this._putTimer = null;

  this._lastToken = 0;
  this._lastEmail = null;
  this._dashboardWindow = null;

  this._log = Log4Moz.repository.getLogger("Service.AITC.Manager");
  this._log.level = Log4Moz.Level[Preferences.get("manager.log.level")];
  this._log.info("Loading AitC manager module");

  // Check if we have pending PUTs from last time.
  let self = this;
  this._pending = new AitcQueue("webapps-pending.json", function _queueDone() {
    // Inform the AitC service that we're good to go!
    self._log.info("AitC manager has finished loading");
    try {
      cb(true);
    } catch (e) {
      self._log.error(new Error("AitC manager callback threw " + e));
    }

    // Schedule them, but only if we can get a silent assertion.
    self._makeClient(function(err, client) {
      if (!err && client) {
        self._client = client;
        self._processQueue();
      }
    }, false);
  });
}
AitcManager.prototype = {
  /**
   * State of the user. ACTIVE implies user is looking at the dashboard,
   * PASSIVE means either not at the dashboard or the idle timer started.
   */
  _ACTIVE: 1,
  _PASSIVE: 2,

  /**
   * Smart setter that will only call _setPoll is the value changes.
   */
  _clientState: null,
  get _state() {
    return this._clientState;
  },
  set _state(value) {
    if (this._clientState == value) {
      return;
    }
    this._clientState = value;
    this._setPoll();
  },

  /**
   * Local app was just installed or uninstalled, ask client to PUT if user
   * is logged in.
   */
  appEvent: function appEvent(type, app) {
    // Add this to the equeue.
    let self = this;
    let obj = {type: type, app: app, retries: 0, lastTime: 0};
    this._pending.enqueue(obj, function _enqueued(err, rec) {
      if (err) {
        self._log.error("Could not add " + type + " " + app + " to queue");
        return;
      }

      // If we already have a client (i.e. user is logged in), attempt to PUT.
      if (self._client) {
        self._processQueue();
        return;
      }

      // If not, try a silent client creation.
      self._makeClient(function(err, client) {
        if (!err && client) {
          self._client = client;
          self._processQueue();
        }
        // If user is not logged in, we'll just have to try later.
      });
    });
  },

  /**
   * User is looking at dashboard. Start polling actively, but if user isn't
   * logged in, prompt for them to login via a dialog.
   */
  userActive: function userActive(win) {
    // Stash a reference to the dashboard window in case we need to prompt
    this._dashboardWindow = win;

    if (this._client) {
      this._state = this._ACTIVE;
      return;
    }

    // Make client will first try silent login, if it doesn't work, a popup
    // will be shown in the context of the dashboard. We shouldn't be
    // trying to make a client every time this function is called, there is
    // room for optimization (Bug 750607).
    let self = this;
    this._makeClient(function(err, client) {
      if (err) {
        // Notify user of error (Bug 750610).
        self._log.error("Client not created at Dashboard");
        return;
      }
      self._client = client;
      self._state = self._ACTIVE;
    }, true, win);
  },

  /**
   * User is idle, (either by idle observer, or by not being on the dashboard).
   * When the user is no longer idle and the dashboard is the current active
   * page, a call to userActive MUST be made.
   */
  userIdle: function userIdle() {
    this._state = this._PASSIVE;
    this._dashboardWindow = null;
  },

  /**
   * Poll the AITC server for any changes and process them. It is safe to call
   * this function multiple times. Last caller wins. The function will
   * grab the current user state from _state and act accordingly.
   *
   * Invalid states will cause this function to throw.
   */
  _setPoll: function _setPoll() {
    if (this._state == this._ACTIVE && !this._client) {
      throw new Error("_setPoll(ACTIVE) called without client");
    }
    if (this._state != this._ACTIVE && this._state != this._PASSIVE) {
      throw new Error("_state is invalid " + this._state);
    }

    if (!this._client) {
      // User is not logged in, we cannot do anything.
      self._log.warn("_setPoll called but user not logged in, ignoring");
      return;
    }

    // Check if there are any PUTs pending first.
    if (this._pending.length && !(this._putTimer)) {
      // There are pending PUTs and no timer, so let's process them. GETs will
      // resume after the PUTs finish (see processQueue)
      this._processQueue();
      return;
    }
    
    // Do one GET soon, but only if user is active.
    let getFreq;
    if (this._state == this._ACTIVE) {
      CommonUtils.nextTick(this._checkServer, this);
      getFreq = PREFS.get("manager.getActiveFreq");
    } else {
      getFreq = PREFS.get("manager.getPassiveFreq");
    }

    // Cancel existing timer, if any.
    if (this._getTimer) {
      this._getTimer.cancel();
      this._getTimer = null;
    }

    // Start the timer for GETs.
    let self = this;
    this._log.info("Starting GET timer");
    this._getTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._getTimer.initWithCallback({notify: this._checkServer.bind(this)},
                                    getFreq, Ci.nsITimer.TYPE_REPEATING_SLACK);

    this._log.info("GET timer set, next attempt in " + getFreq + "ms");
  },

  /**
   * Checks if the current token we hold is valid. If not, we obtain a new one
   * and execute the provided func. If a token could not be obtained, func will
   * not be called and an error will be logged.
   */
  _validateToken: function _validateToken(func) {
    if (Date.now() - this._lastToken < TOKEN_TIMEOUT) {
      func();
      return;
    }

    let win;
    if (this._state == this.ACTIVE) {
      win = this._dashboardWindow;
    }

    let self = this;
    this._refreshToken(function(err, done) {
      if (!done) {
        this._log.warn("_checkServer could not refresh token, aborting");
        return;
      }
      func();
    }, win);
  },

  /**
   * Do a GET check on the server to see if we have any new apps. Abort if
   * there are pending PUTs. If we GET some apps, send to storage for
   * further processing.
   */
  _checkServer: function _checkServer() {
    if (!this._client) {
      throw new Error("_checkServer called without a client");
    }

    if (this._pending.length) {
      this._log.warn("_checkServer aborted because of pending PUTs");
      return;
    }

    this._validateToken(this._getApps.bind(this));
  },

  _getApps: function _getApps() {
    // Do a GET
    this._log.info("Attempting to getApps");

    let self = this;
    this._client.getApps(function gotApps(err, apps) {
      if (err) {
        // Error was logged in client.
        return;
      }
      if (!apps) {
        // No changes, got 304.
        return;
      }
      if (!apps.length) {
        // Empty array, nothing to process
        self._log.info("No apps found on remote server");
        return;
      }

      // Send list of remote apps to storage to apply locally
      AitcStorage.processApps(apps, function processedApps() {
        self._log.info("processApps completed successfully, changes applied");
      });
    });
  },

  /**
   * Go through list of apps to PUT and attempt each one. If we fail, try
   * again in PUT_FREQ. Will throw if called with an empty, _reschedule()
   * makes sure that we don't.
   */
  _processQueue: function _processQueue() {
    if (!this._client) {
      throw new Error("_processQueue called without a client");
    }
    if (!this._pending.length) {
      throw new Error("_processQueue called with an empty queue");
    }

    if (this._putInProgress) {
      // The network request sent out as a result to the last call to
      // _processQueue still isn't done. A timer is created they all
      // finish to make sure this function is called again if neccessary.
      return;
    }

    this._validateToken(this._putApps.bind(this));
  },

  _putApps: function _putApps() {
    this._putInProgress = true;
    let record = this._pending.peek();

    this._log.info("Processing record type " + record.type);

    let self = this;
    function _clientCallback(err, done) {
      // Send to end of queue if unsuccessful or err.removeFromQueue is false.
      if (err && !err.removeFromQueue) {
        self._log.info("PUT failed, re-adding to queue");

        // Update retries and time
        record.retries += 1;
        record.lastTime = new Date().getTime();

        // Add updated record to the end of the queue.
        self._pending.enqueue(record, function(err, done) {
          if (err) {
            self._log.error("Enqueue failed " + err);
            _reschedule();
            return;
          }
          // If record was successfully added, remove old record.
          self._pending.dequeue(function(err, done) {
            if (err) {
              self._log.error("Dequeue failed " + err);
            }
            _reschedule();
            return;
          });
        });
      }

      // If succeeded or client told us to remove from queue
      self._log.info("_putApp asked us to remove it from queue");
      self._pending.dequeue(function(err, done) {
        if (err) {
          self._log.error("Dequeue failed " + e);
        }
        _reschedule();
      });
    }

    function _reschedule() {
      // Release PUT lock
      self._putInProgress = false;

      // We just finished PUTting an object, try the next one immediately,
      // but only if haven't tried it already in the last putFreq (ms).
      if (!self._pending.length) {
        // Start GET timer now that we're done with PUTs.
        self._setPoll();
        return;
      }

      let obj = self._pending.peek();
      let cTime = new Date().getTime();
      let freq = PREFS.get("manager.putFreq");

      // We tried this object recently, we'll come back to it later.
      if (obj.lastTime && ((cTime - obj.lastTime) < freq)) {
        self._log.info("Scheduling next processQueue in " + freq);
        CommonUtils.namedTimer(self._processQueue, freq, self, "_putTimer");
        return;
      }

      // Haven't tried this PUT yet, do it immediately.
      self._log.info("Queue non-empty, processing next PUT");
      self._processQueue();
    }

    switch (record.type) {
      case "install":
        this._client.remoteInstall(record.app, _clientCallback);
        break;
      case "uninstall":
        record.app.hidden = true;
        this._client.remoteUninstall(record.app, _clientCallback);
        break;
      default:
        this._log.warn(
          "Unrecognized type " + record.type + " in queue, removing"
        );
        let self = this;
        this._pending.dequeue(function _dequeued(err) {
          if (err) {
            self._log.error("Dequeue of unrecognized app type failed");
          }
          _reschedule();
        });
    }
  },

  /* Obtain a (new) token from the Sagrada token server. If win is is specified,
   * the user will be asked to login via UI, if required. The callback's
   * signature is cb(err, done). If a token is obtained successfully, done will
   * be true and err will be null.
   */
  _refreshToken: function _refreshToken(cb, win) {
    if (!this._client) {
      throw new Error("_refreshToken called without an active client");
    }

    this._log.info("Token refresh requested");

    let self = this;
    function refreshedAssertion(err, assertion) {
      if (!err) {
        self._getToken(assertion, function(err, token) {
          if (err) {
            cb(err, null);
            return;
          }
          self._lastToken = Date.now();
          self._client.updateToken(token);
          cb(null, true);
        });
        return;
      }

      // Silent refresh was asked for.
      if (!win) {
        cb(err, null);
        return;
      }
      
      // Prompt user to login.
      self._makeClient(function(err, client) {
        if (err) {
          cb(err, null);
          return;
        }
      
        // makeClient sets an updated token.
        self._client = client;
        cb(null, true);
      }, win);
    }

    let options = { audience: DASHBOARD_URL };
    if (this._lastEmail) {
      options.requiredEmail = this._lastEmail;
    } else {
      options.sameEmailAs = MARKETPLACE_URL;
    }
    BrowserID.getAssertion(refreshedAssertion, options);
  },

  /* Obtain a token from Sagrada token server, given a BrowserID assertion
   * cb(err, token) will be invoked on success or failure.
   */
  _getToken: function _getToken(assertion, cb) {
    let url = PREFS.get("tokenServer.url") + "/1.0/aitc/1.0";
    let client = new TokenServerClient();

    this._log.info("Obtaining token from " + url);

    let self = this;
    try {
      client.getTokenFromBrowserIDAssertion(url, assertion, function(err, tok) {
        self._gotToken(err, tok, cb);
      });
    } catch (e) {
      cb(new Error(e), null);
    }
  },

  // Token recieved from _getToken.
  _gotToken: function _gotToken(err, tok, cb) {
    if (!err) {
      this._log.info("Got token from server: " + JSON.stringify(tok));
      cb(null, tok);
      return;
    }

    let msg = err.name + " in _getToken: " + err.error;
    this._log.error(msg);
    cb(msg, null);
  },

  // Extract the email address from a BrowserID assertion.
  _extractEmail: function _extractEmail(assertion) {
    // Please look the other way while I do this. Thanks.
    let chain = assertion.split("~");
    let len = chain.length;
    if (len < 2) {
      return null;
    }

    try {
      // We need CommonUtils.decodeBase64URL.
      let cert = JSON.parse(atob(
        chain[0].split(".")[1].replace("-", "+", "g").replace("_", "/", "g")
      ));
      return cert.principal.email;
    } catch (e) {
      return null;
    }
  },

  /* To start the AitcClient we need a token, for which we need a BrowserID
   * assertion. If login is true, makeClient will ask the user to login in
   * the context of win. cb is called with (err, client).
   */
  _makeClient: function makeClient(cb, login, win) {
    if (!cb) {
      throw new Error("makeClient called without callback");
    }
    if (login && !win) {
      throw new Error("makeClient called with login as true but no win");
    }

    let self = this;
    let ctxWin = win;
    function processAssertion(val) {
      // Store the email we got the token for so we can refresh.
      self._lastEmail = self._extractEmail(val);
      self._log.info("Got assertion from BrowserID, creating token");
      self._getToken(val, function(err, token) {
        if (err) {
          cb(err, null);
          return;
        }

        // Store when we got the token so we can refresh it as needed.
        self._lastToken = Date.now();

        // We only create one client instance, store values in a pref tree
        cb(null, new AitcClient(
          token, new Preferences("services.aitc.client.")
        ));
      });
    }
    function gotSilentAssertion(err, val) {
      self._log.info("gotSilentAssertion called");
      if (err) {
        // If we were asked to let the user login, do the popup method.
        if (login) {
          self._log.info("Could not obtain silent assertion, retrying login");
          BrowserID.getAssertionWithLogin(function gotAssertion(err, val) {
            if (err) {
              self._log.error(err);
              cb(err, false);
              return;
            }
            processAssertion(val);
          }, ctxWin);
          return;
        }
        self._log.warn("Could not obtain assertion in _makeClient");
        cb(err, false);
      } else {
        processAssertion(val);
      }
    }

    // Check if we can get assertion silently first
    self._log.info("Attempting to obtain assertion silently")
    BrowserID.getAssertion(gotSilentAssertion, {
      audience: DASHBOARD_URL,
      sameEmailAs: MARKETPLACE_URL
    });
  },

};
