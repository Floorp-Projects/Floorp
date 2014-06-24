/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/PrivateBrowsingUtils.jsm");
Cu.import("resource://gre/modules/LoginManagerContent.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BrowserUtils",
                                  "resource://gre/modules/BrowserUtils.jsm");

var debug = false;
function log(...pieces) {
    function generateLogMessage(args) {
        let strings = ['Login Manager:'];

        args.forEach(function(arg) {
            if (typeof arg === 'string') {
                strings.push(arg);
            } else if (typeof arg === 'undefined') {
                strings.push('undefined');
            } else if (arg === null) {
                strings.push('null');
            } else {
                try {
                  strings.push(JSON.stringify(arg, null, 2));
                } catch(err) {
                  strings.push("<<something>>");
                }
            }
        });
        return strings.join(' ');
    }

    if (!debug)
        return;

    let message = generateLogMessage(pieces);
    dump(message + "\n");
    Services.console.logStringMessage(message);
}

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

        throw Cr.NS_ERROR_NO_INTERFACE;
    },


    /* ---------- private memebers ---------- */


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
        this._prefBranch.addObserver("", this._observer, false);

        // Get current preference values.
        debug = this._prefBranch.getBoolPref("debug");

        this._remember = this._prefBranch.getBoolPref("rememberSignons");

        // Form submit observer checks forms for new logins and pw changes.
        Services.obs.addObserver(this._observer, "xpcom-shutdown", false);

        // TODO: Make this class useful in the child process (in addition to
        // autoCompleteSearchAsync and fillForm).
        if (Services.appinfo.processType ===
            Services.appinfo.PROCESS_TYPE_DEFAULT) {
            Services.obs.addObserver(this._observer, "passwordmgr-storage-replace",
                                     false);

            // Initialize storage so that asynchronous data loading can start.
            this._initStorage();
        }
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

                if (prefName == "debug") {
                    debug = this._pwmgr._prefBranch.getBoolPref("debug");
                } else if (prefName == "rememberSignons") {
                    this._pwmgr._remember =
                        this._pwmgr._prefBranch.getBoolPref("rememberSignons");
                } else {
                    log("Oops! Pref not handled, change ignored.");
                }
            } else if (topic == "xpcom-shutdown") {
                for (let i in this._pwmgr) {
                  try {
                    this._pwmgr[i] = null;
                  } catch(ex) {}
                }
                this._pwmgr = null;
            } else if (topic == "passwordmgr-storage-replace") {
                Task.spawn(function () {
                  yield this._pwmgr._storage.terminate();
                  this._pwmgr._initStorage();
                  yield this._pwmgr.initializationPromise;
                  Services.obs.notifyObservers(null,
                               "passwordmgr-storage-replace-complete", null);
                }.bind(this));
            } else {
                log("Oops! Unexpected notification:", topic);
            }
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
            throw "Can't add a login with a null or empty hostname.";

        // For logins w/o a username, set to "", not null.
        if (login.username == null)
            throw "Can't add a login with a null username.";

        if (login.password == null || login.password.length == 0)
            throw "Can't add a login with a null or empty password.";

        if (login.formSubmitURL || login.formSubmitURL == "") {
            // We have a form submit URL. Can't have a HTTP realm.
            if (login.httpRealm != null)
                throw "Can't add a login with both a httpRealm and formSubmitURL.";
        } else if (login.httpRealm) {
            // We have a HTTP realm. Can't have a form submit URL.
            if (login.formSubmitURL != null)
                throw "Can't add a login with both a httpRealm and formSubmitURL.";
        } else {
            // Need one or the other!
            throw "Can't add a login without a httpRealm or formSubmitURL.";
        }


        // Look for an existing entry.
        var logins = this.findLogins({}, login.hostname, login.formSubmitURL,
                                     login.httpRealm);

        if (logins.some(function(l) login.matches(l, true)))
            throw "This login already exists.";

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
            throw "Invalid hostname";

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


    /* ------- Internal methods / callbacks for document integration ------- */


    /*
     * _getPasswordOrigin
     *
     * Get the parts of the URL we want for identification.
     */
    _getPasswordOrigin : function (uriString, allowJS) {
        var realm = "";
        try {
            var uri = Services.io.newURI(uriString, null, null);

            if (allowJS && uri.scheme == "javascript")
                return "javascript:"

            realm = uri.scheme + "://" + uri.host;

            // If the URI explicitly specified a port, only include it when
            // it's not the default. (We never want "http://foo.com:80")
            var port = uri.port;
            if (port != -1) {
                var handler = Services.io.getProtocolHandler(uri.scheme);
                if (port != handler.defaultPort)
                    realm += ":" + port;
            }

        } catch (e) {
            // bug 159484 - disallow url types that don't support a hostPort.
            // (although we handle "javascript:..." as a special case above.)
            log("Couldn't parse origin for", uriString);
            realm = null;
        }

        return realm;
    },

    _getActionOrigin : function (form) {
        var uriString = form.action;

        // A blank or missing action submits to where it came from.
        if (uriString == "")
            uriString = form.baseURI; // ala bug 297761

        return this._getPasswordOrigin(uriString, true);
    },


    /*
     * fillForm
     *
     * Fill the form with login information if we can find it.
     */
    fillForm : function (form) {
        log("fillForm processing form[ id:", form.id, "]");
        return LoginManagerContent._asyncFindLogins(form, { showMasterPassword: true })
                                  .then(function({ form, loginsFound }) {
                   return LoginManagerContent._fillForm(form, true, true,
                                                        false, false, loginsFound)[0];
               });
    },

}; // end of LoginManager implementation

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([LoginManager]);
