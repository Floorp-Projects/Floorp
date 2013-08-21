/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["LoginManagerContent"];

const Ci = Components.interfaces;
const Cr = Components.results;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PrivateBrowsingUtils.jsm");

var gEnabled = false, gDebug = false; // these mirror signon.* prefs

function log(...pieces) {
    function generateLogMessage(args) {
        let strings = ['Login Manager (content):'];

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

    if (!gDebug)
        return;

    let message = generateLogMessage(pieces);
    dump(message + "\n");
    Services.console.logStringMessage(message);
}


var observer = {
    QueryInterface : XPCOMUtils.generateQI([Ci.nsIObserver,
                                            Ci.nsIFormSubmitObserver,
                                            Ci.nsISupportsWeakReference]),

    // nsIFormSubmitObserver
    notify : function (formElement, aWindow, actionURI) {
        log("observer notified for form submission.");

        // We're invoked before the content's |onsubmit| handlers, so we
        // can grab form data before it might be modified (see bug 257781).

        try {
            LoginManagerContent._onFormSubmit(formElement);
        } catch (e) {
            log("Caught error in onFormSubmit:", e);
        }

        return true; // Always return true, or form submit will be canceled.
    },

    onPrefChange : function() {
        gDebug = Services.prefs.getBoolPref("signon.debug");
        gEnabled = Services.prefs.getBoolPref("signon.rememberSignons");
    },
};

Services.obs.addObserver(observer, "earlyformsubmit", false);
var prefBranch = Services.prefs.getBranch("signon.");
prefBranch.addObserver("", observer.onPrefChange, false);

observer.onPrefChange(); // read initial values


var LoginManagerContent = {

    __formFillService : null, // FormFillController, for username autocompleting
    get _formFillService() {
        if (!this.__formFillService)
            this.__formFillService =
                            Cc["@mozilla.org/satchel/form-fill-controller;1"].
                            getService(Ci.nsIFormFillController);
        return this.__formFillService;
    },

    onContentLoaded : function (event) {
      if (!event.isTrusted)
          return;

      if (!gEnabled)
          return;

      let domDoc = event.target;

      // Only process things which might have HTML forms.
      if (!(domDoc instanceof Ci.nsIDOMHTMLDocument))
          return;

      this._fillDocument(domDoc);
    },


    /*
     * onUsernameInput
     *
     * Listens for DOMAutoComplete and blur events on an input field.
     */
    onUsernameInput : function(event) {
        if (!event.isTrusted)
            return;

        if (!gEnabled)
            return;

        var acInputField = event.target;

        // This is probably a bit over-conservatative.
        if (!(acInputField.ownerDocument instanceof Ci.nsIDOMHTMLDocument))
            return;

        if (!this._isUsernameFieldType(acInputField))
            return;

        var acForm = acInputField.form;
        if (!acForm)
            return;

        // If the username is blank, bail out now -- we don't want
        // fillForm() to try filling in a login without a username
        // to filter on (bug 471906).
        if (!acInputField.value)
            return;

        log("onUsernameInput from", event.type);

        // Make sure the username field fillForm will use is the
        // same field as the autocomplete was activated on.
        var [usernameField, passwordField, ignored] =
            this._getFormFields(acForm, false);
        if (usernameField == acInputField && passwordField) {
            // If the user has a master password but itsn't logged in, bail
            // out now to prevent annoying prompts.
            if (!Services.logins.isLoggedIn)
                return;

            this._fillForm(acForm, true, true, true, null);
        } else {
            // Ignore the event, it's for some input we don't care about.
        }
    },


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
            log("(form ignored -- no password fields.)");
            return null;
        } else if (pwFields.length > 3) {
            log("(form ignored -- too many password fields. [ got ",
                        pwFields.length, "])");
            return null;
        }

        return pwFields;
    },


    _isUsernameFieldType: function(element) {
        if (!(element instanceof Ci.nsIDOMHTMLInputElement))
            return false;

        let fieldType = (element.hasAttribute("type") ?
                         element.getAttribute("type").toLowerCase() :
                         element.type);
        if (fieldType == "text"  ||
            fieldType == "email" ||
            fieldType == "url"   ||
            fieldType == "tel"   ||
            fieldType == "number") {
            return true;
        }
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
            var element = form.elements[i];
            if (this._isUsernameFieldType(element)) {
                usernameField = element;
                break;
            }
        }

        if (!usernameField)
            log("(form -- no username field found)");


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
                log("(form ignored -- all 3 pw fields differ)");
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

        // For E10S this will need to move.
        function getPrompter(aWindow) {
            var prompterSvc = Cc["@mozilla.org/login-manager/prompter;1"].
                              createInstance(Ci.nsILoginManagerPrompter);
            prompterSvc.init(aWindow);
            return prompterSvc;
        }

        var doc = form.ownerDocument;
        var win = doc.defaultView;

        if (PrivateBrowsingUtils.isWindowPrivate(win)) {
            // We won't do anything in private browsing mode anyway,
            // so there's no need to perform further checks.
            log("(form submission ignored in private browsing mode)");
            return;
        }

        // If password saving is disabled (globally or for host), bail out now.
        if (!gEnabled)
            return;

        var hostname = LoginUtils._getPasswordOrigin(doc.documentURI);
        if (!hostname) {
            log("(form submission ignored -- invalid hostname)");
            return;
        }

        var formSubmitURL = LoginUtils._getActionOrigin(form)
        if (!Services.logins.getLoginSavingEnabled(hostname)) {
            log("(form submission ignored -- saving is disabled for:", hostname, ")");
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
                log("(form submission ignored -- autocomplete=off found)");
                return;
        }


        var formLogin = Cc["@mozilla.org/login-manager/loginInfo;1"].
                        createInstance(Ci.nsILoginInfo);
        formLogin.init(hostname, formSubmitURL, null,
                    (usernameField ? usernameField.value : ""),
                    newPasswordField.value,
                    (usernameField ? usernameField.name  : ""),
                    newPasswordField.name);

        // If we didn't find a username field, but seem to be changing a
        // password, allow the user to select from a list of applicable
        // logins to update the password for.
        if (!usernameField && oldPasswordField) {

            var logins = Services.logins.findLogins({}, hostname, formSubmitURL, null);

            if (logins.length == 0) {
                // Could prompt to save this as a new password-only login.
                // This seems uncommon, and might be wrong, so ignore.
                log("(no logins for this host -- pwchange ignored)");
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
        var logins = Services.logins.findLogins({}, hostname, formSubmitURL, null);

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
            log("Found an existing login matching this form submission");

            // Change password if needed.
            if (existingLogin.password != formLogin.password) {
                log("...passwords differ, prompting to change.");
                prompter = getPrompter(win);
                prompter.promptToChangePassword(existingLogin, formLogin);
            } else {
                // Update the lastUsed timestamp.
                var propBag = Cc["@mozilla.org/hash-property-bag;1"].
                              createInstance(Ci.nsIWritablePropertyBag);
                propBag.setProperty("timeLastUsed", Date.now());
                propBag.setProperty("timesUsedIncrement", 1);
                Services.logins.modifyLogin(existingLogin, propBag);
            }

            return;
        }


        // Prompt user to save login (via dialog or notification bar)
        prompter = getPrompter(win);
        prompter.promptToSavePassword(formLogin);
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

        var formOrigin = LoginUtils._getPasswordOrigin(doc.documentURI);

        // If there are no logins for this site, bail out now.
        if (!Services.logins.countLogins(formOrigin, "", null))
            return;

        // If we're currently displaying a master password prompt, defer
        // processing this document until the user handles the prompt.
        if (Services.logins.uiBusy) {
            log("deferring fillDoc for", doc.documentURI);
            let self = this;
            let observer = {
                QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsISupportsWeakReference]),

                observe: function (subject, topic, data) {
                    log("Got deferred fillDoc notification:", topic);
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
            doc.addEventListener("mozCleverClosureHack", observer);
            return;
        }

        log("fillDocument processing", forms.length, "forms on", doc.documentURI);

        var autofillForm = !PrivateBrowsingUtils.isWindowPrivate(doc.defaultView) &&
                           Services.prefs.getBoolPref("signon.autofillForms");
        var previousActionOrigin = null;
        var foundLogins = null;

        // Limit the number of forms we try to fill. If there are too many
        // forms, just fill some at the beginning and end of the page.
        const MAX_FORMS = 40; // assumed to be an even number
        var skip_from = -1, skip_to = -1;
        if (forms.length > MAX_FORMS) {
            log("fillDocument limiting number of forms filled to", MAX_FORMS);
            let chunk_size = MAX_FORMS / 2;
            skip_from = chunk_size;
            skip_to   = forms.length - chunk_size;
        }

        for (var i = 0; i < forms.length; i++) {
            // Skip some in the middle of the document if there were too many.
            if (i == skip_from)
              i = skip_to;

            var form = forms[i];

            // Only the actionOrigin might be changing, so if it's the same
            // as the last form on the page we can reuse the same logins.
            var actionOrigin = LoginUtils._getActionOrigin(form);
            if (actionOrigin != previousActionOrigin) {
                foundLogins = null;
                previousActionOrigin = actionOrigin;
            }
            log("_fillDocument processing form[", i, "]");
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
            log("not filling form, login fields disabled");
            return [false, foundLogins];
        }

        // Need to get a list of logins if we weren't given them
        if (foundLogins == null) {
            var formOrigin =
                LoginUtils._getPasswordOrigin(form.ownerDocument.documentURI);
            var actionOrigin = LoginUtils._getActionOrigin(form);
            foundLogins = Services.logins.findLogins({}, formOrigin, actionOrigin, null);
            log("found", foundLogins.length, "matching logins.");
        } else {
            log("reusing logins from last form.");
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
                    log("Ignored", l.username, "login: won't fit");

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
            this._formFillService.markAsLoginManagerField(usernameField);

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
            log("form not filled, has autocomplete=off");
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
                log("Password not filled. None of the stored logins match the username already present.");
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
                log("Multiple logins for form, so not filling any.");
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
            log("autofillForms=false but form can be filled; notified observers");
        } else if (selectedLogin && isFormDisabled) {
            // For when autocomplete is off, but we still have the information
            // to fill a form, we notify observers.
            didntFillReason = "autocompleteOff";
            Services.obs.notifyObservers(form, "passwordmgr-found-form", didntFillReason);
            log("autocomplete=off but form can be filled; notified observers");
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

};




LoginUtils = {
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

};
