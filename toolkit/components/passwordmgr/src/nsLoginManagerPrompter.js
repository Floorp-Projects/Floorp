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

/*
 * LoginManagerPromptFactory
 *
 * Implements nsIPromptFactory
 *
 * Invoked by NS_NewAuthPrompter2()
 * [embedding/components/windowwatcher/src/nsPrompt.cpp]
 */
function LoginManagerPromptFactory() {}

LoginManagerPromptFactory.prototype = {

    classDescription : "LoginManagerPromptFactory",
    contractID : "@mozilla.org/passwordmanager/authpromptfactory;1",
    classID : Components.ID("{749e62f4-60ae-4569-a8a2-de78b649660e}"),
    QueryInterface : XPCOMUtils.generateQI([Ci.nsIPromptFactory]),

    getPrompt : function (aWindow, aIID) {
        var prompt = new LoginManagerPrompter().QueryInterface(aIID);
        prompt.init(aWindow);
        return prompt;
    }
}; // end of LoginManagerPromptFactory implementation




/* ==================== LoginManagerPrompter ==================== */




/*
 * LoginManagerPrompter
 *
 * Implements nsIAuthPrompt2 and nsILoginManagerPrompter.
 * nsIAuthPrompt2 usage is invoked by a channel for protocol-based
 * authentication (eg HTTP Authenticate, FTP login). nsILoginManagerPrompter
 * is invoked by Login Manager for saving/changing a login.
 */
function LoginManagerPrompter() {}

LoginManagerPrompter.prototype = {

    classDescription : "LoginManagerPrompter",
    contractID : "@mozilla.org/login-manager/prompter;1",
    classID : Components.ID("{8aa66d77-1bbb-45a6-991e-b8f47751c291}"),
    QueryInterface : XPCOMUtils.generateQI(
                        [Ci.nsIAuthPrompt2, Ci.nsILoginManagerPrompter]),

    _window        : null,
    _debug         : false, // mirrors signon.debug

    __pwmgr : null, // Password Manager service
    get _pwmgr() {
        if (!this.__pwmgr)
            this.__pwmgr = Cc["@mozilla.org/login-manager;1"].
                           getService(Ci.nsILoginManager);
        return this.__pwmgr;
    },

    __logService : null, // Console logging service, used for debugging.
    get _logService() {
        if (!this.__logService)
            this.__logService = Cc["@mozilla.org/consoleservice;1"].
                                getService(Ci.nsIConsoleService);
        return this.__logService;
    },

    __promptService : null, // Prompt service for user interaction
    get _promptService() {
        if (!this.__promptService)
            this.__promptService =
                Cc["@mozilla.org/embedcomp/prompt-service;1"].
                getService(Ci.nsIPromptService2);
        return this.__promptService;
    },


    __strBundle : null, // String bundle for L10N
    get _strBundle() {
        if (!this.__strBundle) {
            var bunService = Cc["@mozilla.org/intl/stringbundle;1"].
                             getService(Ci.nsIStringBundleService);
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
            var bunService = Cc["@mozilla.org/intl/stringbundle;1"].
                             getService(Ci.nsIStringBundleService);
            this.__brandBundle = bunService.createBundle(
                        "chrome://branding/locale/brand.properties");
            if (!this.__brandBundle)
                throw "Branding string bundle not present!";
        }

        return this.__brandBundle;
    },


    /*
     * log
     *
     * Internal function for logging debug messages to the Error Console window.
     */
    log : function (message) {
        if (!this._debug)
            return;

        dump("Pwmgr Prompter: " + message + "\n");
        this._logService.logStringMessage("Pwmgr Prompter: " + message);
    },




    /* ---------- nsIAuthPrompt2 prompts ---------- */




    /*
     * promptAuth
     *
     * Implementation of nsIAuthPrompt2.
     *
     * nsIChannel aChannel
     * int        aLevel
     * nsIAuthInformation aAuthInfo
     */
    promptAuth : function (aChannel, aLevel, aAuthInfo) {
        var selectedLogin = null;
        var checkbox = { value : false };
        var checkboxLabel = null;
        var epicfail = false;

        try {

            this.log("===== promptAuth called =====");

            // If the user submits a login but it fails, we need to remove the
            // notification bar that was displayed. Conveniently, the user will
            // be prompted for authentication again, which brings us here.
            var notifyBox = this._getNotifyBox();
            if (notifyBox)
                this._removeSaveLoginNotification(notifyBox);

            var [hostname, httpRealm] = this._getAuthTarget(aChannel, aAuthInfo);


            // Looks for existing logins to prefill the prompt with.
            var foundLogins = this._pwmgr.findLogins({},
                                        hostname, null, httpRealm);

            // XXX Can't select from multiple accounts yet. (bug 227632)
            if (foundLogins.length > 0) {
                selectedLogin = foundLogins[0];
                this._SetAuthInfo(aAuthInfo, selectedLogin.username,
                                             selectedLogin.password);
                checkbox.value = true;
            }

            var canRememberLogin = this._pwmgr.getLoginSavingEnabled(hostname);
        
            // if checkboxLabel is null, the checkbox won't be shown at all.
            if (canRememberLogin && !notifyBox)
                checkboxLabel = this._getLocalizedString("rememberPassword");
        } catch (e) {
            // Ignore any errors and display the prompt anyway.
            epicfail = true;
            Components.utils.reportError("LoginManagerPrompter: " +
                "Epic fail in promptAuth: " + e + "\n");
        }

        var ok = this._promptService.promptAuth(this._window, aChannel,
                                aLevel, aAuthInfo, checkboxLabel, checkbox);
        if (epicfail)
            return ok;

        try {
            // If there's a notification box, use it to allow the user to
            // determine if the login should be saved. If there isn't a
            // notification box, only save the login if the user set the
            // checkbox to do so.
            var rememberLogin = notifyBox ? canRememberLogin : checkbox.value;

            if (ok && rememberLogin) {
                var newLogin = Cc["@mozilla.org/login-manager/loginInfo;1"].
                               createInstance(Ci.nsILoginInfo);
                newLogin.init(hostname, null, httpRealm,
                              aAuthInfo.username, aAuthInfo.password,
                              "", "");

                // If we didn't find an existing login, or if the username
                // changed, save as a new login.
                if (!selectedLogin ||
                    aAuthInfo.username != selectedLogin.username) {

                    // add as new
                    this.log("New login seen for " + aAuthInfo.username +
                             " @ " + hostname + " (" + httpRealm + ")");
                    if (notifyBox)
                        this._showSaveLoginNotification(notifyBox, newLogin);
                    else
                        this._pwmgr.addLogin(newLogin);

                } else if (selectedLogin &&
                           aAuthInfo.password != selectedLogin.password) {

                    this.log("Updating password for " + aAuthInfo.username +
                             " @ " + hostname + " (" + httpRealm + ")");
                    // update password
                    this._pwmgr.modifyLogin(foundLogins[0], newLogin);

                } else {
                    this.log("Login unchanged, no further action needed.");
                    return ok;
                }
            }
        } catch (e) {
            Components.utils.reportError("LoginManagerPrompter: " +
                "Fail2 in promptAuth: " + e + "\n");
        }

        return ok;
    },

    asyncPromptAuth : function () {
        return NS_ERROR_NOT_IMPLEMENTED;
    },




    /* ---------- nsILoginManagerPrompter prompts ---------- */




    /*
     * init
     *
     */
    init : function (aWindow) {
        this._window = aWindow;

        var prefBranch = Cc["@mozilla.org/preferences-service;1"].
                         getService(Ci.nsIPrefService).getBranch("signon.");
        this._debug = prefBranch.getBoolPref("debug");
        this.log("===== initialized =====");
    },


    /*
     * promptToSavePassword
     *
     */
    promptToSavePassword : function (aLogin) {
        var notifyBox = this._getNotifyBox();

        if (notifyBox)
            this._showSaveLoginNotification(notifyBox, aLogin);
        else
            this._showSaveLoginDialog(aLogin);
    },


    /*
     * _showSaveLoginNotification
     *
     * Displays a notification bar (rather than a popup), to allow the user to
     * save the specified login. This allows the user to see the results of
     * their login, and only save a login which they know worked.
     *
     */
    _showSaveLoginNotification : function (aNotifyBox, aLogin) {

        // Ugh. We can't use the strings from the popup window, because they
        // have the access key marked in the string (eg "Mo&zilla"), along
        // with some weird rules for handling access keys that do not occur
        // in the string, for L10N. See commonDialog.js's setLabelForNode().
        var neverButtonText =
              this._getLocalizedString("notifyBarNeverForSiteButtonText");
        var neverButtonAccessKey =
              this._getLocalizedString("notifyBarNeverForSiteButtonAccessKey");
        var rememberButtonText =
              this._getLocalizedString("notifyBarRememberButtonText");
        var rememberButtonAccessKey =
              this._getLocalizedString("notifyBarRememberButtonAccessKey");

        var brandShortName =
              this._brandBundle.GetStringFromName("brandShortName");
        var notificationText  = this._getLocalizedString(
                                        "savePasswordText", [brandShortName]);

        // The callbacks in |buttons| have a closure to access the variables
        // in scope here; set one to |this._pwmgr| so we can get back to pwmgr
        // without a getService() call.
        var pwmgr = this._pwmgr;


        var buttons = [
            // "Remember" button
            {
                label:     rememberButtonText,
                accessKey: rememberButtonAccessKey,
                popup:     null,
                callback: function(aNotificationBar, aButton) {
                    pwmgr.addLogin(aLogin);
                }
            },

            // "Never for this site" button
            {
                label:     neverButtonText,
                accessKey: neverButtonAccessKey,
                popup:     null,
                callback: function(aNotificationBar, aButton) {
                    pwmgr.setLoginSavingEnabled(aLogin.hostname, false);
                }
            }

            // "Not now" button not needed, as notification bar isn't modal.
        ];


        var oldBar = aNotifyBox.getNotificationWithValue("password-save");
        const priority = aNotifyBox.PRIORITY_INFO_MEDIUM;

        this.log("Adding new save-password notification bar");
        var newBar = aNotifyBox.appendNotification(
                                notificationText, "password-save",
                                null, priority, buttons);

        // The page we're going to hasn't loaded yet, so we want to persist
        // across the first location change.
        newBar.ignoreFirstLocationChange = true;

        // Sites like Gmail perform a funky redirect dance before you end up
        // at the post-authentication page. I don't see a good way to
        // heuristically determine when to ignore such location changes, so
        // we'll try ignoring location changes based on a time interval.
        var now = Date.now() / 1000;
        newBar.ignoreLocationChangeTimeout = now + 10; // 10 seconds

        if (oldBar) {
            this.log("(...and removing old save-password notification bar)");
            aNotifyBox.removeNotification(oldBar);
        }
    },


    /*
     * _removeSaveLoginNotification
     *
     */
    _removeSaveLoginNotification : function (aNotifyBox) {

        var oldBar = aNotifyBox.getNotificationWithValue("password-save");

        if (oldBar) {
            this.log("Removing save-password notification bar.");
            aNotifyBox.removeNotification(oldBar);
        }
    },


    /*
     * _showSaveLoginDialog
     *
     * Called when we detect a new login in a form submission,
     * asks the user what to do.
     *
     */
    _showSaveLoginDialog : function (aLogin) {
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
        var userChoice = this._promptService.confirmEx(this._window,
                                            dialogTitle, dialogText,
                                            buttonFlags, rememberButtonText,
                                            notNowButtonText, neverButtonText,
                                            null, {});
        //  Returns:
        //   0 - Save the login
        //   1 - Ignore the login this time
        //   2 - Never save logins for this site
        if (userChoice == 2) {
            this.log("Disabling " + aLogin.hostname + " logins by request.");
            this._pwmgr.setLoginSavingEnabled(aLogin.hostname, false);
        } else if (userChoice == 0) {
            this.log("Saving login for " + aLogin.hostname);
            this._pwmgr.addLogin(aLogin);
        } else {
            // userChoice == 1 --> just ignore the login.
            this.log("Ignoring login.");
        }
    },


    /*
     * promptToChangePassword
     *
     * Called when we think we detect a password change for an existing
     * login, when the form being submitted contains multiple password
     * fields.
     *
     */
    promptToChangePassword : function (aOldLogin, aNewLogin) {
        const buttonFlags = Ci.nsIPrompt.STD_YES_NO_BUTTONS;

        var dialogText;
        if (aOldLogin.username)
            dialogText  = this._getLocalizedString(
                                    "passwordChangeText",
                                    [aOldLogin.username]);
        else
            dialogText  = this._getLocalizedString(
                                    "passwordChangeTextNoUser");

        var dialogTitle = this._getLocalizedString(
                                    "passwordChangeTitle");

        // returns 0 for yes, 1 for no.
        var ok = !this._promptService.confirmEx(this._window,
                                dialogTitle, dialogText, buttonFlags,
                                null, null, null,
                                null, {});
        if (ok) {
            this.log("Updating password for user " + aOldLogin.username);
            this._pwmgr.modifyLogin(aOldLogin, aNewLogin);
        }
    },


    /*
     * promptToChangePasswordWithUsernames
     *
     * Called when we detect a password change in a form submission, but we
     * don't know which existing login (username) it's for. Asks the user
     * to select a username and confirm the password change.
     *
     * Note: The caller doesn't know the username for aNewLogin, so this
     *       function fills in .username and .usernameField with the values
     *       from the login selected by the user.
     * 
     * Note; XPCOM stupidity: |count| is just |logins.length|.
     */
    promptToChangePasswordWithUsernames : function (logins, count, aNewLogin) {
        const buttonFlags = Ci.nsIPrompt.STD_YES_NO_BUTTONS;

        var usernames = logins.map(function (l) l.username);
        var dialogText  = this._getLocalizedString("userSelectText");
        var dialogTitle = this._getLocalizedString("passwordChangeTitle");
        var selectedIndex = { value: null };

        // If user selects ok, outparam.value is set to the index
        // of the selected username.
        var ok = this._promptService.select(this._window,
                                dialogTitle, dialogText,
                                usernames.length, usernames,
                                selectedIndex);
        if (ok) {
            // Now that we know which login to change the password for,
            // update the missing username info in the aNewLogin.

            var selectedLogin = logins[selectedIndex.value];

            this.log("Updating password for user " + selectedLogin.username);

            aNewLogin.username      = selectedLogin.username;
            aNewLogin.usernameField = selectedLogin.usernameField;

            this._pwmgr.modifyLogin(selectedLogin, aNewLogin);
        }
    },




    /* ---------- Internal Methods ---------- */




    /*
     * _getNotifyBox
     *
     * Returns the notification box to this prompter, or null if there isn't
     * a notification box available.
     */
    _getNotifyBox : function () {
        try {
            // Get topmost window, in case we're in a frame.
            var notifyWindow = this._window.top

            // Some sites pop up a temporary login window, when disappears
            // upon submission of credentials. We want to put the notification
            // bar in the opener window if this seems to be happening.
            if (notifyWindow.opener) {
                var chromeWin = notifyWindow
                                    .QueryInterface(Ci.nsIInterfaceRequestor)
                                    .getInterface(Ci.nsIWebNavigation)
                                    .QueryInterface(Ci.nsIDocShellTreeItem)
                                    .rootTreeItem
                                    .QueryInterface(Ci.nsIInterfaceRequestor)
                                    .getInterface(Ci.nsIDOMWindow);
                var chromeDoc = chromeWin.document.documentElement;

                // Check to see if the current window was opened with
                // chrome disabled, and if so use the opener window.
                if (chromeDoc.getAttribute("chromehidden")) {
                    this.log("Using opener window for notification bar.");
                    notifyWindow = notifyWindow.opener;
                }
            }


            // Find the <browser> which contains notifyWindow, by looking
            // through all the open windows and all the <browsers> in each.
            var wm = Cc["@mozilla.org/appshell/window-mediator;1"].
                     getService(Ci.nsIWindowMediator);
            var enumerator = wm.getEnumerator("navigator:browser");
            var tabbrowser = null;
            var foundBrowser = null;

            while (!foundBrowser && enumerator.hasMoreElements()) {
                var win = enumerator.getNext();
                tabbrowser = win.getBrowser(); 
                foundBrowser = tabbrowser.getBrowserForDocument(
                                                  notifyWindow.document);
            }

            // Return the notificationBox associated with the browser.
            if (foundBrowser)
                return tabbrowser.getNotificationBox(foundBrowser)

        } catch (e) {
            // If any errors happen, just assume no notification box.
            this.log("No notification box available: " + e)
        }

        return null;
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
    },


    /*
     * _getFormattedHostname
     *
     * Returns the hostname to use in a nsILoginInfo object (for example,
     * "http://example.com").
     */
    _getFormattedHostname : function (aURI) {
        var scheme = aURI.scheme;

        var hostname = scheme + "://" + aURI.host;

        // If the URI explicitly specified a port, only include it when
        // it's not the default. (We never want "http://foo.com:80")
        port = aURI.port;
        if (port != -1) {
            var ioService = Cc["@mozilla.org/network/io-service;1"].
                            getService(Ci.nsIIOService);
            var handler = ioService.getProtocolHandler(scheme);
            if (port != handler.defaultPort)
                hostname += ":" + port;
        }

        return hostname;
    },


    /*
     * _getAuthTarget
     *
     * Returns the hostname and realm for which authentication is being
     * requested, in the format expected to be used with nsILoginInfo.
     */
    _getAuthTarget : function (aChannel, aAuthInfo) {
        var hostname, realm;

        // If our proxy is demanding authentication, don't use the
        // channel's actual destination.
        if (aAuthInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY) {
            this.log("getAuthTarget is for proxy auth");
            if (!(aChannel instanceof Ci.nsIProxiedChannel))
                throw "proxy auth needs nsIProxiedChannel";

            var info = aChannel.proxyInfo;
            if (!info)
                throw "proxy auth needs nsIProxyInfo";

            // Proxies don't have a scheme, but we'll use "moz-proxy://"
            // so that it's more obvious what the login is for.
            var idnService = Cc["@mozilla.org/network/idn-service;1"].
                             getService(Ci.nsIIDNService);
            hostname = "moz-proxy://" +
                        idnService.convertUTF8toACE(info.host) +
                        ":" + info.port;
            realm = aAuthInfo.realm;
            if (!realm)
                realm = hostname;

            return [hostname, realm];
        }

        hostname = this._getFormattedHostname(aChannel.URI);

        // If a HTTP WWW-Authenticate header specified a realm, that value
        // will be available here. If it wasn't set or wasn't HTTP, we'll use
        // the formatted hostname instead.
        realm = aAuthInfo.realm;
        if (!realm)
            realm = hostname;

        return [hostname, realm];
    },

    /**
     * Given a username (possibly in DOMAIN\user form) and password, parses the
     * domain out of the username if necessary and sets domain, username and
     * password on the auth information object.
     */
    _SetAuthInfo : function (aAuthInfo, username, password) {
        var flags = aAuthInfo.flags;
        if (flags & Ci.nsIAuthInformation.NEED_DOMAIN) {
            // Domain is separated from username by a backslash
            var idx = username.indexOf("\\");
            if (idx == -1) {
                aAuthInfo.username = username;
            } else {
                aAuthInfo.domain   =  username.substring(0, idx);
                aAuthInfo.username =  username.substring(idx+1);
            }
        } else {
            aAuthInfo.username = username;
        }
        aAuthInfo.password = password;
    }

}; // end of LoginManagerPrompter implementation

var component = [LoginManagerPromptFactory, LoginManagerPrompter];
function NSGetModule(compMgr, fileSpec) {
    return XPCOMUtils.generateModule(component);
}


