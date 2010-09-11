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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Justin Dolske <dolske@mozilla.com> (original author)
 *  Ehsan Akhgari <ehsan.akhgari@gmail.com>
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
Components.utils.import("resource://gre/modules/Services.jsm");

function LoginManager() {
    this.init();
}

LoginManager.prototype = {

    classID: Components.ID("{cb9e0de8-3598-4ed7-857b-827f011ad5d8}"),
    QueryInterface : XPCOMUtils.generateQI([Ci.nsILoginManager,
                                            Ci.nsISupportsWeakReference]),


    /* ---------- private memebers ---------- */


    __formFillService : null, // FormFillController, for username autocompleting
    get _formFillService() {
        if (!this.__formFillService)
            this.__formFillService =
                            Cc["@mozilla.org/satchel/form-fill-controller;1"].
                            getService(Ci.nsIFormFillController);
        return this.__formFillService;
    },


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


    // Private Browsing Service
    // If the service is not available, null will be returned.
    __privateBrowsingService : undefined,
    get _privateBrowsingService() {
        if (this.__privateBrowsingService == undefined) {
            if ("@mozilla.org/privatebrowsing;1" in Cc)
                this.__privateBrowsingService = Cc["@mozilla.org/privatebrowsing;1"].
                                                getService(Ci.nsIPrivateBrowsingService);
            else
                this.__privateBrowsingService = null;
        }
        return this.__privateBrowsingService;
    },


    // Whether we are in private browsing mode
    get _inPrivateBrowsing() {
        var pbSvc = this._privateBrowsingService;
        if (pbSvc)
            return pbSvc.privateBrowsingEnabled;
        else
            return false;
    },

    _prefBranch  : null, // Preferences service
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
        this._prefBranch = Services.prefs.getBranch("signon.");
        this._prefBranch.QueryInterface(Ci.nsIPrefBranch2);
        this._prefBranch.addObserver("", this._observer, false);

        // Get current preference values.
        this._debug = this._prefBranch.getBoolPref("debug");

        this._remember = this._prefBranch.getBoolPref("rememberSignons");


        // Get constructor for nsILoginInfo
        this._nsLoginInfo = new Components.Constructor(
            "@mozilla.org/login-manager/loginInfo;1", Ci.nsILoginInfo);


        // Form submit observer checks forms for new logins and pw changes.
        Services.obs.addObserver(this._observer, "earlyformsubmit", false);
        Services.obs.addObserver(this._observer, "xpcom-shutdown", false);

        // WebProgressListener for getting notification of new doc loads.
        var progress = Cc["@mozilla.org/docloaderservice;1"].
                       getService(Ci.nsIWebProgress);
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
        Services.console.logStringMessage("Login Manager: " + message);
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

            this._pwmgr.log("onStateChange accepted: req = " +
                            (aRequest ?  aRequest.name : "(null)") +
                            ", flags = 0x" + aStateFlags.toString(16));

            // Fastback doesn't fire DOMContentLoaded, so process forms now.
            if (aStateFlags & Ci.nsIWebProgressListener.STATE_RESTORING) {
                this._pwmgr.log("onStateChange: restoring document");
                return this._pwmgr._fillDocument(domDoc);
            }

            // Add event listener to process page when DOM is complete.
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
            if (!event.isTrusted)
                return;

            this._pwmgr.log("domEventListener: got event " + event.type);

            switch (event.type) {
                case "DOMContentLoaded":
                    this._pwmgr._fillDocument(event.target);
                    return;

                case "DOMAutoComplete":
                case "blur":
                    var acInputField = event.target;
                    var acForm = acInputField.form;

                    // If the username is blank, bail out now -- we don't want
                    // fillForm() to try filling in a login without a username
                    // to filter on (bug 471906).
                    if (!acInputField.value)
                        return;

                    // Make sure the username field fillForm will use is the
                    // same field as the autocomplete was activated on. If
                    // not, the DOM has been altered and we'll just give up.
                    var [usernameField, passwordField, ignored] =
                        this._pwmgr._getFormFields(acForm, false);
                    if (usernameField == acInputField && passwordField) {
                        // This shouldn't trigger a master password prompt,
                        // because we don't attach to the input until after we
                        // successfully obtain logins for the form.
                        this._pwmgr._fillForm(acForm, true, true, true, null);
                    } else {
                        this._pwmgr.log("Oops, form changed before AC invoked");
                    }
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
            var element = form.elements[i];
            if (!(element instanceof Ci.nsIDOMHTMLInputElement) ||
                element.type != "password")
                continue;

            if (skipEmptyFields && !element.value)
                continue;

            pwFields[pwFields.length] = {
                                            index   : i,
                                            element : element
                                        };
        }

        // If too few or too many fields, bail out.
        if (pwFields.length == 0) {
            this.log("(form ignored -- no password fields.)");
            return null;
        } else if (pwFields.length > 3) {
            this.log("(form ignored -- too many password fields. [got " +
                        pwFields.length + "])");
            return null;
        }

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
        var usernameField = null;

        // Locate the password field(s) in the form. Up to 3 supported.
        // If there's no password field, there's nothing for us to do.
        var pwFields = this._getPasswordFields(form, isSubmission);
        if (!pwFields)
            return [null, null, null];


        // Locate the username field in the form by searching backwards
        // from the first passwordfield, assume the first text field is the
        // username. We might not find a username field if the user is
        // already logged in to the site. 
        for (var i = pwFields[0].index - 1; i >= 0; i--) {
            if (form.elements[i].type == "text") {
                usernameField = form.elements[i];
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
     * _isAutoCompleteDisabled
     *
     * Returns true if the page requests autocomplete be disabled for the
     * specified form input.
     */
    _isAutocompleteDisabled :  function (element) {
        if (element && element.hasAttribute("autocomplete") &&
            element.getAttribute("autocomplete").toLowerCase() == "off")
            return true;

        return false;
    },

    /*
     * _onFormSubmit
     *
     * Called by the our observer when notified of a form submission.
     * [Note that this happens before any DOM onsubmit handlers are invoked.]
     * Looks for a password change in the submitted form, so we can update
     * our stored password.
     */
    _onFormSubmit : function (form) {

        // local helper function
        function getPrompter(aWindow) {
            var prompterSvc = Cc["@mozilla.org/login-manager/prompter;1"].
                              createInstance(Ci.nsILoginManagerPrompter);
            prompterSvc.init(aWindow);
            return prompterSvc;
        }

        if (this._inPrivateBrowsing) {
            // We won't do anything in private browsing mode anyway,
            // so there's no need to perform further checks.
            this.log("(form submission ignored in private browsing mode)");
            return;
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
        // XXX spin out a bug that we don't update timeLastUsed in this case?
        if (this._isAutocompleteDisabled(form) ||
            this._isAutocompleteDisabled(usernameField) ||
            this._isAutocompleteDisabled(newPasswordField) ||
            this._isAutocompleteDisabled(oldPasswordField)) {
                this.log("(form submission ignored -- autocomplete=off found)");
                return;
        }


        var formLogin = new this._nsLoginInfo();
        formLogin.init(hostname, formSubmitURL, null,
                    (usernameField ? usernameField.value : ""),
                    newPasswordField.value,
                    (usernameField ? usernameField.name  : ""),
                    newPasswordField.name);

        // If we didn't find a username field, but seem to be changing a
        // password, allow the user to select from a list of applicable
        // logins to update the password for.
        if (!usernameField && oldPasswordField) {

            var logins = this.findLogins({}, hostname, formSubmitURL, null);

            if (logins.length == 0) {
                // Could prompt to save this as a new password-only login.
                // This seems uncommon, and might be wrong, so ignore.
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
                prompter = getPrompter(win);
                prompter.promptToChangePassword(existingLogin, formLogin);
            } else {
                // Update the lastUsed timestamp.
                var propBag = Cc["@mozilla.org/hash-property-bag;1"].
                              createInstance(Ci.nsIWritablePropertyBag);
                propBag.setProperty("timeLastUsed", Date.now());
                propBag.setProperty("timesUsedIncrement", 1);
                this.modifyLogin(existingLogin, propBag);
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
        var uriString = form.mozActionUri;

        // A blank or mission action submits to where it came from.
        if (uriString == "")
            uriString = form.baseURI; // ala bug 297761

        return this._getPasswordOrigin(uriString, true);
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

        // If there are no logins for this site, bail out now.
        if (!this.countLogins(formOrigin, "", null))
            return;

        // If we're currently displaying a master password prompt, defer
        // processing this document until the user handles the prompt.
        if (this.uiBusy) {
            this.log("deferring fillDoc for " + doc.documentURI);
            let self = this;
            let observer = {
                QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsISupportsWeakReference]),

                observe: function (subject, topic, data) {
                    self.log("Got deferred fillDoc notification: " + topic);
                    // Only run observer once.
                    Services.obs.removeObserver(this, "passwordmgr-crypto-login");
                    Services.obs.removeObserver(this, "passwordmgr-crypto-loginCanceled");
                    if (topic == "passwordmgr-crypto-loginCanceled")
                        return;
                    self._fillDocument(doc);
                },
                handleEvent : function (event) {
                    // Not expected to be called
                }
            };
            // Trickyness follows: We want an observer, but don't want it to
            // cause leaks. So add the observer with a weak reference, and use
            // a dummy event listener (a strong reference) to keep it alive
            // until the document is destroyed.
            Services.obs.addObserver(observer, "passwordmgr-crypto-login", true);
            Services.obs.addObserver(observer, "passwordmgr-crypto-loginCanceled", true);
            doc.addEventListener("mozCleverClosureHack", observer, false);
            return;
        }

        this.log("fillDocument processing " + forms.length +
                 " forms on " + doc.documentURI);

        var autofillForm = !this._inPrivateBrowsing &&
                           this._prefBranch.getBoolPref("autofillForms");
        var previousActionOrigin = null;
        var foundLogins = null;

        for (var i = 0; i < forms.length; i++) {
            var form = forms[i];

            // Only the actionOrigin might be changing, so if it's the same
            // as the last form on the page we can reuse the same logins.
            var actionOrigin = this._getActionOrigin(form);
            if (actionOrigin != previousActionOrigin) {
                foundLogins = null;
                previousActionOrigin = actionOrigin;
            }
            this.log("_fillDocument processing form[" + i + "]");
            foundLogins = this._fillForm(form, autofillForm, false, false, foundLogins)[1];
        } // foreach form
    },


    /*
     * _fillform
     *
     * Fill the form with login information if we can find it. This will find
     * an array of logins if not given any, otherwise it will use the logins
     * passed in. The logins are returned so they can be reused for
     * optimization. Success of action is also returned in format
     * [success, foundLogins]. autofillForm denotes if we should fill the form
     * in automatically, ignoreAutocomplete denotes if we should ignore
     * autocomplete=off attributes, and foundLogins is an array of nsILoginInfo
     * for optimization
     */
    _fillForm : function (form, autofillForm, ignoreAutocomplete,
                          clobberPassword, foundLogins) {
        // Heuristically determine what the user/pass fields are
        // We do this before checking to see if logins are stored,
        // so that the user isn't prompted for a master password
        // without need.
        var [usernameField, passwordField, ignored] =
            this._getFormFields(form, false);

        // Need a valid password field to do anything.
        if (passwordField == null)
            return [false, foundLogins];

        // If the fields are disabled or read-only, there's nothing to do.
        if (passwordField.disabled || passwordField.readOnly ||
            usernameField && (usernameField.disabled ||
                              usernameField.readOnly)) {
            this.log("not filling form, login fields disabled");
            return [false, foundLogins];
        }

        // Need to get a list of logins if we weren't given them
        if (foundLogins == null) {
            var formOrigin = 
                this._getPasswordOrigin(form.ownerDocument.documentURI);
            var actionOrigin = this._getActionOrigin(form);
            foundLogins = this.findLogins({}, formOrigin, actionOrigin, null);
            this.log("found " + foundLogins.length + " matching logins.");
        } else {
            this.log("reusing logins from last form.");
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

        var logins = foundLogins.filter(function (l) {
                var fit = (l.username.length <= maxUsernameLen &&
                           l.password.length <= maxPasswordLen);
                if (!fit)
                    this.log("Ignored " + l.username + " login: won't fit");

                return fit;
            }, this);


        // Nothing to do if we have no matching logins available.
        if (logins.length == 0)
            return [false, foundLogins];


        // The reason we didn't end up filling the form, if any.  We include
        // this in the formInfo object we send with the passwordmgr-found-logins
        // notification.  See the _notifyFoundLogins docs for possible values.
        var didntFillReason = null;

        // Attach autocomplete stuff to the username field, if we have
        // one. This is normally used to select from multiple accounts,
        // but even with one account we should refill if the user edits.
        if (usernameField)
            this._attachToInput(usernameField);

        // Don't clobber an existing password.
        if (passwordField.value && !clobberPassword) {
            didntFillReason = "existingPassword";
            this._notifyFoundLogins(didntFillReason, usernameField,
                                    passwordField, foundLogins, null);
            return [false, foundLogins];
        }

        // If the form has an autocomplete=off attribute in play, don't
        // fill in the login automatically. We check this after attaching
        // the autocomplete stuff to the username field, so the user can
        // still manually select a login to be filled in.
        var isFormDisabled = false;
        if (!ignoreAutocomplete &&
            (this._isAutocompleteDisabled(form) ||
             this._isAutocompleteDisabled(usernameField) ||
             this._isAutocompleteDisabled(passwordField))) {

            isFormDisabled = true;
            this.log("form not filled, has autocomplete=off");
        }

        // Variable such that we reduce code duplication and can be sure we
        // should be firing notifications if and only if we can fill the form.
        var selectedLogin = null;

        if (usernameField && usernameField.value) {
            // If username was specified in the form, only fill in the
            // password if we find a matching login.
            var username = usernameField.value.toLowerCase();

            let matchingLogins = logins.filter(function(l)
                                     l.username.toLowerCase() == username);
            if (matchingLogins.length) {
                selectedLogin = matchingLogins[0];
            } else {
                didntFillReason = "existingUsername";
                this.log("Password not filled. None of the stored " +
                         "logins match the username already present.");
            }
        } else if (logins.length == 1) {
            selectedLogin = logins[0];
        } else {
            // We have multiple logins. Handle a special case here, for sites
            // which have a normal user+pass login *and* a password-only login
            // (eg, a PIN). Prefer the login that matches the type of the form
            // (user+pass or pass-only) when there's exactly one that matches.
            let matchingLogins;
            if (usernameField)
                matchingLogins = logins.filter(function(l) l.username);
            else
                matchingLogins = logins.filter(function(l) !l.username);
            if (matchingLogins.length == 1) {
                selectedLogin = matchingLogins[0];
            } else {
                didntFillReason = "multipleLogins";
                this.log("Multiple logins for form, so not filling any.");
            }
        }

        var didFillForm = false;
        if (selectedLogin && autofillForm && !isFormDisabled) {
            // Fill the form
            if (usernameField)
                usernameField.value = selectedLogin.username;
            passwordField.value = selectedLogin.password;
            didFillForm = true;
        } else if (selectedLogin && !autofillForm) {
            // For when autofillForm is false, but we still have the information
            // to fill a form, we notify observers.
            didntFillReason = "noAutofillForms";
            Services.obs.notifyObservers(form, "passwordmgr-found-form", didntFillReason);
            this.log("autofillForms=false but form can be filled; notified observers");
        } else if (selectedLogin && isFormDisabled) {
            // For when autocomplete is off, but we still have the information
            // to fill a form, we notify observers.
            didntFillReason = "autocompleteOff";
            Services.obs.notifyObservers(form, "passwordmgr-found-form", didntFillReason);
            this.log("autocomplete=off but form can be filled; notified observers");
        }

        this._notifyFoundLogins(didntFillReason, usernameField, passwordField,
                                foundLogins, selectedLogin);

        return [didFillForm, foundLogins];
    },

    /**
     * Notify observers about an attempt to fill a form that resulted in some
     * saved logins being found for the form.
     *
     * This does not get called if the login manager attempts to fill a form
     * but does not find any saved logins.  It does, however, get called when
     * the login manager does find saved logins whether or not it actually
     * fills the form with one of them.
     *
     * @param didntFillReason {String}
     *        the reason the login manager didn't fill the form, if any;
     *        if the value of this parameter is null, then the form was filled;
     *        otherwise, this parameter will be one of these values:
     *          existingUsername: the username field already contains a username
     *                            that doesn't match any stored usernames
     *          existingPassword: the password field already contains a password
     *          autocompleteOff:  autocomplete has been disabled for the form
     *                            or its username or password fields
     *          multipleLogins:   we have multiple logins for the form
     *          noAutofillForms:  the autofillForms pref is set to false
     *
     * @param usernameField   {HTMLInputElement}
     *        the username field detected by the login manager, if any;
     *        otherwise null
     *
     * @param passwordField   {HTMLInputElement}
     *        the password field detected by the login manager
     *
     * @param foundLogins     {Array}
     *        an array of nsILoginInfos that can be used to fill the form
     *
     * @param selectedLogin   {nsILoginInfo}
     *        the nsILoginInfo that was/would be used to fill the form, if any;
     *        otherwise null; whether or not it was actually used depends on
     *        the value of the didntFillReason parameter
     */
    _notifyFoundLogins : function (didntFillReason, usernameField,
                                   passwordField, foundLogins, selectedLogin) {
        // We need .setProperty(), which is a method on the original
        // nsIWritablePropertyBag. Strangley enough, nsIWritablePropertyBag2
        // doesn't inherit from that, so the additional QI is needed.
        let formInfo = Cc["@mozilla.org/hash-property-bag;1"].
                       createInstance(Ci.nsIWritablePropertyBag2).
                       QueryInterface(Ci.nsIWritablePropertyBag);

        formInfo.setPropertyAsACString("didntFillReason", didntFillReason);
        formInfo.setPropertyAsInterface("usernameField", usernameField);
        formInfo.setPropertyAsInterface("passwordField", passwordField);
        formInfo.setProperty("foundLogins", foundLogins.concat());
        formInfo.setPropertyAsInterface("selectedLogin", selectedLogin);

        Services.obs.notifyObservers(formInfo, "passwordmgr-found-logins", null);
    },

    /*
     * fillForm
     *
     * Fill the form with login information if we can find it.
     */
    fillForm : function (form) {
        this.log("fillForm processing form[id=" + form.id + "]");
        return this._fillForm(form, true, true, false, null)[0];
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

    getLabelAt: function(index) {
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
