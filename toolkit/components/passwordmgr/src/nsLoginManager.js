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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Justin Dolske <dolske@mozilla.com> (original author)
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


const Cc = Components.classes;
const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function LoginManager() {
    this.init();
}

LoginManager.prototype = {

    classDescription: "LoginManager",
    contractID: "@mozilla.org/login-manager;1",
    classID: Components.ID("{cb9e0de8-3598-4ed7-857b-827f011ad5d8}"),
    QueryInterface : XPCOMUtils.generateQI([Ci.nsILoginManager,
                                            Ci.nsISupportsWeakReference]),


    /* ---------- private memebers ---------- */


    __logService : null, // Console logging service, used for debugging.
    get _logService() {
        if (!this.__logService)
            this.__logService = Cc["@mozilla.org/consoleservice;1"]
                                    .getService(Ci.nsIConsoleService);
        return this.__logService;
    },


    __promptService : null, // Prompt service for user interaction
    get _promptService() {
        if (!this.__promptService)
            this.__promptService = Cc["@mozilla.org/embedcomp/prompt-service;1"]
                                        .getService(Ci.nsIPromptService2);
        return this.__promptService;
    },


    __ioService: null, // IO service for string -> nsIURI conversion
    get _ioService() {
        if (!this.__ioService)
            this.__ioService = Cc["@mozilla.org/network/io-service;1"]
                                    .getService(Ci.nsIIOService);
        return this.__ioService;
    },


    __formFillService : null, // FormFillController, for username autocompleting
    get _formFillService() {
        if (!this.__formFillService)
            this.__formFillService = Cc[
                                "@mozilla.org/satchel/form-fill-controller;1"]
                                    .getService(Ci.nsIFormFillController);
        return this.__formFillService;
    },


    __strBundle : null, // String bundle for L10N
    get _strBundle() {
        if (!this.__strBundle) {
            var bunService = Cc["@mozilla.org/intl/stringbundle;1"]
                                    .getService(Ci.nsIStringBundleService);
            this.__strBundle = bunService.createBundle(
                        "chrome://passwordmgr/locale/passwordmgr.properties");
            if (!this.__strBundle)
                throw "String bundle for Login Manager not present!";
        }

        return this.__strBundle;
    },


    __brandBundle : null, // String bundle for L10N
    get _brandBundle() {
        if (!this.__brandBundle) {
            var bunService = Cc["@mozilla.org/intl/stringbundle;1"]
                                    .getService(Ci.nsIStringBundleService);
            this.__brandBundle = bunService.createBundle(
                        "chrome://branding/locale/brand.properties");
            if (!this.__brandBundle)
                throw "Branding string bundle not present!";
        }

        return this.__brandBundle;
    },


    __storage : null, // Storage component which contains the saved logins
    get _storage() {
        if (!this.__storage) {
            this.__storage = Cc["@mozilla.org/login-manager/storage/legacy;1"]
                                .createInstance(Ci.nsILoginManagerStorage);
            try {
                this.__storage.init();
            } catch (e) {
                this.log("Initialization of storage component failed: " + e);
                this.__storage = null;
            }
        }

        return this.__storage;
    },

    _prefBranch : null, // Preferences service
    _nsLoginInfo : null, // Constructor for nsILoginInfo implementation

    _remember : true,  // mirrors signon.rememberSignons preference
    _debug    : false, // mirrors signon.debug


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
        this._webProgressListener._domEventListener = this._domEventListener;
        this._webProgressListener._pwmgr = this;
        this._domEventListener._pwmgr    = this;
        this._observer._pwmgr            = this;

        // Preferences. Add observer so we get notified of changes.
        this._prefBranch = Cc["@mozilla.org/preferences-service;1"]
            .getService(Ci.nsIPrefService).getBranch("signon.");
        this._prefBranch.QueryInterface(Ci.nsIPrefBranch2);
        this._prefBranch.addObserver("", this._observer, false);

        // Get current preference values.
        this._debug = this._prefBranch.getBoolPref("debug");

        this._remember = this._prefBranch.getBoolPref("rememberSignons");


        // Get constructor for nsILoginInfo
        this._nsLoginInfo = new Components.Constructor(
            "@mozilla.org/login-manager/loginInfo;1", Ci.nsILoginInfo);


        // Form submit observer checks forms for new logins and pw changes.
        var observerService = Cc["@mozilla.org/observer-service;1"]
                                .getService(Ci.nsIObserverService);
        observerService.addObserver(this._observer, "earlyformsubmit", false);

        // WebProgressListener for getting notification of new doc loads.
        var progress = Cc["@mozilla.org/docloaderservice;1"]
                        .getService(Ci.nsIWebProgress);
        progress.addProgressListener(this._webProgressListener,
                                     Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT);


    },


    /*
     * log
     *
     * Internal function for logging debug messages to the Error Console window
     */
    log : function (message) {
        if (!this._debug)
            return;
        dump("Login Manager: " + message + "\n");
        this._logService.logStringMessage("Login Manager: " + message);
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
                                                Ci.nsIFormSubmitObserver,
                                                Ci.nsISupportsWeakReference]),


        // nsFormSubmitObserver
        notify : function (formElement, aWindow, actionURI) {
            this._pwmgr.log("observer notified for form submission.");

            // We're invoked before the content's |onsubmit| handlers, so we
            // can grab form data before it might be modified (see bug 257781).

            try {
                this._pwmgr._onFormSubmit(formElement);
            } catch (e) {
                this._pwmgr.log("Caught error in onFormSubmit: " + e);
            }

            return true; // Always return true, or form submt will be canceled.
        },

        // nsObserver
        observe : function (subject, topic, data) {

            if (topic == "nsPref:changed") {
                var prefName = data;
                this._pwmgr.log("got change to " + prefName + " preference");

                if (prefName == "debug") {
                    this._pwmgr._debug = 
                        this._pwmgr._prefBranch.getBoolPref("debug");
                } else if (prefName == "rememberSignons") {
                    this._pwmgr._remember =
                        this._pwmgr._prefBranch.getBoolPref("rememberSignons");
                } else {
                    this._pwmgr.log("Oops! Pref not handled, change ignored.");
                }
            } else {
                this._pwmgr.log("Oops! Unexpected notification: " + topic);
            }
        }
    },


    /*
     * _webProgressListener object
     *
     * Internal utility object, implements nsIWebProgressListener interface.
     * This is attached to the document loader service, so we get
     * notifications about all page loads.
     */
    _webProgressListener : {
        _pwmgr : null,
        _domEventListener : null,

        QueryInterface : XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                                Ci.nsISupportsWeakReference]),


        onStateChange : function (aWebProgress, aRequest,
                                  aStateFlags,  aStatus) {

            // STATE_START is too early, doc is still the old page.
            if (!(aStateFlags & Ci.nsIWebProgressListener.STATE_TRANSFERRING))
                return 0;

            if (!this._pwmgr._remember)
                return 0;

            var domWin = aWebProgress.DOMWindow;
            var domDoc = domWin.document;

            // Only process things which might have HTML forms.
            if (! domDoc instanceof Ci.nsIDOMHTMLDocument)
                return 0;

            this._pwmgr.log("onStateChange accepted: req = " + (aRequest ?
                        aRequest.name : "(null)") + ", flags = " + aStateFlags);

            // fastback navigation... We won't get a DOMContentLoaded
            // event again, so process any forms now.
            if (aStateFlags & Ci.nsIWebProgressListener.STATE_RESTORING) {
                this._pwmgr.log("onStateChange: restoring document");
                return this._pwmgr._fillDocument(domDoc);
            }

            // Add event listener to process page when DOM is complete.
            this._pwmgr.log("onStateChange: adding dom listeners");
            domDoc.addEventListener("DOMContentLoaded",
                                    this._domEventListener, false);
            return 0;
        },

        // stubs for the nsIWebProgressListener interfaces which we don't use.
        onProgressChange : function() { throw "Unexpected onProgressChange"; },
        onLocationChange : function() { throw "Unexpected onLocationChange"; },
        onStatusChange   : function() { throw "Unexpected onStatusChange";   },
        onSecurityChange : function() { throw "Unexpected onSecurityChange"; }
    },


    /*
     * _domEventListener object
     *
     * Internal utility object, implements nsIDOMEventListener
     * Used to catch certain DOM events needed to properly implement form fill.
     */
    _domEventListener : {
        _pwmgr : null,

        QueryInterface : XPCOMUtils.generateQI([Ci.nsIDOMEventListener,
                                                Ci.nsISupportsWeakReference]),


        handleEvent : function (event) {
            this._pwmgr.log("domEventListener: got event " + event.type);

            var doc, inputElement;
            switch (event.type) {
                case "DOMContentLoaded":
                    doc = event.target;
                    return this._pwmgr._fillDocument(doc);

                case "DOMAutoComplete":
                case "blur":
                    inputElement = event.target;
                    return this._pwmgr._fillPassword(inputElement);

                default:
                    this._pwmgr.log("Oops! This event unexpected.");
                    return 0;
            }
        }
    },




    /* ---------- Primary Public interfaces ---------- */




    /*
     * addLogin
     *
     * Add a new login to login storage.
     */
    addLogin : function (login) {
        // Sanity check the login
        if (login.hostname == null || login.hostname.length == 0)
            throw "Can't add a login with a null or empty hostname.";

        if (login.username == null || login.username.length == 0)
            throw "Can't add a login with a null or empty username.";

        if (login.password == null || login.password.length == 0)
            throw "Can't add a login with a null or empty password.";

        if (!login.httpRealm && !login.formSubmitURL)
            throw "Can't add a login without a httpRealm or formSubmitURL.";

        // Look for an existing entry.
        var logins = this.findLogins({}, login.hostname, login.formSubmitURL,
                                     login.httpRealm);

        if (logins.some(function(l) { return login.username == l.username }))
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
        var logins = this._storage.getAllLogins({});
        count.value = logins.length;
        return logins;
    },


    /*
     * clearAllLogins
     *
     * Clears all stored logins.
     */
    clearAllLogins : function () {
        this.log("Clearing all logins");
        this._storage.clearAllLogins();
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
        var hosts = this._storage.getAllDisabledHosts({});
        count.value = hosts.length;
        return hosts;
    },


    /*
     * findLogins
     *
     * Search the known logins for entries matching the specified criteria
     * for a protocol login (eg HTTP Auth).
     */
    findLogins : function (count, hostname, formSubmitURL, httpRealm) {
        this.log("Searching for logins matching host: " + hostname +
            ", formSubmitURL: " + formSubmitURL + ", httpRealm: " + httpRealm);

        var logins = this._storage.findLogins({}, hostname, formSubmitURL,
                                              httpRealm);
        count.value = logins.length;
        return logins;
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
            return false;

        this.log("AutoCompleteSearch invoked. Search is: " + aSearchString);

        var result = null;

        if (aPreviousResult) {
            this.log("Using previous autocomplete result");
            result = aPreviousResult;

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
            // XXX The C++ code took care to avoid reentrancy if a
            // master-password dialog was triggered here, but since
            // we're decrypting at load time that can't happen right now.
            this.log("Creating new autocomplete search result.");

            var doc = aElement.ownerDocument;
            var origin = this._getPasswordOrigin(doc.documentURI);
            var actionOrigin = this._getActionOrigin(aElement.form);

            var logins = this.findLogins({}, origin, actionOrigin, null);
            var matchingLogins = [];

            for (i = 0; i < logins.length; i++) {
                var username = logins[i].username.toLowerCase();
                if (aSearchString.length <= username.length &&
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
    },




    /* ------- Internal methods / callbacks for document integration ------- */




    /*
     * _getPasswordFields
     */
    _getPasswordFields : function (form, skipEmptyFields) {
        // Locate the password fields in the form.
        var pwFields = [];
        for (var i = 0; i < form.elements.length; i++) {
            if (form.elements[i].type != "password")
                continue;

            if (skipEmptyFields && !form.elements[i].value)
                continue;

            pwFields[pwFields.length] = {
                                            index   : i,
                                            element : form.elements[i]
                                        };
        }

        // If too few or too many fields, bail out.
        if (pwFields.length == 0 || pwFields.length > 3)
            return null;

        return pwFields;
    },


    /*
     * _onFormSubmit
     *
     * Called by the our observer when notified of a form submission.
     * [Note that this happens before any DOM onsubmit handlers are invoked.]
     * Looks for a password change in the submitted form, so we can update
     * our stored password.
     *
     * XXX update actionURL of existing login, even if pw not being changed?
     */
    _onFormSubmit : function (form) {

        // local helper function
        function autocompleteDisabled(element) {
            if (element && element.hasAttribute("autocomplete") &&
                element.getAttribute("autocomplete").toLowerCase() == "off")
                return true;

           return false;
        };

        // local helper function
        function getUsernameField (passwordFieldIndex) {
            var usernameField = null;

            // Search backwards to first text field (assume it's the username)
            for (var i = passwordFieldIndex - 1; i >= 0; i--) {
                if (form.elements[i].type == "text") {
                    usernameField = form.elements[i];
                    break;
                }
            }

            return usernameField;
        };

        // local helper function
        function findExistingLogin(pwmgr) {
            var searchLogin = new pwmgr._nsLoginInfo();
            searchLogin.init(hostname, formSubmitURL, null,
                             usernameField.value, "",
                             usernameField.name,  "");

            // TODO: random note: devmo has conflicting info on Array.some()...
            // One page says it runs on *every* element, another
            // says it stops at the first match.

            var logins = pwmgr.findLogins({}, hostname, formSubmitURL, null);
            var existingLogin;
            var found = logins.some(function(l) {
                                    existingLogin = l;
                                    return searchLogin.equalsIgnorePassword(l);
                                });

            return (found ? existingLogin : null);
        }




        var doc = form.ownerDocument;
        var win = doc.window;

        // If password saving is disabled (globally or for host), bail out now.
        if (!this._remember)
            return;

        var hostname      = this._getPasswordOrigin(doc.documentURI);
        var formSubmitURL = this._getActionOrigin(form)
        if (!this.getLoginSavingEnabled(hostname)) {
            this.log("(form submission ignored -- saving is " +
                     "disabled for: " + hostname + ")");
            return;
        }

        // Locate the password field(s) in the form. Up to 3 supported.
        var pwFields = this._getPasswordFields(form, true);
        if (!pwFields) {
            this.log("(form submission ignored -- found " + pwFields.length +
                     " fields, can only handle 1-3.)");
            return;
        }

        // Locate the username field in the form. We might not find a
        // username field if the user is already logged in to the site. 
        var usernameField = getUsernameField(pwFields[0].index);
        if (!usernameField) {
            this.log("(form submission -- no username field found)");
        }


        // Check for autocomplete=off attribute. We don't use it to prevent
        // autofilling (for existing logins), but won't save logins when it's
        // present.
        if (autocompleteDisabled(form) ||
            autocompleteDisabled(usernameField) ||
            autocompleteDisabled(pwFields[0].element) ||
            (pwFields[1] && autocompleteDisabled(pwFields[1].element)) ||
            (pwFields[2] && autocompleteDisabled(pwFields[2].element))) {
                this.log("(form submission ignored -- autocomplete=off found)");
                return;
        }


        // Try to figure out WTF is in the form based on the password values
        var pw1 = pwFields[0].element.value;
        var pw2 = (pwFields[1] ? pwFields[1].element.value : null);
        var pw3 = (pwFields[2] ? pwFields[2].element.value : null);
        // By default, assume a 1-password case
        var oldPasswordField = null, newPasswordField = pwFields[0].element;

        if (pwFields.length == 3) {
            // Look for two identical passwords, that's the new password

            if (pw1 == pw2 && pw2 == pw3) {
                // All 3 passwords the same? Weird! Treat as if 1 pw field.
                pwFields.length = 1;
            } else if (pw1 == pw2) {
                newPasswordField = pwFields[0].element;
                oldPasswordField = pwFields[2].element;
            } else if (pw2 == pw3) {
                oldPasswordField = pwFields[0].element;
                newPasswordField = pwFields[2].element;
            } else  if (pw1 == pw3) {
                // A bit odd, but could make sense with the right page layout.
                newPasswordField = pwFields[0].element;
                oldPasswordField = pwFields[1].element;
            } else {
                // We can't tell which of the 3 passwords should be saved.
                this.log("(form submission ignored -- all 3 pw fields differ)");
                return;
            }
        } else if (pwFields.length == 2) {
            if (pw1 == pw2) {
                // Treat as if 1 pw field
                pwFields.length = 1;
            } else {
                // Just assume that the 2nd password is the new password
                oldPasswordField = pwFields[0].element;
                newPasswordField = pwFields[1].element;
            }
        }


        var formLogin = new this._nsLoginInfo();
        formLogin.init(hostname, formSubmitURL, null,
                    (usernameField ? usernameField.value : null),
                    newPasswordField.value,
                    (usernameField ? usernameField.name  : null),
                    newPasswordField.name);

        // If we didn't find a username field, allow the user to select
        // from a list of applicable logins to update.
        if (!usernameField) {
            // Without an oldPasswordField, we can't know for sure if
            // the user is creating an account or changing a password.
            if (!oldPasswordField) {
                this.log("(form submission ignored -- couldn't find a " +
                         "username, and no oldPasswordField found.");
                return;
            }

            var ok, username;
            var logins = pwmgr.findLogins({}, hostname, formSubmitURL, null);

            // XXX we could be smarter here: look for a login matching the
            // old password value. If there's only one, update it. If there's
            // more than one we could filter the list (but, edge case: the
            // login for the pwchange is in pwmgr, but with an outdated
            // password. and the user has another login, with the same
            // password as the form login's old password.) ugh.

            if (logins.length == 0) {
                this.log("(no logins for this host -- pwchange ignored)");
                return;
            } else if (logins.length == 1) {
                username = logins[0].username;
                ok = _promptToChangePassword(win, username)
            } else {
                var usernames = [];
                logins.forEach(function(l) { usernames.push(l.username); });

                [ok, username] = _promptToChangePasswordWithUsername(
                                                                win, usernames);
            }

            if (!ok)
                return;

            // Now that we know the desired username, find that login and
            // update the info in our formLogin representation.
            this.log("Updating password for username " + username);

            var existingLogin;
            logins.some(function(l) {
                                    existingLogin = l;
                                    return (l.username == username);
                                });

            formLogin.username      = username;
            formLogin.usernameField = existingLogin.usernameField;

            this.modifyLogin(existingLogin, formLogin);
            
            return;
        }


        // Look for an existing login that matches the form data (other
        // than the password, which might be different)
        existingLogin = findExistingLogin(this);
        if (existingLogin) {
            // Change password if needed
            this.log("Updating password for existing login.");
            if (existingLogin.password != newPasswordField.value)
                this.modifyLogin(existingLogin, formLogin);

            return;
        }


        // Prompt user to save a new login.
        var userChoice = this._promptToSaveLogin(win);

        if (userChoice == 2) {
            this.log("Disabling " + hostname + " logins by user request.");
            this.setLoginSavingEnabled(hostname, false);
        } else if (userChoice == 0) {
            this.log("Saving login for " + hostname);
            this.addLogin(formLogin);
        } else {
            // userChoice == 1 --> just ignore the login.
            this.log("Ignoring login.");
        }
    },


    /*
     * _getPasswordOrigin
     *
     * Get the parts of the URL we want for identification.
     */
    _getPasswordOrigin : function (uriString) {
        var realm = "";
        try {
            var uri = this._ioService.newURI(uriString, null, null);

            realm += uri.scheme;
            realm += "://";
            realm += uri.hostPort;
        } catch (e) {
            // bug 159484 - disallow url types that don't support a hostPort.
            // (set null to cause throw in the JS above)
            realm = null;
        }

        return realm;
    },

    _getActionOrigin : function (form) {
        var uriString = form.action;

        // A blank or mission action submits to where it came from.
        if (uriString == "")
            uriString = form.baseURI; // ala bug 297761

        return this._getPasswordOrigin(uriString);
    },


    /*
     * _fillDocument
     *
     * Called when a page has loaded. Checks the document for forms,
     * and for each form checks to see if it can be filled out with a
     * stored login.
     */
    _fillDocument : function (doc)  {

        function getElementByName (elements, name) {
            for (var i = 0; i < elements.length; i++) {
                var element = elements[i];
                if (element.name && element.name == name)
                    return element;
            }
            return null;
        };

        /*
         * Bug 380222...
         *
         * CRIKEY! The original C++ code eeds a cleanup. This function is
         * just a straight conversion to JS, with some changes for interacting
         * with other parts of pwmgr that have been updated.
         */

        var forms = doc.forms;
        if (!forms || forms.length == 0)
            return; 

        var formOrigin = this._getPasswordOrigin(doc.documentURI);
        var prefillForm = this._prefBranch.getBoolPref("autofillForms");

        this.log("fillDocument found " + forms.length +
                 " forms on " + doc.documentURI);

        // We can auto-prefill the username and password if there is only
        // one stored login that matches the username and password field names
        // on the form in question.  Note that we only need to worry about a
        // single login per form.

        for (var i = 0; i < forms.length; i++) {
            var form = forms[i];

            var firstMatch = null;
            var attachedToInput = false;
            var prefilledUser = false;
            var userField, passField;
            // Note: C++ code only had |temp| and reused it.
            var tempUserField = null, tempPassField = null;


            var actionOrigin = this._getActionOrigin(form);

            var logins = this.findLogins({}, formOrigin, actionOrigin, null);

            this.log("form[" + i + "]: found " + logins.length +
                     " matching logins.");

            for (var j = 0; j < logins.length; j++) {
                var login = logins[j];

                if (login.usernameField != "") {
                    var foundNode = getElementByName(form.elements,
                                                     login.usernameField);
                    var tempUserField = foundNode;
                }

                var oldUserValue;
                var userFieldFound = false;

                if (tempUserField) {
                    if (tempUserField.type != "text")
                        continue;

                    oldUserValue = tempUserField.value;
                    userField = tempUserField;
                    userFieldFound = true;
                } else if (login.passwordField == "") {
                    // Happens sometimes when we import passwords from IE since
                    // their form name match is case insensitive. In this case,
                    // we'll just have to do a case insensitive search for the
                    // userField and hope we get something.

                    // loop over each form element
                    for (var k = 0; i < form.elements.length; k++) {
                        // |inputField| is |formControl| in C++
                        var inputField = form.elements[k];

                        if (inputField.type != "text")
                            continue;

                        if (login.usernameField.toLowerCase() ==
                            inputField.name.toLowerCase())
                        {
                            oldUserValue = inputField.value;
                            userField = inputField;
                            foundNode = inputField;
                            login.usernameField = inputField.name;
                            //TODO write modified login to disk
                            userFieldFound = true;
                            break;
                        }
                    }
                }

                // Bail out if we should be seeing a userField but we're not
                this.log(".... found userField? " + userFieldFound);
                if (!userFieldFound && login.usernameField != "")
                    continue;

                if (login.passwordField != "") {
                    foundNode = getElementByName(form.elements,
                                                 login.passwordField);
                    if (foundNode && foundNode.type != "password")
                        foundNode = null;

                    tempPassField = foundNode;
                } else if (userField) {
                    // No password field name was supplied,
                    // try to locate one in the form, but only if we have
                    // a username field.

                    for (var index = 0; index < form.elements.length; index++) {
                        if (form.elements[index].isSameNode(foundNode))
                            break;
                    }

                    if (index >= 0) {
                        // Search forwards
                        for (var l = index + 1; l < form.elements.length; l++) {
                            var e = form.elements[l];
                            if (e.type == "password")
                                foundNode = tempPassField = e;
                        }
                    }

                    if (!tempPassField && index != 0) {
                        // Search backwards
                        for (l = index - 1; l >= 0; l--) {
                            var e = form.elements[l];
                            if (e.type == "password")
                                foundNode = tempPassField = e;
                        }
                    }
                }

                this.log(".... found passField? " +
                            (tempPassField ? true : false));
                if (!tempPassField)
                    continue;

                oldPassValue = tempPassField.value;
                passField = tempPassField;
                if (login.passwordField == "")
                    login.passwordField = passField.name;

                if (oldUserValue != "" && prefillForm) {
                    // The page has prefilled a username.
                    // If it matches any of our saved usernames, prefill
                    // the password for that username.  If there are
                    // multiple saved usernames, we will also attach the
                    // autocomplete listener.

                    prefilledUser = true;
                    if (login.username == oldUserValue)
                        passField.value = login.password;
                }

                if (firstMatch && userField && !attachedToInput) {
                    // We found more than one possible signon for this form...
                    // Listen for blur and autocomplete events on the username
                    // field so we can attempt to prefill the password after
                    // the user has entered/selected the username.

                    this.log(".... found multiple matching logins, " +
                             "attaching autocomplete stuff");
                    this._attachToInput(userField);
                    attachedToInput = true;
                } else {
                    firstMatch = login;
                }
            }

            // If we found more than one match, attachedToInput will be true.
            // But if we found just one, we need to attach the autocomplete
            // listener, and fill in the username and password (if the HTML
            // didn't prefill the username.

            if (firstMatch && !attachedToInput) {
                if (userField)
                    this._attachToInput(userField);

                if (!prefilledUser && prefillForm) {
                        if (userField)
                            userField.value = firstMatch.username;

                        passField.value = firstMatch.password;
                }
            }

            // We're done with this form, loop to the next one...
        }
    },


    /*
     * _attachToInput
     *
     */
    _attachToInput : function (element) {
        this.log("attaching autocomplete stuff");
        element.addEventListener("blur",
                                this._domEventListener, false);
        element.addEventListener("DOMAutoComplete",
                                this._domEventListener, false);
        this._formFillService.markAsLoginManagerField(element);
    },


    /*
     * _fillPassword
     *
     * The user has autocompleted a username field, so fill in the password.
     */
    _fillPassword : function (usernameField) {
        this.log("fillPassword autocomplete username: " + usernameField.value);

        var form = usernameField.form;
        var doc = form.ownerDocument;

        var hostname = this._getPasswordOrigin(doc.documentURI);
        var formSubmitURL = this._getActionOrigin(form)

        // Find the password field. We should always have at least one,
        // or else something has gone rather wrong.
        var pwFields = this._getPasswordFields(form, false);
        if (!pwFields) {
            const err = "No password field for autocomplete password fill.";

            // We want to know about this even if debugging is disabled.
            if (!this._debug)
                dump(err);
            else
                this.log(err);

            return;
        }

        // XXX: we could do better on forms with 2 or 3 password fields.
        var passwordField = pwFields[pwFields.length - 1].element;

        // XXX this would really be cleaner if we could get at the
        // AutoCompleteResult, which has the actual nsILoginInfo for the
        // username selected.

        // Temporary LoginInfo with the info we know.
        var currentLogin = new this._nsLoginInfo();
        currentLogin.init(hostname, formSubmitURL, null,
                          usernameField.value, null,
                          usernameField.name, passwordField.name);

        // Look for a existing login and use its password.
        var match = null;
        var logins = this.findLogins({}, hostname, formSubmitURL, null);

        if (!logins.some(function(l) {
                                match = l;
                                return currentLogin.equalsIgnorePassword(l);
                        }))
        {
            this.log("Can't find a login for this autocomplete result.");
            return;
        }

        this.log("Found a matching login, filling in password.");
        passwordField.value = match.password;
    },






    /* ---------- User Prompts ---------- */




    /*
     * _promptToSaveLogin
     *
     * Called when we detect a new login in a form submission,
     * asks the user what to do.
     *
     * Return values:
     *   0 - Save the login
     *   1 - Ignore the login this time
     *   2 - Never save logins for this site
     */
    _promptToSaveLogin : function (aWindow) {
        const buttonFlags = Ci.nsIPrompt.BUTTON_POS_1_DEFAULT +
            (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_0) +
            (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_1) +
            (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_2);

        var brandShortName =
                this._brandBundle.GetStringFromName("brandShortName");

        var dialogText         = this._getLocalizedString(
                                        "savePasswordText", [brandShortName]);
        var dialogTitle        = this._getLocalizedString(
                                        "savePasswordTitle");
        var neverButtonText    = this._getLocalizedString(
                                        "neverForSiteButtonText");
        var rememberButtonText = this._getLocalizedString(
                                        "rememberButtonText");
        var notNowButtonText   = this._getLocalizedString(
                                        "notNowButtonText");

        this.log("Prompting user to save/ignore login");
        var result = this._promptService.confirmEx(aWindow,
                                            dialogTitle, dialogText,
                                            buttonFlags, rememberButtonText,
                                            notNowButtonText, neverButtonText,
                                            null, {});
        return result;
    },


    /*
     * _promptToChangePassword
     *
     * Called when we think we detect a password change for an existing
     * login, when the form being submitted contains multiple password
     * fields.
     *
     * Return values:
     *   0 - Update the stored password
     *   1 - Do not update the stored password
     */
    _promptToChangePassword : function (aWindow, username) {
        const buttonFlags = Ci.nsIPrompt.STD_YES_NO_BUTTONS;

        var dialogText  = this._getLocalizedString(
                                    "passwordChangeText", [username]);
        var dialogTitle = this._getLocalizedString(
                                    "passwordChangeTitle");

        var result = this._promptService.confirmEx(aWindow,
                                dialogTitle, dialogText, buttonFlags,
                                null, null, null,
                                null, {});
        return result;
    },


    /*
     * _promptToChangePasswordWithUsernames
     *
     * Called when we detect a password change in a form submission, but we
     * don't know which existing login (username) it's for. Asks the user
     * to select a username and confirm the password change.
     *
     * Returns multiple paramaters:
     * [0] - User's respone to the dialog
     *   0 = Update the stored password
     *   1 = Do not update the stored password
     * [1] - The username selected
     *   (null if [0] is 1)
     *  
     */
    _promptToChangePasswordWithUsernames : function (aWindow, usernames) {
        const buttonFlags = Ci.nsIPrompt.STD_YES_NO_BUTTONS;

        var dialogText  = this._getLocalizedString("userSelectText");
        var dialogTitle = this._getLocalizedString("passwordChangeTitle");
        var selectedUser = { value: null };

        // If user selects ok, outparam.value is set to the index
        // of the selected username.
        var result = this._promptService.select(aWindow,
                                dialogTitle, dialogText,
                                usernames.length, usernames,
                                selectedUser);

        return [result, selectedUser.value];
    },


    /*
     * _getLocalisedString
     *
     * Can be called as:
     *   _getLocalisedString("key1");
     *   _getLocalizedString("key2", ["arg1"]);
     *   _getLocalizedString("key3", ["arg1", "arg2"]);
     *   (etc)
     *
     * Returns the localized string for the specified key,
     * formatted if required.
     *
     */ 
    _getLocalizedString : function (key, formatArgs) {
        if (formatArgs)
            return this._strBundle.formatStringFromName(
                                        key, formatArgs, formatArgs.length);
        else
            return this._strBundle.GetStringFromName(key);
    }

}; // end of LoginManager implementation




// nsIAutoCompleteResult implementation
function UserAutoCompleteResult (aSearchString, matchingLogins) {
    function loginSort(a,b) {
        var userA = a.username.toLowerCase();
        var userB = b.username.toLowerCase();

        if (a < b)
            return -1;

        if (b > a)
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

    getCommentAt : function (index) {
        return "";
    },

    getStyleAt : function (index) {
        return "";
    },

    removeValueAt : function (index, removeFromDB) {
        if (index < 0 || index >= this.logins.length)
            throw "Index out of range.";

        var removedLogin = this.logins.splice(index, 1);
        this.matchCount--;
        if (this.defaultIndex > this.logins.length)
            this.defaultIndex--;

        if (removeFromDB) {
            var pwmgr = Cc["@mozilla.org/login-manager;1"]
                            .getService(Ci.nsILoginManager);
            pwmgr.removeLogin(removedLogin);
        }
    },
};

var component = [LoginManager];
function NSGetModule (compMgr, fileSpec) {
    return XPCOMUtils.generateModule(component);
}
