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

var Cc = Components.classes;
var Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

var loginManager = {

    /* ---------- private members ---------- */


    get _formFillService() {
        return this._formFillService =
                        Cc["@mozilla.org/satchel/form-fill-controller;1"].
                        getService(Ci.nsIFormFillController);
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

    _nsLoginInfo : null, // Constructor for nsILoginInfo implementation
    _debug    : false, // mirrors signon.debug
    _remember : true,  // mirrors signon.rememberSignons preference

    init : function () {
        // Cache references to current |this| in utility objects
        this._domEventListener._pwmgr    = this;
        this._observer._pwmgr            = this;

        // Form submit observer checks forms for new logins and pw changes.
        Services.obs.addObserver(this._observer, "earlyformsubmit", false);

        // Add event listener to process page when DOM is complete.
        addEventListener("pageshow", this._domEventListener, false);
        addEventListener("unload", this._domEventListener, false);

        // Get constructor for nsILoginInfo
        this._nsLoginInfo = new Components.Constructor(
            "@mozilla.org/login-manager/loginInfo;1", Ci.nsILoginInfo);

        // Preferences. Add observer so we get notified of changes.
        Services.prefs.addObserver("signon.", this._observer, false);

        // Get current preference values.
        this._debug = Services.prefs.getBoolPref("signon.debug");
        this._remember = Services.prefs.getBoolPref("signon.rememberSignons");
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


    /* fillForm
     *
     * Fill the form with login information if we can find it.
     */
    fillForm: function (form) {
        this._fillForm(form, true, true, false, null);
    },


    /*
     * _fillform
     *
     * Fill the form with login information if we can find it.
     * autofillForm denotes if we should fill the form in automatically
     * ignoreAutocomplete denotes if we should ignore autocomplete=off attributes
     * foundLogins is an array of nsILoginInfo
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
            return false;

        // If the fields are disabled or read-only, there's nothing to do.
        if (passwordField.disabled || passwordField.readOnly ||
            usernameField && (usernameField.disabled ||
                              usernameField.readOnly)) {
            this.log("not filling form, login fields disabled");
            return false;
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
            return false;


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
            return false;
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

        return didFillForm;
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


    /* ---------- Private methods ---------- */


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
     * _fillDocument
     *
     * Called when a page has loaded. For each form in the document,
     * we ask the parent process to see if it can be filled with a stored login
     * and fill them in with the results
     */
    _fillDocument : function (doc) {
        var forms = doc.forms;
        if (!forms || forms.length == 0)
            return;

        this.log("_fillDocument processing " + forms.length +
                 " forms on " + doc.documentURI);

        var autofillForm = !this._inPrivateBrowsing &&
                           Services.prefs.getBoolPref("signon.autofillForms");

        // actionOrigins is a list of each form's action origins for this
        // document. The parent process needs this to find the passwords
        // for each action origin.
        var actionOrigins = [];

        for (var i = 0; i < forms.length; i++) {
            var form = forms[i];
            let [, passwordField, ] = this._getFormFields(form, false);
            if (passwordField) {
                var actionOrigin = this._getActionOrigin(form);
                actionOrigins.push(actionOrigin);
            }
        } // foreach form

        if (!actionOrigins.length)
            return;

        var formOrigin = this._getPasswordOrigin(doc.documentURI);
        var foundLogins = this._getPasswords(actionOrigins, formOrigin);

        for (var i = 0; i < forms.length; i++) {
            var form = forms[i];
            var actionOrigin = this._getActionOrigin(form);
            if (foundLogins[actionOrigin]) {
                this.log("_fillDocument processing form[" + i + "]");
                this._fillForm(form, autofillForm, false, false,
                               foundLogins[actionOrigin]);
            }
        } // foreach form
    },

    /*
     * _getPasswords
     *
     * Retrieve passwords from parent process and prepare logins to be passed to
     * _fillForm.  Returns map from action origins to passwords.
     */
    _getPasswords: function(actionOrigins, formOrigin) {
        // foundLogins will be a map from action origins to passwords.
        var message = sendSyncMessage("PasswordMgr:GetPasswords", {
            actionOrigins: actionOrigins,
            formOrigin: formOrigin
        })[0];

        // XXX need to somehow respond to the UI being busy
        // not needed for Fennec yet

        var foundLogins = message.foundLogins;

        // Each password will be a JSON-unserialized object, but they need to be
        // nsILoginInfo's.
        for (var key in foundLogins) {
            var logins = foundLogins[key];
            for (var i = 0; i < logins.length; i++) {
                var obj = logins[i];
                logins[i] = new this._nsLoginInfo();
                logins[i].init(obj.hostname, obj.formSubmitURL, obj.httpRealm,
                               obj.username, obj.password,
                               obj.usernameField, obj.passwordField);
            }
        }

        return foundLogins;
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
        if (this._inPrivateBrowsing) {
            // We won't do anything in private browsing mode anyway,
            // so there's no need to perform further checks.
            this.log("(form submission ignored in private browsing mode)");
            return;
        }

        var doc = form.ownerDocument;

        // If password saving is disabled (globally or for host), bail out now.
        if (!this._remember)
            return;

        var hostname      = this._getPasswordOrigin(doc.documentURI);
        var formSubmitURL = this._getActionOrigin(form);


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

        sendSyncMessage("PasswordMgr:FormSubmitted", {
            hostname: hostname,
            formSubmitURL: formSubmitURL,
            usernameField: usernameField ? usernameField.name : "",
            usernameValue: usernameField ? usernameField.value : "",
            passwordField: newPasswordField.name,
            passwordValue: newPasswordField.value,
            hasOldPasswordField: !!oldPasswordField
        });
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
            // Counterintuitively, form submit observers fire for content that 
            // may not be the content in this context.
            if (aWindow.top != content)
                return true;

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

        observe : function (aSubject, aTopic, aData) {
          this._pwmgr._debug    = Services.prefs.getBoolPref("signon.debug");
          this._pwmgr._remember = Services.prefs.getBoolPref("signon.rememberSignons");
        }
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
                case "DOMAutoComplete":
                case "blur":
                    var acInputField = event.target;
                    var acForm = acInputField.form;

                    // If the username is blank, bail out now -- we don't want
                    // _fillForm() to try filling in a login without a username
                    // to filter on (bug 471906).
                    if (!acInputField.value)
                        return;

                    // Make sure the username field _fillForm will use is the
                    // same field as the autocomplete was activated on. If
                    // not, the DOM has been altered and we'll just give up.
                    var [usernameField, passwordField, ignored] =
                        this._pwmgr._getFormFields(acForm, false);
                    if (usernameField == acInputField && passwordField) {
                        var actionOrigin = this._pwmgr._getActionOrigin(acForm);
                        var formOrigin = this._pwmgr._getPasswordOrigin(acForm.ownerDocument.documentURI);
                        var foundLogins = this._pwmgr._getPasswords([actionOrigin], formOrigin);
                        this._pwmgr._fillForm(acForm, true, true, true, foundLogins[actionOrigin]);
                    } else {
                        this._pwmgr.log("Oops, form changed before AC invoked");
                    }
                    return;

                case "pageshow":
                    // Only process when we need to
                    if (this._pwmgr._remember && event.target instanceof Ci.nsIDOMHTMLDocument)
                        this._pwmgr._fillDocument(event.target);
                    break;

                case "unload":
                    Services.prefs.removeObserver("signon.", this._pwmgr._observer);
                    Services.obs.removeObserver(this._pwmgr._observer, "earlyformsubmit");
                    break;

                default:
                    this._pwmgr.log("Oops! This event unexpected.");
                    return;
            }
        }
    }
};

loginManager.init();
