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
        observerService.addObserver(this._observer, "xpcom-shutdown", false);

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

            return true; // Always return true, or form submit will be canceled.
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
            } else if (topic == "xpcom-shutdown") {
                for (let i in this._pwmgr) {
                  try {
                    this._pwmgr[i] = null;
                  } catch(ex) {}
                }
                this._pwmgr = null;
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
                return;

            if (!this._pwmgr._remember)
                return;

            var domWin = aWebProgress.DOMWindow;
            var domDoc = domWin.document;

            // Only process things which might have HTML forms.
            if (!(domDoc instanceof Ci.nsIDOMHTMLDocument))
                return;

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
            return;
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
                    this._pwmgr._fillDocument(doc);
                    return;

                case "DOMAutoComplete":
                case "blur":
                    inputElement = event.target;
                    this._pwmgr._fillPassword(inputElement);
                    return;

                default:
                    this._pwmgr.log("Oops! This event unexpected.");
                    return;
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
     *
     * Returns an array of password field elements for the specified form.
     * If no pw fields are found, or if more than 3 are found, then null
     * is returned.
     *
     * skipEmptyFields can be set to ignore password fields with no value.
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
     * _getFormFields
     *
     * Returns the username and password fields found in the form.
     * Can handle complex forms by trying to figure out what the
     * relevant fields are.
     *
     * Returns: [usernameField, newPasswordField, oldPasswordField]
     *
     * usernameField may be null.
     * newPasswordField will always be non-null.
     * oldPasswordField may be null. If null, newPasswordField is just
     * "theLoginField". If not null, the form is apparently a
     * change-password field, with oldPasswordField containing the password
     * that is being changed.
     */
    _getFormFields : function (form, isSubmission) {
        // Locate the password field(s) in the form. Up to 3 supported.
        // If there's no password field, there's nothing for us to do.
        var pwFields = this._getPasswordFields(form, isSubmission);
        if (!pwFields) {
            this.log("(form ignored -- either 0 or >3 pw fields.)");
            return [null, null, null];
        }


        // Locate the username field in the form by searching backwards
        // from the first passwordfield, assume the first text field is the
        // username. We might not find a username field if the user is
        // already logged in to the site. 
        for (var i = pwFields[0].index - 1; i >= 0; i--) {
            if (form.elements[i].type == "text") {
                var usernameField = form.elements[i];
                break;
            }
        }

        if (!usernameField)
            this.log("(form -- no username field found)");


        // If we're not submitting a form (it's a page load), there are no
        // password field values for us to use for identifying fields. So,
        // just assume the first password field is the one to be filled in.
        if (!isSubmission || pwFields.length == 1)
            return [usernameField, pwFields[0].element, null];


        // Try to figure out WTF is in the form based on the password values.
        var oldPasswordField, newPasswordField;
        var pw1 = pwFields[0].element.value;
        var pw2 = pwFields[1].element.value;
        var pw3 = (pwFields[2] ? pwFields[2].element.value : null);

        if (pwFields.length == 3) {
            // Look for two identical passwords, that's the new password

            if (pw1 == pw2 && pw2 == pw3) {
                // All 3 passwords the same? Weird! Treat as if 1 pw field.
                newPasswordField = pwFields[0].element;
                oldPasswordField = null;
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
                this.log("(form ignored -- all 3 pw fields differ)");
                return [null, null, null];
            }
        } else { // pwFields.length == 2
            if (pw1 == pw2) {
                // Treat as if 1 pw field
                newPasswordField = pwFields[0].element;
                oldPasswordField = null;
            } else {
                // Just assume that the 2nd password is the new password
                oldPasswordField = pwFields[0].element;
                newPasswordField = pwFields[1].element;
            }
        }

        return [usernameField, newPasswordField, oldPasswordField];
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
        function findExistingLogin(pwmgr, hostname,
                                   formSubmitURL, usernameField) {

            var searchLogin = new pwmgr._nsLoginInfo();
            searchLogin.init(hostname, formSubmitURL, null,
                             usernameField.value, "",
                             usernameField.name,  "");

            var logins = pwmgr.findLogins({}, hostname, formSubmitURL, null);
            var existingLogin;
            var found = logins.some(function(l) {
                                    existingLogin = l;
                                    return searchLogin.equalsIgnorePassword(l);
                                });

            return (found ? existingLogin : null);
        }

        function getPrompter(aWindow) {
            var prompterSvc = Cc["@mozilla.org/login-manager/prompter;1"].
                            createInstance(Ci.nsILoginManagerPrompter);
            prompterSvc.init(aWindow);
            return prompterSvc;
        }



        var doc = form.ownerDocument;
        var win = doc.defaultView;

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


        // Get the appropriate fields from the form.
        var [usernameField, newPasswordField, oldPasswordField] =
            this._getFormFields(form, true);

        // Need at least 1 valid password field to do anything.
        if (newPasswordField == null)
                return;

        // Check for autocomplete=off attribute. We don't use it to prevent
        // autofilling (for existing logins), but won't save logins when it's
        // present.
        if (autocompleteDisabled(form) ||
            autocompleteDisabled(usernameField) ||
            autocompleteDisabled(newPasswordField) ||
            autocompleteDisabled(oldPasswordField)) {
                this.log("(form submission ignored -- autocomplete=off found)");
                return;
        }


        var formLogin = new this._nsLoginInfo();
        formLogin.init(hostname, formSubmitURL, null,
                    (usernameField ? usernameField.value : null),
                    newPasswordField.value,
                    (usernameField ? usernameField.name  : null),
                    newPasswordField.name);

        // If we didn't find a username field, but seem to be changing a
        // password, allow the user to select from a list of applicable
        // logins to update the password for.
        if (!usernameField && oldPasswordField) {

            var logins = this.findLogins({}, hostname, formSubmitURL, null);

            // XXX we could be smarter here: look for a login matching the
            // old password value. If there's only one, update it. If there's
            // more than one we could filter the list (but, edge case: the
            // login for the pwchange is in pwmgr, but with an outdated
            // password. and the user has another login, with the same
            // password as the form login's old password.) ugh.
            // XXX if you're changing a password, and there's no username
            // in the form, then you can't add the login. Will need to change
            // prompting to allow this.

            if (logins.length == 0) {
                this.log("(no logins for this host -- pwchange ignored)");
                return;
            }

            var prompter = getPrompter(win);

            if (logins.length == 1) {
                var oldLogin = logins[0];
                formLogin.username      = oldLogin.username;
                formLogin.usernameField = oldLogin.usernameField;

                prompter.promptToChangePassword(oldLogin, formLogin);
            } else {
                prompter.promptToChangePasswordWithUsernames(
                                    logins, logins.length, formLogin);
            }

            return;
        }

        if (!usernameField && !oldPasswordField) {
            this.log("XXX not handled yet");
            return;
        }

        // We have a username. Look for an existing login that matches the
        // form data (other than the password, which might be different)
        existingLogin = findExistingLogin(this, hostname, formSubmitURL,
                                          usernameField);
        if (existingLogin) {
            this.log("Found an existing login matching this form submission");

            // Change password if needed
            if (existingLogin.password != formLogin.password) {
                this.log("...Updating password for existing login.");
                this.modifyLogin(existingLogin, formLogin);
            }

            return;
        }


        // Prompt user to save login (via dialog or notification bar)
        prompter = getPrompter(win);
        prompter.promptToSavePassword(formLogin);
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
     * Called when a page has loaded. For each form in the document,
     * we check to see if it can be filled with a stored login.
     */
    _fillDocument : function (doc) {
        var forms = doc.forms;
        if (!forms || forms.length == 0)
            return; 

        var formOrigin = this._getPasswordOrigin(doc.documentURI);
        var autofillForm = this._prefBranch.getBoolPref("autofillForms");

        this.log("fillDocument found " + forms.length +
                 " forms on " + doc.documentURI);

        var previousActionOrigin = null;

        for (var i = 0; i < forms.length; i++) {
            var form = forms[i];
            var actionOrigin = this._getActionOrigin(form);

            // Heuristically determine what the user/pass fields are
            // We do this before checking to see if logins are stored,
            // so that the user isn't prompted for a master password
            // without need.
            var [usernameField, passwordField, ignored] =
                this._getFormFields(form, false);

            // Need a valid password field to do anything.
            if (passwordField == null)
                continue;


            // Only the actionOrigin might be changing, so if it's the same
            // as the last form on the page we can reuse the same logins.
            if (actionOrigin != previousActionOrigin) {
                var foundLogins =
                    this.findLogins({}, formOrigin, actionOrigin, null);

                this.log("form[" + i + "]: got " +
                         foundLogins.length + " logins.");

                previousActionOrigin = actionOrigin;
            } else {
                this.log("form[" + i + "]: using logins from last form.");
            }


            // Discard logins which have username/password values that don't
            // fit into the fields (as specified by the maxlength attribute).
            // The user couldn't enter these values anyway, and it helps
            // with sites that have an extra PIN to be entered (bug 391514)
            var maxUsernameLen = Number.MAX_VALUE;
            var maxPasswordLen = Number.MAX_VALUE;

            // If attribute wasn't set, default is -1.
            if (usernameField && usernameField.maxLength >= 0)
                maxUsernameLen = usernameField.maxLength;
            if (passwordField.maxLength >= 0)
                maxPasswordLen = passwordField.maxLength;

            logins = foundLogins.filter(function (l) {
                    var fit = (l.username.length <= maxUsernameLen &&
                               l.password.length <= maxPasswordLen);
                    if (!fit)
                        this.log("Ignored " + l.username + " login: won't fit");

                    return fit;
                }, this);


            // Nothing to do if we have no matching logins available.
            if (logins.length == 0)
                continue;


            // Attach autocomplete stuff to the username field, if we have
            // one. This is normally used to select from multiple accounts,
            // but even with one account we should refill if the user edits.
            // XXX should be able to pass in |logins| to init attachment
            if (usernameField)
                this._attachToInput(usernameField);

            if (autofillForm) {

                // If username was specified in the form, only fill in the
                // password if we find a matching login.
                if (usernameField && usernameField.value) {
                    var username = usernameField.value;

                    var matchingLogin;
                    var found = logins.some(function(l) {
                                                matchingLogin = l;
                                                return (l.username == username);
                                            });
                    if (found)
                        passwordField.value = matchingLogin.password;

                } else if (logins.length == 1) {
                    if (usernameField)
                        usernameField.value = logins[0].username;
                    passwordField.value = logins[0].password;
                }
            }
        } // foreach form
    },


    /*
     * _attachToInput
     *
     * Hooks up autocomplete support to a username field, to allow
     * a user editing the field to select an existing login and have
     * the password field filled in.
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
