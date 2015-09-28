/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/LoginManagerContent.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BrowserUtils",
                                  "resource://gre/modules/BrowserUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LoginHelper",
                                  "resource://gre/modules/LoginHelper.jsm");

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let logger = LoginHelper.createLogger("nsLoginManager");
  return logger.log.bind(logger);
});

const MS_PER_DAY = 24 * 60 * 60 * 1000;

function LoginManager() {
  this.init();
}

LoginManager.prototype = {

  classID: Components.ID("{cb9e0de8-3598-4ed7-857b-827f011ad5d8}"),
  QueryInterface : XPCOMUtils.generateQI([Ci.nsILoginManager,
                                          Ci.nsISupportsWeakReference,
                                          Ci.nsIInterfaceRequestor]),
  getInterface : function(aIID) {
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


  __formFillService : null, // FormFillController, for username autocompleting
  get _formFillService() {
    if (!this.__formFillService)
      this.__formFillService =
                      Cc["@mozilla.org/satchel/form-fill-controller;1"].
                      getService(Ci.nsIFormFillController);
    return this.__formFillService;
  },


  _storage : null, // Storage component which contains the saved logins
  _prefBranch  : null, // Preferences service
  _remember : true,  // mirrors signon.rememberSignons preference


  /*
   * init
   *
   * Initialize the Login Manager. Automatically called when service
   * is created.
   *
   * Note: Service created in /browser/base/content/browser.js,
   *       delayedStartup()
   */
  init : function () {

    // Cache references to current |this| in utility objects
    this._observer._pwmgr            = this;

    // Preferences. Add observer so we get notified of changes.
    this._prefBranch = Services.prefs.getBranch("signon.");
    this._prefBranch.addObserver("rememberSignons", this._observer, false);

    this._remember = this._prefBranch.getBoolPref("rememberSignons");

    // Form submit observer checks forms for new logins and pw changes.
    Services.obs.addObserver(this._observer, "xpcom-shutdown", false);

    if (Services.appinfo.processType ===
        Services.appinfo.PROCESS_TYPE_DEFAULT) {
      Services.obs.addObserver(this._observer, "passwordmgr-storage-replace",
                               false);

      // Initialize storage so that asynchronous data loading can start.
      this._initStorage();
    }

    Services.obs.addObserver(this._observer, "gather-telemetry", false);
  },


  _initStorage : function () {
#ifdef ANDROID
    var contractID = "@mozilla.org/login-manager/storage/mozStorage;1";
#else
    var contractID = "@mozilla.org/login-manager/storage/json;1";
#endif
    try {
      var catMan = Cc["@mozilla.org/categorymanager;1"].
                   getService(Ci.nsICategoryManager);
      contractID = catMan.getCategoryEntry("login-manager-storage",
                                           "nsILoginManagerStorage");
      log("Found alternate nsILoginManagerStorage with contract ID:", contractID);
    } catch (e) {
      log("No alternate nsILoginManagerStorage registered");
    }

    this._storage = Cc[contractID].
                    createInstance(Ci.nsILoginManagerStorage);
    this.initializationPromise = this._storage.initialize();
  },


  /* ---------- Utility objects ---------- */


  /*
   * _observer object
   *
   * Internal utility object, implements the nsIObserver interface.
   * Used to receive notification for: form submission, preference changes.
   */
  _observer : {
    _pwmgr : null,

    QueryInterface : XPCOMUtils.generateQI([Ci.nsIObserver,
                                            Ci.nsISupportsWeakReference]),

    // nsObserver
    observe : function (subject, topic, data) {

      if (topic == "nsPref:changed") {
        var prefName = data;
        log("got change to", prefName, "preference");

        if (prefName == "rememberSignons") {
          this._pwmgr._remember =
              this._pwmgr._prefBranch.getBoolPref("rememberSignons");
        } else {
          log("Oops! Pref not handled, change ignored.");
        }
      } else if (topic == "xpcom-shutdown") {
        delete this._pwmgr.__formFillService;
        delete this._pwmgr._storage;
        delete this._pwmgr._prefBranch;
        this._pwmgr = null;
      } else if (topic == "passwordmgr-storage-replace") {
        Task.spawn(function () {
          yield this._pwmgr._storage.terminate();
          this._pwmgr._initStorage();
          yield this._pwmgr.initializationPromise;
          Services.obs.notifyObservers(null,
                       "passwordmgr-storage-replace-complete", null);
        }.bind(this));
      } else if (topic == "gather-telemetry") {
        // When testing, the "data" parameter is a string containing the
        // reference time in milliseconds for time-based statistics.
        this._pwmgr._gatherTelemetry(data ? parseInt(data)
                                          : new Date().getTime());
      } else {
        log("Oops! Unexpected notification:", topic);
      }
    }
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
  _gatherTelemetry : function (referenceTimeMs) {
    function clearAndGetHistogram(histogramId) {
      let histogram = Services.telemetry.getHistogramById(histogramId);
      histogram.clear();
      return histogram;
    }

    clearAndGetHistogram("PWMGR_BLOCKLIST_NUM_SITES").add(
      this.getAllDisabledHosts({}).length
    );
    clearAndGetHistogram("PWMGR_NUM_SAVED_PASSWORDS").add(
      this.countLogins("", "", "")
    );
    clearAndGetHistogram("PWMGR_NUM_HTTPAUTH_PASSWORDS").add(
      this.countLogins("", null, "")
    );

    // This is a boolean histogram, and not a flag, because we don't want to
    // record any value if _gatherTelemetry is not called.
    clearAndGetHistogram("PWMGR_SAVING_ENABLED").add(this._remember);

    // Don't try to get logins if MP is enabled, since we don't want to show a MP prompt.
    if (!this.isLoggedIn) {
      return;
    }

    let logins = this.getAllLogins({});

    let usernamePresentHistogram = clearAndGetHistogram("PWMGR_USERNAME_PRESENT");
    let loginLastUsedDaysHistogram = clearAndGetHistogram("PWMGR_LOGIN_LAST_USED_DAYS");

    let hostnameCount = new Map();
    for (let login of logins) {
      usernamePresentHistogram.add(!!login.username);

      let hostname = login.hostname;
      hostnameCount.set(hostname, (hostnameCount.get(hostname) || 0 ) + 1);

      login.QueryInterface(Ci.nsILoginMetaInfo);
      let timeLastUsedAgeMs = referenceTimeMs - login.timeLastUsed;
      if (timeLastUsedAgeMs > 0) {
        loginLastUsedDaysHistogram.add(
          Math.floor(timeLastUsedAgeMs / MS_PER_DAY)
        );
      }
    }

    let passwordsCountHistogram = clearAndGetHistogram("PWMGR_NUM_PASSWORDS_PER_HOSTNAME");
    for (let count of hostnameCount.values()) {
      passwordsCountHistogram.add(count);
    }
  },





  /* ---------- Primary Public interfaces ---------- */




  /*
   * initializationPromise
   *
   * This promise is resolved when initialization is complete, and is rejected
   * in case the asynchronous part of initialization failed.
   */
  initializationPromise : null,


  /*
   * addLogin
   *
   * Add a new login to login storage.
   */
  addLogin : function (login) {
    // Sanity check the login
    if (login.hostname == null || login.hostname.length == 0)
      throw new Error("Can't add a login with a null or empty hostname.");

    // For logins w/o a username, set to "", not null.
    if (login.username == null)
      throw new Error("Can't add a login with a null username.");

    if (login.password == null || login.password.length == 0)
      throw new Error("Can't add a login with a null or empty password.");

    if (login.formSubmitURL || login.formSubmitURL == "") {
      // We have a form submit URL. Can't have a HTTP realm.
      if (login.httpRealm != null)
        throw new Error("Can't add a login with both a httpRealm and formSubmitURL.");
    } else if (login.httpRealm) {
      // We have a HTTP realm. Can't have a form submit URL.
      if (login.formSubmitURL != null)
        throw new Error("Can't add a login with both a httpRealm and formSubmitURL.");
    } else {
      // Need one or the other!
      throw new Error("Can't add a login without a httpRealm or formSubmitURL.");
    }


    // Look for an existing entry.
    var logins = this.findLogins({}, login.hostname, login.formSubmitURL,
                                 login.httpRealm);

    if (logins.some(l => login.matches(l, true)))
      throw new Error("This login already exists.");

    log("Adding login");
    return this._storage.addLogin(login);
  },

  /*
   * removeLogin
   *
   * Remove the specified login from the stored logins.
   */
  removeLogin : function (login) {
    log("Removing login");
    return this._storage.removeLogin(login);
  },


  /*
   * modifyLogin
   *
   * Change the specified login to match the new login.
   */
  modifyLogin : function (oldLogin, newLogin) {
    log("Modifying login");
    return this._storage.modifyLogin(oldLogin, newLogin);
  },


  /*
   * getAllLogins
   *
   * Get a dump of all stored logins. Used by the login manager UI.
   *
   * |count| is only needed for XPCOM.
   *
   * Returns an array of logins. If there are no logins, the array is empty.
   */
  getAllLogins : function (count) {
    log("Getting a list of all logins");
    return this._storage.getAllLogins(count);
  },


  /*
   * removeAllLogins
   *
   * Remove all stored logins.
   */
  removeAllLogins : function () {
    log("Removing all logins");
    this._storage.removeAllLogins();
  },

  /*
   * getAllDisabledHosts
   *
   * Get a list of all hosts for which logins are disabled.
   *
   * |count| is only needed for XPCOM.
   *
   * Returns an array of disabled logins. If there are no disabled logins,
   * the array is empty.
   */
  getAllDisabledHosts : function (count) {
    log("Getting a list of all disabled hosts");
    return this._storage.getAllDisabledHosts(count);
  },


  /*
   * findLogins
   *
   * Search for the known logins for entries matching the specified criteria.
   */
  findLogins : function (count, hostname, formSubmitURL, httpRealm) {
    log("Searching for logins matching host:", hostname,
        "formSubmitURL:", formSubmitURL, "httpRealm:", httpRealm);

    return this._storage.findLogins(count, hostname, formSubmitURL,
                                    httpRealm);
  },


  /*
   * searchLogins
   *
   * Public wrapper around _searchLogins to convert the nsIPropertyBag to a
   * JavaScript object and decrypt the results.
   *
   * Returns an array of decrypted nsILoginInfo.
   */
  searchLogins : function(count, matchData) {
   log("Searching for logins");

    return this._storage.searchLogins(count, matchData);
  },


  /*
   * countLogins
   *
   * Search for the known logins for entries matching the specified criteria,
   * returns only the count.
   */
  countLogins : function (hostname, formSubmitURL, httpRealm) {
    log("Counting logins matching host:", hostname,
        "formSubmitURL:", formSubmitURL, "httpRealm:", httpRealm);

    return this._storage.countLogins(hostname, formSubmitURL, httpRealm);
  },


  /*
   * uiBusy
   */
  get uiBusy() {
    return this._storage.uiBusy;
  },


  /*
   * isLoggedIn
   */
  get isLoggedIn() {
    return this._storage.isLoggedIn;
  },


  /*
   * getLoginSavingEnabled
   *
   * Check to see if user has disabled saving logins for the host.
   */
  getLoginSavingEnabled : function (host) {
    log("Checking if logins to", host, "can be saved.");
    if (!this._remember)
      return false;

    return this._storage.getLoginSavingEnabled(host);
  },


  /*
   * setLoginSavingEnabled
   *
   * Enable or disable storing logins for the specified host.
   */
  setLoginSavingEnabled : function (hostname, enabled) {
    // Nulls won't round-trip with getAllDisabledHosts().
    if (hostname.indexOf("\0") != -1)
      throw new Error("Invalid hostname");

    log("Login saving for", hostname, "now enabled?", enabled);
    return this._storage.setLoginSavingEnabled(hostname, enabled);
  },

  /*
   * autoCompleteSearchAsync
   *
   * Yuck. This is called directly by satchel:
   * nsFormFillController::StartSearch()
   * [toolkit/components/satchel/nsFormFillController.cpp]
   *
   * We really ought to have a simple way for code to register an
   * auto-complete provider, and not have satchel calling pwmgr directly.
   */
  autoCompleteSearchAsync : function (aSearchString, aPreviousResult,
                                      aElement, aCallback) {
    // aPreviousResult is an nsIAutoCompleteResult, aElement is
    // nsIDOMHTMLInputElement

    if (!this._remember) {
      setTimeout(function() {
        aCallback.onSearchCompletion(new UserAutoCompleteResult(aSearchString, []));
      }, 0);
      return;
    }

    log("AutoCompleteSearch invoked. Search is:", aSearchString);

    var previousResult;
    if (aPreviousResult) {
      previousResult = { searchString: aPreviousResult.searchString,
                         logins: aPreviousResult.wrappedJSObject.logins };
    } else {
      previousResult = null;
    }

    let rect = BrowserUtils.getElementBoundingScreenRect(aElement);
    LoginManagerContent._autoCompleteSearchAsync(aSearchString, previousResult,
                                                 aElement, rect)
                       .then(function(logins) {
                         let results =
                             new UserAutoCompleteResult(aSearchString, logins);
                         aCallback.onSearchCompletion(results);
                       })
                       .then(null, Cu.reportError);
  },
}; // end of LoginManager implementation

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([LoginManager]);
