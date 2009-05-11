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

const EXPORTED_SYMBOLS = ["Authenticator"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

let Authenticator = {
    __logService : null, // Console logging service, used for debugging.
    get _logService() {
        if (!this.__logService)
            this.__logService = Cc["@mozilla.org/consoleservice;1"].
                                getService(Ci.nsIConsoleService);
        return this.__logService;
    },

    __ioService: null, // IO service for string -> nsIURI conversion
    get _ioService() {
        if (!this.__ioService)
            this.__ioService = Cc["@mozilla.org/network/io-service;1"].
                               getService(Ci.nsIIOService);
        return this.__ioService;
    },


    __formFillService : null, // FormFillController, for username autocompleting
    get _formFillService() {
        if (!this.__formFillService)
            this.__formFillService =
                            Cc["@mozilla.org/satchel/form-fill-controller;1"].
                            getService(Ci.nsIFormFillController);
        return this.__formFillService;
    },


    __observerService : null, // Observer Service, for notifications
    get _observerService() {
        if (!this.__observerService)
            this.__observerService = Cc["@mozilla.org/observer-service;1"].
                                     getService(Ci.nsIObserverService);
        return this.__observerService;
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
     * Initialize the Authenticator. Automatically called when service
     * is created.
     *
     * Note: Service created in /browser/base/content/browser.js,
     *       delayedStartup()
     */
    init : function () {
        // Cache references to current |this| in utility objects
        this._observer._pwmgr            = this;

        // Preferences. Add observer so we get notified of changes.
        this._prefBranch = Cc["@mozilla.org/preferences-service;1"].
                           getService(Ci.nsIPrefService).getBranch("signon.");
        this._prefBranch.QueryInterface(Ci.nsIPrefBranch2);
        this._prefBranch.addObserver("", this._observer, false);

        // Get current preference values.
        this._debug = this._prefBranch.getBoolPref("debug");
        this._remember = this._prefBranch.getBoolPref("rememberSignons");

        // Get constructor for nsILoginInfo
        this._nsLoginInfo = new Components.Constructor(
            "@mozilla.org/login-manager/loginInfo;1", Ci.nsILoginInfo);
    },

    /*
     * log
     *
     * Internal function for logging debug messages to the Error Console window
     */
    log : function (message) {
        if (!this._debug)
            return;
        dump("Authenticator: " + message + "\n");
        this._logService.logStringMessage("Authenticator: " + message);
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

    /* ---------- Primary Public interfaces ---------- */

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
     * _getPasswordOrigin
     *
     * Get the parts of the URL we want for identification.
     */
    _getPasswordOrigin : function (uriString, allowJS) {
        var realm = "";
        try {
            var uri = this._ioService.newURI(uriString, null, null);

            if (allowJS && uri.scheme == "javascript")
                return "javascript:"

            realm = uri.scheme + "://" + uri.host;

            // If the URI explicitly specified a port, only include it when
            // it's not the default. (We never want "http://foo.com:80")
            var port = uri.port;
            if (port != -1) {
                var handler = this._ioService.getProtocolHandler(uri.scheme);
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


    /*
     * _fillDocument
     *
     * Called when a page has loaded. For each form in the document,
     * we check to see if it can be filled with a stored login.
     */
    _fillDocument : function (doc) {
        var forms = doc.forms;
        if (!forms || forms.length == 0)
            return [];

        var formOrigin = this._getPasswordOrigin(doc.documentURI);

        // If there are no logins for this site, bail out now.
        if (!this.countLogins(formOrigin, "", null))
            return [];

        this.log("fillDocument processing " + forms.length +
                 " forms on " + doc.documentURI);

        var autofillForm = !this._inPrivateBrowsing &&
                           this._prefBranch.getBoolPref("autofillForms");
        var previousActionOrigin = null;
        var foundLogins = null;

        let formInfos = [];
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
            let formInfo = this._fillForm(form, autofillForm, false, foundLogins);
            foundLogins = formInfo.foundLogins;
            if (formInfo.canFillForm)
                formInfos.push(formInfo);
        } // foreach form
        return formInfos;
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
    _fillForm : function (form, autofillForm, ignoreAutocomplete, foundLogins) {
        // Heuristically determine what the user/pass fields are
        // We do this before checking to see if logins are stored,
        // so that the user isn't prompted for a master password
        // without need.
        var [usernameField, passwordField, ignored] =
            this._getFormFields(form, false);

        // Need a valid password field to do anything.
        if (passwordField == null)
            return { foundLogins: foundLogins };

        // If the fields are disabled or read-only, there's nothing to do.
        if (passwordField.disabled || passwordField.readOnly ||
            usernameField && (usernameField.disabled ||
                              usernameField.readOnly)) {
            this.log("not filling form, login fields disabled");
            return { foundLogins: foundLogins };
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

        logins = foundLogins.filter(function (l) {
                var fit = (l.username.length <= maxUsernameLen &&
                           l.password.length <= maxPasswordLen);
                if (!fit)
                    this.log("Ignored " + l.username + " login: won't fit");

                return fit;
            }, this);


        // Nothing to do if we have no matching logins available.
        if (logins.length == 0)
            return { foundLogins: foundLogins };



        // Attach autocomplete stuff to the username field, if we have
        // one. This is normally used to select from multiple accounts,
        // but even with one account we should refill if the user edits.
        //if (usernameField)
        //    this._attachToInput(usernameField);

        // Don't clobber an existing password.
        if (passwordField.value)
            return { foundLogins: foundLogins };

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
            if (matchingLogins.length)
                selectedLogin = matchingLogins[0];
            else
                this.log("Password not filled. None of the stored " +
                         "logins match the username already present.");
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
            if (matchingLogins.length == 1)
                selectedLogin = matchingLogins[0];
            else
                this.log("Multiple logins for form, so not filling any.");
        }

        //var didFillForm = false;
        //if (selectedLogin && autofillForm && !isFormDisabled) {
        //    // Fill the form
        //    if (usernameField)
        //        usernameField.value = selectedLogin.username;
        //    passwordField.value = selectedLogin.password;
        //    didFillForm = true;
        //} else if (selectedLogin && !autofillForm) {
        //    // For when autofillForm is false, but we still have the information
        //    // to fill a form, we notify observers.
        //    this._observerService.notifyObservers(form, "passwordmgr-found-form", "noAutofillForms");
        //    this.log("autofillForms=false but form can be filled; notified observers");
        //} else if (selectedLogin && isFormDisabled) {
        //    // For when autocomplete is off, but we still have the information
        //    // to fill a form, we notify observers.
        //    this._observerService.notifyObservers(form, "passwordmgr-found-form", "autocompleteOff");
        //    this.log("autocomplete=off but form can be filled; notified observers");
        //}

        return { canFillForm:   true,
                 usernameField: usernameField,
                 passwordField: passwordField,
                 foundLogins:   foundLogins,
                 selectedLogin: selectedLogin };
    }

}; // end of LoginManager implementation

Authenticator.init();
