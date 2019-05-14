/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PERMISSION_SAVE_LOGINS = "login-saving";

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "LoginHelper",
                               "resource://gre/modules/LoginHelper.jsm");
ChromeUtils.defineModuleGetter(this, "LoginFormFactory",
                               "resource://gre/modules/LoginFormFactory.jsm");
ChromeUtils.defineModuleGetter(this, "LoginManagerContent",
                               "resource://gre/modules/LoginManagerContent.jsm");
ChromeUtils.defineModuleGetter(this, "InsecurePasswordUtils",
                               "resource://gre/modules/InsecurePasswordUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let logger = LoginHelper.createLogger("nsLoginManager");
  return logger;
});

const MS_PER_DAY = 24 * 60 * 60 * 1000;

if (Services.appinfo.processType !== Services.appinfo.PROCESS_TYPE_DEFAULT) {
  throw new Error("LoginManager.jsm should only run in the parent process");
}


function LoginManager() {
  this.init();
}

LoginManager.prototype = {

  classID: Components.ID("{cb9e0de8-3598-4ed7-857b-827f011ad5d8}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsILoginManager,
                                          Ci.nsISupportsWeakReference,
                                          Ci.nsIInterfaceRequestor]),
  getInterface(aIID) {
    if (aIID.equals(Ci.mozIStorageConnection) && this._storage) {
      let ir = this._storage.QueryInterface(Ci.nsIInterfaceRequestor);
      return ir.getInterface(aIID);
    }

    if (aIID.equals(Ci.nsIVariant)) {
      // Allows unwrapping the JavaScript object for regression tests.
      return this;
    }

    throw new Components.Exception("Interface not available", Cr.NS_ERROR_NO_INTERFACE);
  },


  /* ---------- private members ---------- */

  _storage: null, // Storage component which contains the saved logins


  /**
   * Initialize the Login Manager. Automatically called when service
   * is created.
   *
   * Note: Service created in BrowserGlue#_scheduleStartupIdleTasks()
   */
  init() {
    // Cache references to current |this| in utility objects
    this._observer._pwmgr            = this;

    Services.obs.addObserver(this._observer, "xpcom-shutdown");
    Services.obs.addObserver(this._observer, "passwordmgr-storage-replace");

    // Initialize storage so that asynchronous data loading can start.
    this._initStorage();

    Services.obs.addObserver(this._observer, "gather-telemetry");
  },

  _initStorage() {
    this._storage = Cc["@mozilla.org/login-manager/storage/default;1"]
                    .createInstance(Ci.nsILoginManagerStorage);
    this.initializationPromise = this._storage.initialize();
    this.initializationPromise.then(() => {
      log.debug("initializationPromise is resolved, updating isMasterPasswordSet in sharedData");
      Services.ppmm.sharedData.set("isMasterPasswordSet", LoginHelper.isMasterPasswordSet());
    });
  },


  /* ---------- Utility objects ---------- */


  /**
   * Internal utility object, implements the nsIObserver interface.
   * Used to receive notification for: form submission, preference changes.
   */
  _observer: {
    _pwmgr: null,

    QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver,
                                            Ci.nsISupportsWeakReference]),

    // nsIObserver
    observe(subject, topic, data) {
      if (topic == "xpcom-shutdown") {
        delete this._pwmgr._storage;
        this._pwmgr = null;
      } else if (topic == "passwordmgr-storage-replace") {
        (async () => {
          await this._pwmgr._storage.terminate();
          this._pwmgr._initStorage();
          await this._pwmgr.initializationPromise;
          Services.obs.notifyObservers(null,
                                       "passwordmgr-storage-replace-complete");
        })();
      } else if (topic == "gather-telemetry") {
        // When testing, the "data" parameter is a string containing the
        // reference time in milliseconds for time-based statistics.
        this._pwmgr._gatherTelemetry(data ? parseInt(data)
                                          : new Date().getTime());
      } else {
        log.debug("Oops! Unexpected notification:", topic);
      }
    },
  },

  /**
   * Collects statistics about the current logins and settings. The telemetry
   * histograms used here are not accumulated, but are reset each time this
   * function is called, since it can be called multiple times in a session.
   *
   * This function might also not be called at all in the current session.
   *
   * @param referenceTimeMs
   *        Current time used to calculate time-based statistics, expressed as
   *        the number of milliseconds since January 1, 1970, 00:00:00 UTC.
   *        This is set to a fake value during unit testing.
   */
  _gatherTelemetry(referenceTimeMs) {
    function clearAndGetHistogram(histogramId) {
      let histogram = Services.telemetry.getHistogramById(histogramId);
      histogram.clear();
      return histogram;
    }

    clearAndGetHistogram("PWMGR_BLOCKLIST_NUM_SITES").add(
      this.getAllDisabledHosts().length
    );
    clearAndGetHistogram("PWMGR_NUM_SAVED_PASSWORDS").add(
      this.countLogins("", "", "")
    );
    clearAndGetHistogram("PWMGR_NUM_HTTPAUTH_PASSWORDS").add(
      this.countLogins("", null, "")
    );
    Services.obs.notifyObservers(null, "weave:telemetry:histogram", "PWMGR_BLOCKLIST_NUM_SITES");
    Services.obs.notifyObservers(null, "weave:telemetry:histogram", "PWMGR_NUM_SAVED_PASSWORDS");

    // This is a boolean histogram, and not a flag, because we don't want to
    // record any value if _gatherTelemetry is not called.
    clearAndGetHistogram("PWMGR_SAVING_ENABLED").add(LoginHelper.enabled);
    Services.obs.notifyObservers(null, "weave:telemetry:histogram", "PWMGR_SAVING_ENABLED");

    // Don't try to get logins if MP is enabled, since we don't want to show a MP prompt.
    if (!this.isLoggedIn) {
      return;
    }

    let logins = this.getAllLogins();

    let usernamePresentHistogram = clearAndGetHistogram("PWMGR_USERNAME_PRESENT");
    let loginLastUsedDaysHistogram = clearAndGetHistogram("PWMGR_LOGIN_LAST_USED_DAYS");

    let hostnameCount = new Map();
    for (let login of logins) {
      usernamePresentHistogram.add(!!login.username);

      let hostname = login.hostname;
      hostnameCount.set(hostname, (hostnameCount.get(hostname) || 0) + 1);

      login.QueryInterface(Ci.nsILoginMetaInfo);
      let timeLastUsedAgeMs = referenceTimeMs - login.timeLastUsed;
      if (timeLastUsedAgeMs > 0) {
        loginLastUsedDaysHistogram.add(
          Math.floor(timeLastUsedAgeMs / MS_PER_DAY)
        );
      }
    }
    Services.obs.notifyObservers(null, "weave:telemetry:histogram", "PWMGR_LOGIN_LAST_USED_DAYS");

    let passwordsCountHistogram = clearAndGetHistogram("PWMGR_NUM_PASSWORDS_PER_HOSTNAME");
    for (let count of hostnameCount.values()) {
      passwordsCountHistogram.add(count);
    }
    Services.obs.notifyObservers(null, "weave:telemetry:histogram", "PWMGR_NUM_PASSWORDS_PER_HOSTNAME");
  },


  /**
   * Ensures that a login isn't missing any necessary fields.
   *
   * @param login
   *        The login to check.
   */
  _checkLogin(login) {
    // Sanity check the login
    if (login.hostname == null || login.hostname.length == 0) {
      throw new Error("Can't add a login with a null or empty hostname.");
    }

    // For logins w/o a username, set to "", not null.
    if (login.username == null) {
      throw new Error("Can't add a login with a null username.");
    }

    if (login.password == null || login.password.length == 0) {
      throw new Error("Can't add a login with a null or empty password.");
    }

    if (login.formSubmitURL || login.formSubmitURL == "") {
      // We have a form submit URL. Can't have a HTTP realm.
      if (login.httpRealm != null) {
        throw new Error("Can't add a login with both a httpRealm and formSubmitURL.");
      }
    } else if (login.httpRealm) {
      // We have a HTTP realm. Can't have a form submit URL.
      if (login.formSubmitURL != null) {
        throw new Error("Can't add a login with both a httpRealm and formSubmitURL.");
      }
    } else {
      // Need one or the other!
      throw new Error("Can't add a login without a httpRealm or formSubmitURL.");
    }
  },




  /* ---------- Primary Public interfaces ---------- */




  /**
   * @type Promise
   * This promise is resolved when initialization is complete, and is rejected
   * in case the asynchronous part of initialization failed.
   */
  initializationPromise: null,


  /**
   * Add a new login to login storage.
   */
  addLogin(login) {
    this._checkLogin(login);

    // Look for an existing entry.
    var logins = this.findLogins(login.hostname, login.formSubmitURL,
                                 login.httpRealm);

    if (logins.some(l => login.matches(l, true))) {
      throw new Error("This login already exists.");
    }

    log.debug("Adding login");
    return this._storage.addLogin(login);
  },

  async addLogins(logins) {
    let crypto = Cc["@mozilla.org/login-manager/crypto/SDR;1"].
                 getService(Ci.nsILoginManagerCrypto);
    let plaintexts = logins.map(l => l.username).concat(logins.map(l => l.password));
    let ciphertexts = await crypto.encryptMany(plaintexts);
    let usernames = ciphertexts.slice(0, logins.length);
    let passwords = ciphertexts.slice(logins.length);
    let resultLogins = [];
    for (let i = 0; i < logins.length; i++) {
      try {
        this._checkLogin(logins[i]);
      } catch (e) {
        Cu.reportError(e);
        continue;
      }

      let plaintextUsername = logins[i].username;
      let plaintextPassword = logins[i].password;
      logins[i].username = usernames[i];
      logins[i].password = passwords[i];
      log.debug("Adding login");
      let resultLogin = this._storage.addLogin(logins[i], true);
      // Reset the username and password to keep the same guarantees as addLogin
      logins[i].username = plaintextUsername;
      logins[i].password = plaintextPassword;

      resultLogin.username = plaintextUsername;
      resultLogin.password = plaintextPassword;
      resultLogins.push(resultLogin);
    }
    return resultLogins;
  },

  /**
   * Remove the specified login from the stored logins.
   */
  removeLogin(login) {
    log.debug("Removing login");
    return this._storage.removeLogin(login);
  },


  /**
   * Change the specified login to match the new login.
   */
  modifyLogin(oldLogin, newLogin) {
    log.debug("Modifying login");
    return this._storage.modifyLogin(oldLogin, newLogin);
  },


  /**
   * Get a dump of all stored logins. Used by the login manager UI.
   *
   * @return {nsILoginInfo[]} - If there are no logins, the array is empty.
   */
  getAllLogins() {
    log.debug("Getting a list of all logins");
    return this._storage.getAllLogins();
  },


  /**
   * Remove all stored logins.
   */
  removeAllLogins() {
    log.debug("Removing all logins");
    this._storage.removeAllLogins();
  },

  /**
   * Get a list of all origins for which logins are disabled.
   *
   * @param {Number} count - only needed for XPCOM.
   *
   * @return {String[]} of disabled origins. If there are no disabled origins,
   *                    the array is empty.
   */
  getAllDisabledHosts() {
    log.debug("Getting a list of all disabled origins");

    let disabledHosts = [];
    for (let perm of Services.perms.enumerator) {
      if (perm.type == PERMISSION_SAVE_LOGINS && perm.capability == Services.perms.DENY_ACTION) {
        disabledHosts.push(perm.principal.URI.displayPrePath);
      }
    }

    log.debug("getAllDisabledHosts: returning", disabledHosts.length, "disabled hosts.");
    return disabledHosts;
  },


  /**
   * Search for the known logins for entries matching the specified criteria.
   */
  findLogins(origin, formActionOrigin, httpRealm) {
    log.debug("Searching for logins matching origin:", origin,
              "formActionOrigin:", formActionOrigin, "httpRealm:", httpRealm);

    return this._storage.findLogins(origin, formActionOrigin, httpRealm);
  },


  /**
   * Public wrapper around _searchLogins to convert the nsIPropertyBag to a
   * JavaScript object and decrypt the results.
   *
   * @return {nsILoginInfo[]} which are decrypted.
   */
  searchLogins(matchData) {
    log.debug("Searching for logins");

    matchData.QueryInterface(Ci.nsIPropertyBag2);
    if (!matchData.hasKey("guid")) {
      if (!matchData.hasKey("hostname")) {
        log.warn("searchLogins: A `hostname` is recommended");
      }

      if (!matchData.hasKey("formSubmitURL") && !matchData.hasKey("httpRealm")) {
        log.warn("searchLogins: `formSubmitURL` or `httpRealm` is recommended");
      }
    }

    return this._storage.searchLogins(matchData);
  },


  /**
   * Search for the known logins for entries matching the specified criteria,
   * returns only the count.
   */
  countLogins(origin, formActionOrigin, httpRealm) {
    log.debug("Counting logins matching origin:", origin,
              "formActionOrigin:", formActionOrigin, "httpRealm:", httpRealm);

    return this._storage.countLogins(origin, formActionOrigin, httpRealm);
  },


  get uiBusy() {
    return this._storage.uiBusy;
  },


  get isLoggedIn() {
    return this._storage.isLoggedIn;
  },


  /**
   * Check to see if user has disabled saving logins for the origin.
   */
  getLoginSavingEnabled(origin) {
    log.debug("Checking if logins to", origin, "can be saved.");
    if (!LoginHelper.enabled) {
      return false;
    }

    let uri = Services.io.newURI(origin);
    return Services.perms.testPermission(uri, PERMISSION_SAVE_LOGINS) != Services.perms.DENY_ACTION;
  },


  /**
   * Enable or disable storing logins for the specified origin.
   */
  setLoginSavingEnabled(origin, enabled) {
    // Throws if there are bogus values.
    LoginHelper.checkHostnameValue(origin);

    let uri = Services.io.newURI(origin);
    if (enabled) {
      Services.perms.remove(uri, PERMISSION_SAVE_LOGINS);
    } else {
      Services.perms.add(uri, PERMISSION_SAVE_LOGINS, Services.perms.DENY_ACTION);
    }

    log.debug("Login saving for", origin, "now enabled?", enabled);
    LoginHelper.notifyStorageChanged(enabled ? "hostSavingEnabled" : "hostSavingDisabled", origin);
  },
}; // end of LoginManager implementation

var EXPORTED_SYMBOLS = ["LoginManager"];
