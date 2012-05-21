/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


const Cc = Components.classes;
const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

function LoginManager() {
    this.init();
}

LoginManager.prototype = {

    classID: Components.ID("{f9a0edde-2a8d-4bfd-a08c-3f9333213a85}"),
    QueryInterface : XPCOMUtils.generateQI([Ci.nsILoginManager,
                                            Ci.nsIObserver,
                                            Ci.nsISupportsWeakReference]),


    /* ---------- private members ---------- */


    __storage : null, // Storage component which contains the saved logins
    get _storage() {
        if (!this.__storage) {

            var contractID = "@mozilla.org/login-manager/storage/mozStorage;1";
            try {
                var catMan = Cc["@mozilla.org/categorymanager;1"].
                             getService(Ci.nsICategoryManager);
                contractID = catMan.getCategoryEntry("login-manager-storage",
                                                     "nsILoginManagerStorage");
                this.log("Found alternate nsILoginManagerStorage with " +
                         "contract ID: " + contractID);
            } catch (e) {
                this.log("No alternate nsILoginManagerStorage registered");
            }

            this.__storage = Cc[contractID].
                             createInstance(Ci.nsILoginManagerStorage);
            try {
                this.__storage.init();
            } catch (e) {
                this.log("Initialization of storage component failed: " + e);
                this.__storage = null;
            }
        }

        return this.__storage;
    },


    _nsLoginInfo : null, // Constructor for nsILoginInfo implementation
    _debug    : false, // mirrors signon.debug
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
        // Add content listener.
        var messageManager = Cc["@mozilla.org/globalmessagemanager;1"].
                             getService(Ci.nsIChromeFrameMessageManager);
        messageManager.loadFrameScript("chrome://browser/content/LoginManagerChild.js", true);
        messageManager.addMessageListener("PasswordMgr:FormSubmitted", this);
        messageManager.addMessageListener("PasswordMgr:GetPasswords", this);

        // Get constructor for nsILoginInfo
        this._nsLoginInfo = new Components.Constructor(
            "@mozilla.org/login-manager/loginInfo;1", Ci.nsILoginInfo);

        // Preferences. Add observer so we get notified of changes.
        Services.prefs.addObserver("signon.", this, false);

        // Get current preference values.
        this._debug = Services.prefs.getBoolPref("signon.debug");
        this._remember = Services.prefs.getBoolPref("signon.rememberSignons");

        // Listen for shutdown to clean up
        Services.obs.addObserver(this, "xpcom-shutdown", false);
    },

    /*
     * log
     *
     * Internal function for logging debug messages to the Error Console window
     */
    log : function (message) {
        if (!this._debug)
            return;
        dump("PasswordUtils: " + message + "\n");
        Services.console.logStringMessage("PasswordUtils: " + message);
    },

    /*
     * observe
     *
     * Implements nsIObserver for preferences and shutdown.
     */
    observe : function (subject, topic, data) {
        if (topic == "nsPref:changed") {
            this._pwmgr._debug    = Services.prefs.getBoolPref("signon.debug");
            this._pwmgr._remember = Services.prefs.getBoolPref("signon.rememberSignons");
        } else if (topic == "xpcom-shutdown") {
            // Circular reference forms when we mark an input field as managed
            // by the password manager
            this._formFillService = null;
        } else {
            this._pwmgr.log("Oops! Unexpected notification: " + topic);
        }
    },

    /*
     * receiveMessage
     *
     * Receives messages from content process.
     */
    receiveMessage: function (message) {
        // local helper function
        function getPrompter(aBrowser) {
            var prompterSvc = Cc["@mozilla.org/login-manager/prompter;1"].
                              createInstance(Ci.nsILoginManagerPrompter);
            prompterSvc.init(aBrowser);
            return prompterSvc;
        }

        switch (message.name) {
            case "PasswordMgr:GetPasswords":
                // If there are no logins for this site, bail out now.
                if (!this.countLogins(message.json.formOrigin, "", null))
                    return { foundLogins: {} };

                var foundLogins = {};

                if (!this.uiBusy) {
                    for (var i = 0; i < message.json.actionOrigins.length; i++) {
                        var actionOrigin = message.json.actionOrigins[i];
                        var logins = this.findLogins({}, message.json.formOrigin, actionOrigin, null);
                        if (logins.length) {
                            foundLogins[actionOrigin] = logins;
                        }
                    }
                }

                return {
                    uiBusy: this.uiBusy,
                    foundLogins: foundLogins
                };

            case "PasswordMgr:FormSubmitted":
                var json = message.json;
                var hostname = json.hostname;
                var formSubmitURL = json.formSubmitURL;

                if (!this.getLoginSavingEnabled(hostname)) {
                    this.log("(form submission ignored -- saving is " +
                             "disabled for: " + hostname + ")");
                    return {};
                }

                var browser = message.target;

                var formLogin = new this._nsLoginInfo();

                formLogin.init(hostname, formSubmitURL, null,
                            json.usernameValue,
                            json.passwordValue,
                            json.usernameField,
                            json.passwordField);

                // If we didn't find a username field, but seem to be changing a
                // password, allow the user to select from a list of applicable
                // logins to update the password for.
                if (!json.usernameField && json.hasOldPasswordField) {

                    var logins = this.findLogins({}, hostname, formSubmitURL, null);

                    if (logins.length == 0) {
                        // Could prompt to save this as a new password-only login.
                        // This seems uncommon, and might be wrong, so ignore.
                        this.log("(no logins for this host -- pwchange ignored)");
                        return {};
                    }

                    var prompter = getPrompter(browser);

                    if (logins.length == 1) {
                        var oldLogin = logins[0];
                        formLogin.username      = oldLogin.username;
                        formLogin.usernameField = oldLogin.usernameField;

                        prompter.promptToChangePassword(oldLogin, formLogin);
                    } else {
                        prompter.promptToChangePasswordWithUsernames(
                                            logins, logins.length, formLogin);
                    }

                } else {

                    // Look for an existing login that matches the form login.
                    var existingLogin = null;
                    var logins = this.findLogins({}, hostname, formSubmitURL, null);

                    for (var i = 0; i < logins.length; i++) {
                        var same, login = logins[i];

                        // If one login has a username but the other doesn't, ignore
                        // the username when comparing and only match if they have the
                        // same password. Otherwise, compare the logins and match even
                        // if the passwords differ.
                        if (!login.username && formLogin.username) {
                            var restoreMe = formLogin.username;
                            formLogin.username = ""; 
                            same = formLogin.matches(login, false);
                            formLogin.username = restoreMe;
                        } else if (!formLogin.username && login.username) {
                            formLogin.username = login.username;
                            same = formLogin.matches(login, false);
                            formLogin.username = ""; // we know it's always blank.
                        } else {
                            same = formLogin.matches(login, true);
                        }

                        if (same) {
                            existingLogin = login;
                            break;
                        }
                    }

                    if (existingLogin) {
                        this.log("Found an existing login matching this form submission");

                        // Change password if needed.
                        if (existingLogin.password != formLogin.password) {
                            this.log("...passwords differ, prompting to change.");
                            prompter = getPrompter(browser);
                            prompter.promptToChangePassword(existingLogin, formLogin);
                        } else {
                            // Update the lastUsed timestamp.
                            var propBag = Cc["@mozilla.org/hash-property-bag;1"].
                                          createInstance(Ci.nsIWritablePropertyBag);
                            propBag.setProperty("timeLastUsed", Date.now());
                            propBag.setProperty("timesUsedIncrement", 1);
                            this.modifyLogin(existingLogin, propBag);
                        }

                        return {};
                    }


                    // Prompt user to save login (via dialog or notification bar)
                    prompter = getPrompter(browser);
                    prompter.promptToSavePassword(formLogin);
                }
                return {};

            default:
                throw "Unexpected message " + message.name;
        }
    },


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
            this.log("Couldn't parse origin for " + uriString);
            realm = null;
        }

        return realm;
    },


    _getActionOrigin : function (form) {
        var uriString = form.action;

        // A blank or mission action submits to where it came from.
        if (uriString == "")
            uriString = form.baseURI; // ala bug 297761

        return this._getPasswordOrigin(uriString, true);
    },


    /* ---------- Primary Public interfaces ---------- */


    /*
     * fillForm
     *
     * Fill the form with login information if we can find it.
     */
    fillForm : function (form) {
        // XXX figure out what to do about fillForm
        return false;
    },


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

        this.log("Adding login: " + login);
        return this._storage.addLogin(login);
    },


    /*
     * removeLogin
     *
     * Remove the specified login from the stored logins.
     */
    removeLogin : function (login) {
        this.log("Removing login: " + login);
        return this._storage.removeLogin(login);
    },


    /*
     * modifyLogin
     *
     * Change the specified login to match the new login.
     */
    modifyLogin : function (oldLogin, newLogin) {
        this.log("Modifying oldLogin: " + oldLogin + " newLogin: " + newLogin);
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
        this.log("Getting a list of all logins");
        return this._storage.getAllLogins(count);
    },


    /*
     * removeAllLogins
     *
     * Remove all stored logins.
     */
    removeAllLogins : function () {
        this.log("Removing all logins");
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
        this.log("Getting a list of all disabled hosts");
        return this._storage.getAllDisabledHosts(count);
    },


    /*
     * findLogins
     *
     * Search for the known logins for entries matching the specified criteria.
     */
    findLogins : function (count, hostname, formSubmitURL, httpRealm) {
        this.log("Searching for logins matching host: " + hostname +
            ", formSubmitURL: " + formSubmitURL + ", httpRealm: " + httpRealm);

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
       this.log("Searching for logins");

        return this._storage.searchLogins(count, matchData);
    },


    /*
     * countLogins
     *
     * Search for the known logins for entries matching the specified criteria,
     * returns only the count.
     */
    countLogins : function (hostname, formSubmitURL, httpRealm) {
        this.log("Counting logins matching host: " + hostname +
            ", formSubmitURL: " + formSubmitURL + ", httpRealm: " + httpRealm);

        return this._storage.countLogins(hostname, formSubmitURL, httpRealm);
    },


    /*
     * uiBusy
     */
    get uiBusy() {
        return this._storage.uiBusy;
    },


    /*
     * getLoginSavingEnabled
     *
     * Check to see if user has disabled saving logins for the host.
     */
    getLoginSavingEnabled : function (host) {
        this.log("Checking if logins to " + host + " can be saved.");
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

        this.log("Saving logins for " + hostname + " enabled? " + enabled);
        return this._storage.setLoginSavingEnabled(hostname, enabled);
    },


    /*
     * autoCompleteSearch
     *
     * Yuck. This is called directly by satchel:
     * nsFormFillController::StartSearch()
     * [toolkit/components/satchel/src/nsFormFillController.cpp]
     *
     * We really ought to have a simple way for code to register an
     * auto-complete provider, and not have satchel calling pwmgr directly.
     */
    autoCompleteSearch : function (aSearchString, aPreviousResult, aElement) {
        // aPreviousResult & aResult are nsIAutoCompleteResult,
        // aElement is nsIDOMHTMLInputElement

        if (!this._remember)
            return null;

        this.log("AutoCompleteSearch invoked. Search is: " + aSearchString);

        var result = null;

        if (aPreviousResult &&
                aSearchString.substr(0, aPreviousResult.searchString.length) == aPreviousResult.searchString) {
            this.log("Using previous autocomplete result");
            result = aPreviousResult;
            result.wrappedJSObject.searchString = aSearchString;

            // We have a list of results for a shorter search string, so just
            // filter them further based on the new search string.
            // Count backwards, because result.matchCount is decremented
            // when we remove an entry.
            for (var i = result.matchCount - 1; i >= 0; i--) {
                var match = result.getValueAt(i);

                // Remove results that are too short, or have different prefix.
                if (aSearchString.length > match.length ||
                    aSearchString.toLowerCase() !=
                        match.substr(0, aSearchString.length).toLowerCase())
                {
                    this.log("Removing autocomplete entry '" + match + "'");
                    result.removeValueAt(i, false);
                }
            }
        } else {
            this.log("Creating new autocomplete search result.");

            var doc = aElement.ownerDocument;
            var origin = this._getPasswordOrigin(doc.documentURI);
            var actionOrigin = this._getActionOrigin(aElement.form);

            // This shouldn't trigger a master password prompt, because we
            // don't attach to the input until after we successfully obtain
            // logins for the form.
            var logins = this.findLogins({}, origin, actionOrigin, null);
            var matchingLogins = [];

            // Filter out logins that don't match the search prefix. Also
            // filter logins without a username, since that's confusing to see
            // in the dropdown and we can't autocomplete them anyway.
            for (i = 0; i < logins.length; i++) {
                var username = logins[i].username.toLowerCase();
                if (username &&
                    aSearchString.length <= username.length &&
                    aSearchString.toLowerCase() ==
                        username.substr(0, aSearchString.length))
                {
                    matchingLogins.push(logins[i]);
                }
            }
            this.log(matchingLogins.length + " autocomplete logins avail.");
            result = new UserAutoCompleteResult(aSearchString, matchingLogins);
        }

        return result;
    }
}; // end of LoginManager implementation




// nsIAutoCompleteResult implementation
function UserAutoCompleteResult (aSearchString, matchingLogins) {
    function loginSort(a,b) {
        var userA = a.username.toLowerCase();
        var userB = b.username.toLowerCase();

        if (userA < userB)
            return -1;

        if (userB > userA)
            return  1;

        return 0;
    };

    this.searchString = aSearchString;
    this.logins = matchingLogins.sort(loginSort);
    this.matchCount = matchingLogins.length;

    if (this.matchCount > 0) {
        this.searchResult = Ci.nsIAutoCompleteResult.RESULT_SUCCESS;
        this.defaultIndex = 0;
    }
}

UserAutoCompleteResult.prototype = {
    QueryInterface : XPCOMUtils.generateQI([Ci.nsIAutoCompleteResult,
                                            Ci.nsISupportsWeakReference]),

    // private
    logins : null,

    // Allow autoCompleteSearch to get at the JS object so it can
    // modify some readonly properties for internal use.
    get wrappedJSObject() {
        return this;
    },

    // Interfaces from idl...
    searchString : null,
    searchResult : Ci.nsIAutoCompleteResult.RESULT_NOMATCH,
    defaultIndex : -1,
    errorDescription : "",
    matchCount : 0,

    getValueAt : function (index) {
        if (index < 0 || index >= this.logins.length)
            throw "Index out of range.";

        return this.logins[index].username;
    },

    getLabelAt : function (index) {
      return this.getValueAt(index);
    },

    getCommentAt : function (index) {
        return "";
    },

    getStyleAt : function (index) {
        return "";
    },

    getImageAt : function (index) {
        return "";
    },

    removeValueAt : function (index, removeFromDB) {
        if (index < 0 || index >= this.logins.length)
            throw "Index out of range.";

        var [removedLogin] = this.logins.splice(index, 1);

        this.matchCount--;
        if (this.defaultIndex > this.logins.length)
            this.defaultIndex--;

        if (removeFromDB) {
            var pwmgr = Cc["@mozilla.org/login-manager;1"].
                        getService(Ci.nsILoginManager);
            pwmgr.removeLogin(removedLogin);
        }
    }
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([LoginManager]);
