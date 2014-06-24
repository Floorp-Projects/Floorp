/* vim: set ts=4 sts=4 sw=4 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Ci = Components.interfaces;
const Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "UserAutoCompleteResult",
                                  "resource://gre/modules/LoginManagerContent.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AutoCompleteE10S",
                                  "resource://gre/modules/AutoCompleteE10S.jsm");

this.EXPORTED_SYMBOLS = [ "LoginManagerParent" ];

var gDebug;

function log(...pieces) {
    function generateLogMessage(args) {
        let strings = ['Login Manager (parent):'];

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

function prefChanged() {
    gDebug = Services.prefs.getBoolPref("signon.debug");
}

Services.prefs.addObserver("signon.debug", prefChanged, false);
prefChanged();

var LoginManagerParent = {
    init: function() {
        let mm = Cc["@mozilla.org/globalmessagemanager;1"]
                   .getService(Ci.nsIMessageListenerManager);
        mm.addMessageListener("RemoteLogins:findLogins", this);
        mm.addMessageListener("RemoteLogins:onFormSubmit", this);
        mm.addMessageListener("RemoteLogins:autoCompleteLogins", this);
    },

    receiveMessage: function (msg) {
        let data = msg.data;

        switch (msg.name) {
            case "RemoteLogins:findLogins": {
                // TODO Verify msg.target's principals against the formOrigin?
                this.findLogins(data.options.showMasterPassword,
                                data.formOrigin,
                                data.actionOrigin,
                                data.requestId,
                                msg.target.messageManager);
                break;
            }

            case "RemoteLogins:onFormSubmit": {
                // TODO Verify msg.target's principals against the formOrigin?
                this.onFormSubmit(data.hostname,
                                  data.formSubmitURL,
                                  data.usernameField,
                                  data.newPasswordField,
                                  data.oldPasswordField,
                                  msg.target);
                break;
            }

            case "RemoteLogins:autoCompleteLogins": {
                this.doAutocompleteSearch(data, msg.target);
                break;
            }
        }
    },

    findLogins: function(showMasterPassword, formOrigin, actionOrigin,
                         requestId, target) {
        if (!showMasterPassword && !Services.logins.isLoggedIn) {
            target.sendAsyncMessage("RemoteLogins:loginsFound",
                                    { requestId: requestId, logins: [] });
            return;
        }

        // If there are no logins for this site, bail out now.
        if (!Services.logins.countLogins(formOrigin, "", null)) {
            target.sendAsyncMessage("RemoteLogins:loginsFound",
                                    { requestId: requestId, logins: [] });
            return;
        }

        // If we're currently displaying a master password prompt, defer
        // processing this form until the user handles the prompt.
        if (Services.logins.uiBusy) {
            log("deferring onFormPassword for", formOrigin);
            let self = this;
            let observer = {
                QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                                       Ci.nsISupportsWeakReference]),

                observe: function (subject, topic, data) {
                    log("Got deferred onFormPassword notification:", topic);
                    // Only run observer once.
                    Services.obs.removeObserver(this, "passwordmgr-crypto-login");
                    Services.obs.removeObserver(this, "passwordmgr-crypto-loginCanceled");
                    if (topic == "passwordmgr-crypto-loginCanceled") {
                        target.sendAsyncMessage("RemoteLogins:loginsFound",
                                                { requestId: requestId, logins: [] });
                        return;
                    }

                    self.findLogins(showMasterPassword, formOrigin, actionOrigin,
                                    requestId, target);
                },
            };

            // Possible leak: it's possible that neither of these notifications
            // will fire, and if that happens, we'll leak the observer (and
            // never return). We should guarantee that at least one of these
            // will fire.
            // See bug XXX.
            Services.obs.addObserver(observer, "passwordmgr-crypto-login", false);
            Services.obs.addObserver(observer, "passwordmgr-crypto-loginCanceled", false);
            return;
        }

        var logins = Services.logins.findLogins({}, formOrigin, actionOrigin, null);
        target.sendAsyncMessage("RemoteLogins:loginsFound",
                                { requestId: requestId, logins: logins });
    },

    doAutocompleteSearch: function({ formOrigin, actionOrigin,
                                     searchString, previousResult,
                                     rect, requestId, remote }, target) {
        // Note: previousResult is a regular object, not an
        // nsIAutoCompleteResult.
        var result;
        var matchingLogins;

        let searchStringLower = searchString.toLowerCase();
        let logins;
        if (previousResult &&
            searchStringLower.startsWith(previousResult.searchString.toLowerCase())) {
            log("Using previous autocomplete result");

            // We have a list of results for a shorter search string, so just
            // filter them further based on the new search string.
            logins = previousResult.logins;
        } else {
            log("Creating new autocomplete search result.");

            // Grab the logins from the database.
            logins = Services.logins.findLogins({}, formOrigin, actionOrigin, null);
        }

        let matchingLogins = logins.filter(function(fullMatch) {
            let match = fullMatch.username;

            // Remove results that are too short, or have different prefix.
            // Also don't offer empty usernames as possible results.
            return match && match.toLowerCase().startsWith(searchStringLower);
        });

        // XXX In the E10S case, we're responsible for showing our own
        // autocomplete popup here because the autocomplete protocol hasn't
        // been e10s-ized yet. In the non-e10s case, our caller is responsible
        // for showing the autocomplete popup (via the regular
        // nsAutoCompleteController).
        if (remote) {
            result = new UserAutoCompleteResult(searchString, matchingLogins);
            AutoCompleteE10S.showPopupWithResults(target.ownerDocument.defaultView, rect, result);
        }

        target.messageManager.sendAsyncMessage("RemoteLogins:loginsAutoCompleted",
                                               { requestId: requestId,
                                                 logins: matchingLogins });
    },

    onFormSubmit: function(hostname, formSubmitURL,
                           usernameField, newPasswordField,
                           oldPasswordField,
                           target) {
        function getPrompter() {
            var prompterSvc = Cc["@mozilla.org/login-manager/prompter;1"].
                              createInstance(Ci.nsILoginManagerPrompter);
            // XXX For E10S, we don't want to use the browser's contentWindow
            // because it's in another process, so we use our chrome window as
            // the window parent (the content process is responsible for
            // making sure that its window is not in private browsing mode).
            // In the same-process case, we can simply use the content window.
            prompterSvc.init(target.isRemoteBrowser ?
                                target.ownerDocument.defaultView :
                                target.contentWindow);
            return prompterSvc;
        }

        if (!Services.logins.getLoginSavingEnabled(hostname)) {
            log("(form submission ignored -- saving is disabled for:", hostname, ")");
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

            var prompter = getPrompter();

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
                prompter = getPrompter();
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
        prompter = getPrompter();
        prompter.promptToSavePassword(formLogin);
    }
};
