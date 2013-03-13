/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

/* ==================== LoginManagerPrompter ==================== */
/*
 * LoginManagerPrompter
 *
 * Implements interfaces for prompting the user to enter/save/change auth info.
 *
 * nsILoginManagerPrompter: Used by Login Manager for saving/changing logins
 * found in HTML forms.
 */
function LoginManagerPrompter() {
}

LoginManagerPrompter.prototype = {

    classID : Components.ID("97d12931-abe2-11df-94e2-0800200c9a66"),
    QueryInterface : XPCOMUtils.generateQI([Ci.nsILoginManagerPrompter]),

    _factory       : null,
    _window       : null,
    _debug         : false, // mirrors signon.debug

    __pwmgr : null, // Password Manager service
    get _pwmgr() {
        if (!this.__pwmgr)
            this.__pwmgr = Cc["@mozilla.org/login-manager;1"].
                           getService(Ci.nsILoginManager);
        return this.__pwmgr;
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


    __ellipsis : null,
    get _ellipsis() {
        if (!this.__ellipsis) {
            this.__ellipsis = "\u2026";
            try {
                this.__ellipsis = Services.prefs.getComplexValue(
                                    "intl.ellipsis", Ci.nsIPrefLocalizedString).data;
            } catch (e) { }
        }
        return this.__ellipsis;
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
        Services.console.logStringMessage("Pwmgr Prompter: " + message);
    },


    /* ---------- nsILoginManagerPrompter prompts ---------- */




    /*
     * init
     *
     */
    init : function (aWindow, aFactory) {
        this._window = aWindow;
        this._factory = aFactory || null;

        var prefBranch = Services.prefs.getBranch("signon.");
        this._debug = prefBranch.getBoolPref("debug");
        this.log("===== initialized =====");
    },


    /*
     * promptToSavePassword
     *
     */
    promptToSavePassword : function (aLogin) {
        this._showSaveLoginNotification(aLogin);
    },


    /*
     * _showLoginNotification
     *
     * Displays a notification doorhanger.
     *
     */
    _showLoginNotification : function (aName, aText, aButtons) {
        this.log("Adding new " + aName + " notification bar");
        let notifyWin = this._window.top;
        let chromeWin = this._getChromeWindow(notifyWin).wrappedJSObject;
        let browser = chromeWin.BrowserApp.getBrowserForWindow(notifyWin);
        let tabID = chromeWin.BrowserApp.getTabForBrowser(browser).id;

        // The page we're going to hasn't loaded yet, so we want to persist
        // across the first location change.

        // Sites like Gmail perform a funky redirect dance before you end up
        // at the post-authentication page. I don't see a good way to
        // heuristically determine when to ignore such location changes, so
        // we'll try ignoring location changes based on a time interval.

        let options = {
            persistWhileVisible: true,
            timeout: Date.now() + 10000
        }

        var nativeWindow = this._getNativeWindow();
        if (nativeWindow)
            nativeWindow.doorhanger.show(aText, aName, aButtons, tabID, options);
    },


    /*
     * _showSaveLoginNotification
     *
     * Displays a notification doorhanger (rather than a popup), to allow the user to
     * save the specified login. This allows the user to see the results of
     * their login, and only save a login which they know worked.
     *
     */
    _showSaveLoginNotification : function (aLogin) {
        var displayHost = this._getShortDisplayHost(aLogin.hostname);
        var notificationText;
        if (aLogin.username) {
            var displayUser = this._sanitizeUsername(aLogin.username);
            notificationText  = this._getLocalizedString("savePassword", [displayUser, displayHost]);
        } else {
            notificationText  = this._getLocalizedString("savePasswordNoUser", [displayHost]);
        }

        // The callbacks in |buttons| have a closure to access the variables
        // in scope here; set one to |this._pwmgr| so we can get back to pwmgr
        // without a getService() call.
        var pwmgr = this._pwmgr;

        var buttons = [
            {
                label: this._getLocalizedString("saveButton"),
                callback: function() {
                    pwmgr.addLogin(aLogin);
                }
            },
            {
                label: this._getLocalizedString("dontSaveButton"),
                callback: function() {
                    // Don't set a permanent exception
                }
            }
        ];

        this._showLoginNotification("password-save", notificationText, buttons);
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
        this._showChangeLoginNotification(aOldLogin, aNewLogin.password);
    },

    /*
     * _showChangeLoginNotification
     *
     * Shows the Change Password notification doorhanger.
     *
     */
    _showChangeLoginNotification : function (aOldLogin, aNewPassword) {
        var notificationText;
        if (aOldLogin.username) {
            let displayUser = this._sanitizeUsername(aOldLogin.username);
            notificationText  = this._getLocalizedString("updatePassword", [displayUser]);
        } else {
            notificationText  = this._getLocalizedString("updatePasswordNoUser");
        }

        // The callbacks in |buttons| have a closure to access the variables
        // in scope here; set one to |this._pwmgr| so we can get back to pwmgr
        // without a getService() call.
        var self = this;

        var buttons = [
            {
                label: this._getLocalizedString("updateButton"),
                callback:  function() {
                    self._updateLogin(aOldLogin, aNewPassword);
                }
            },
            {
                label: this._getLocalizedString("dontUpdateButton"),
                callback:  function() {
                    // do nothing
                }
            }
        ];

        this._showLoginNotification("password-change", notificationText, buttons);
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
        var ok = this._promptService.select(null,
                                dialogTitle, dialogText,
                                usernames.length, usernames,
                                selectedIndex);
        if (ok) {
            // Now that we know which login to use, modify its password.
            var selectedLogin = logins[selectedIndex.value];
            this.log("Updating password for user " + selectedLogin.username);
            this._updateLogin(selectedLogin, aNewLogin.password);
        }
    },




    /* ---------- Internal Methods ---------- */




    /*
     * _updateLogin
     */
    _updateLogin : function (login, newPassword) {
        var now = Date.now();
        var propBag = Cc["@mozilla.org/hash-property-bag;1"].
                      createInstance(Ci.nsIWritablePropertyBag);
        if (newPassword) {
            propBag.setProperty("password", newPassword);
            // Explicitly set the password change time here (even though it would
            // be changed automatically), to ensure that it's exactly the same
            // value as timeLastUsed.
            propBag.setProperty("timePasswordChanged", now);
        }
        propBag.setProperty("timeLastUsed", now);
        propBag.setProperty("timesUsedIncrement", 1);
        this._pwmgr.modifyLogin(login, propBag);
    },

    /*
     * _getChromeWindow
     *
     * Given a content DOM window, returns the chrome window it's in.
     */
    _getChromeWindow: function (aWindow) {
        var chromeWin = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIWebNavigation)
                               .QueryInterface(Ci.nsIDocShell)
                               .chromeEventHandler.ownerDocument.defaultView;
        return chromeWin;
    },

    /*
     * _getNativeWindow
     *
     * Returns the NativeWindow to this prompter, or null if there isn't
     * a NativeWindow available (w/ error sent to logcat).
     */
    _getNativeWindow : function () {
        let nativeWindow = null;
        try {
            let notifyWin = this._window.top;
            let chromeWin = this._getChromeWindow(notifyWin).wrappedJSObject;
            if (chromeWin.NativeWindow) {
                nativeWindow = chromeWin.NativeWindow;
            } else {
                Cu.reportError("NativeWindow not available on window");
            }

        } catch (e) {
            // If any errors happen, just assume no native window helper.
            Cu.reportError("No NativeWindow available: " + e);
        }
        return nativeWindow;
    },

    /*
     * _getLocalizedString
     *
     * Can be called as:
     *   _getLocalizedString("key1");
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
     * _sanitizeUsername
     *
     * Sanitizes the specified username, by stripping quotes and truncating if
     * it's too long. This helps prevent an evil site from messing with the
     * "save password?" prompt too much.
     */
    _sanitizeUsername : function (username) {
        if (username.length > 30) {
            username = username.substring(0, 30);
            username += this._ellipsis;
        }
        return username.replace(/['"]/g, "");
    },


    /*
     * _getFormattedHostname
     *
     * The aURI parameter may either be a string uri, or an nsIURI instance.
     *
     * Returns the hostname to use in a nsILoginInfo object (for example,
     * "http://example.com").
     */
    _getFormattedHostname : function (aURI) {
        var uri;
        if (aURI instanceof Ci.nsIURI) {
            uri = aURI;
        } else {
            uri = Services.io.newURI(aURI, null, null);
        }
        var scheme = uri.scheme;

        var hostname = scheme + "://" + uri.host;

        // If the URI explicitly specified a port, only include it when
        // it's not the default. (We never want "http://foo.com:80")
        port = uri.port;
        if (port != -1) {
            var handler = Services.io.getProtocolHandler(scheme);
            if (port != handler.defaultPort)
                hostname += ":" + port;
        }

        return hostname;
    },


    /*
     * _getShortDisplayHost
     *
     * Converts a login's hostname field (a URL) to a short string for
     * prompting purposes. Eg, "http://foo.com" --> "foo.com", or
     * "ftp://www.site.co.uk" --> "site.co.uk".
     */
    _getShortDisplayHost: function (aURIString) {
        var displayHost;

        var eTLDService = Cc["@mozilla.org/network/effective-tld-service;1"].
                          getService(Ci.nsIEffectiveTLDService);
        var idnService = Cc["@mozilla.org/network/idn-service;1"].
                         getService(Ci.nsIIDNService);
        try {
            var uri = Services.io.newURI(aURIString, null, null);
            var baseDomain = eTLDService.getBaseDomain(uri);
            displayHost = idnService.convertToDisplayIDN(baseDomain, {});
        } catch (e) {
            this.log("_getShortDisplayHost couldn't process " + aURIString);
        }

        if (!displayHost)
            displayHost = aURIString;

        return displayHost;
    },

}; // end of LoginManagerPrompter implementation


var component = [LoginManagerPrompter];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(component);

