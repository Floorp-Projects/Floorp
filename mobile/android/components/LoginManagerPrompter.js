/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

/* Constants for password prompt telemetry.
* Mirrored in nsLoginManagerPrompter.js */
const PROMPT_DISPLAYED = 0;

const PROMPT_ADD = 1;
const PROMPT_NOTNOW = 2;
const PROMPT_NEVER = 3;

const PROMPT_UPDATE = 1;

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
      this.__promptService = Cc["@mozilla.org/embedcomp/prompt-service;1"].
                             getService(Ci.nsIPromptService2);
    return this.__promptService;
  },

  __strBundle : null, // String bundle for L10N
  get _strBundle() {
    if (!this.__strBundle) {
      let bunService = Cc["@mozilla.org/intl/stringbundle;1"].
                       getService(Ci.nsIStringBundleService);
      this.__strBundle = {
        pwmgr : bunService.createBundle("chrome://passwordmgr/locale/passwordmgr.properties"),
        brand : bunService.createBundle("chrome://branding/locale/brand.properties")
      };

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
    this._chromeWindow = this._getChromeWindow(aWindow).wrappedJSObject;
    this._factory = aFactory || null;
    this._browser = null;

    var prefBranch = Services.prefs.getBranch("signon.");
    this._debug = prefBranch.getBoolPref("debug");
    this.log("===== initialized =====");
  },

  set browser(aBrowser) {
    this._browser = aBrowser;
  },

  // setting this attribute is ignored because Android does not consider
  // opener windows when displaying login notifications
  set opener(aOpener) { },

  /*
   * promptToSavePassword
   *
   */
  promptToSavePassword : function (aLogin) {
    this._showSaveLoginNotification(aLogin);
      Services.telemetry.getHistogramById("PWMGR_PROMPT_REMEMBER_ACTION").add(PROMPT_DISPLAYED);
    Services.obs.notifyObservers(aLogin, "passwordmgr-prompt-save");
  },

  /*
   * _showLoginNotification
   *
   * Displays a notification doorhanger.
   * @param aBody
   *        String message to be displayed in the doorhanger
   * @param aButtons
   *        Buttons to display with the doorhanger
   * @param aUsername
   *        Username string used in creating a doorhanger action
   * @param aPassword
   *        Password string used in creating a doorhanger action
   */
  _showLoginNotification : function (aBody, aButtons, aUsername, aPassword) {
    let tabID = this._chromeWindow.BrowserApp.getTabForBrowser(this._browser).id;

    let actionText = {
      text: aUsername,
      type: "EDIT",
      bundle: { username: aUsername,
      password: aPassword }
    };

    // The page we're going to hasn't loaded yet, so we want to persist
    // across the first location change.

    // Sites like Gmail perform a funky redirect dance before you end up
    // at the post-authentication page. I don't see a good way to
    // heuristically determine when to ignore such location changes, so
    // we'll try ignoring location changes based on a time interval.
    let options = {
      persistWhileVisible: true,
      timeout: Date.now() + 10000,
      actionText: actionText
    }

    var nativeWindow = this._getNativeWindow();
    if (nativeWindow)
      nativeWindow.doorhanger.show(aBody, "password", aButtons, tabID, options, "LOGIN");
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
    let brandShortName = this._strBundle.brand.GetStringFromName("brandShortName");
    let notificationText  = this._getLocalizedString("saveLogin", [brandShortName]);

    let username = aLogin.username ? this._sanitizeUsername(aLogin.username) : "";

    // The callbacks in |buttons| have a closure to access the variables
    // in scope here; set one to |this._pwmgr| so we can get back to pwmgr
    // without a getService() call.
    var pwmgr = this._pwmgr;
    let promptHistogram = Services.telemetry.getHistogramById("PWMGR_PROMPT_REMEMBER_ACTION");

    var buttons = [
      {
        label: this._getLocalizedString("neverButton"),
        callback: function() {
          promptHistogram.add(PROMPT_NEVER);
          pwmgr.setLoginSavingEnabled(aLogin.hostname, false);
        }
      },
      {
        label: this._getLocalizedString("rememberButton"),
        callback: function(checked, response) {
          if (response) {
            aLogin.username = response.username || aLogin.username;
            aLogin.password = response.password || aLogin.password;
          }
          pwmgr.addLogin(aLogin);
          promptHistogram.add(PROMPT_ADD);
        },
        positive: true
      }
    ];

    this._showLoginNotification(notificationText, buttons, aLogin.username, aLogin.password);
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
    Services.telemetry.getHistogramById("PWMGR_PROMPT_UPDATE_ACTION").add(PROMPT_DISPLAYED);
    let oldGUID = aOldLogin.QueryInterface(Ci.nsILoginMetaInfo).guid;
    Services.obs.notifyObservers(aNewLogin, "passwordmgr-prompt-change", oldGUID);
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
    let promptHistogram = Services.telemetry.getHistogramById("PWMGR_PROMPT_UPDATE_ACTION");

    var buttons = [
      {
        label: this._getLocalizedString("dontUpdateButton"),
        callback:  function() {
          promptHistogram.add(PROMPT_NOTNOW);
          // do nothing
        }
      },
      {
        label: this._getLocalizedString("updateButton"),
        callback:  function(checked, response) {
          let password = response ? response.password : aNewPassword;
          self._updateLogin(aOldLogin, password);

          promptHistogram.add(PROMPT_UPDATE);
        },
        positive: true
      }
    ];

    this._showLoginNotification(notificationText, buttons, aOldLogin.username, aNewPassword);
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

    var usernames = logins.map(l => l.username);
    var dialogText  = this._getLocalizedString("userSelectText2");
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
      let selectedLogin = logins[selectedIndex.value];
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
    if (aWindow instanceof Ci.nsIDOMChromeWindow)
      return aWindow;
    var chromeWin = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebNavigation)
      .QueryInterface(Ci.nsIDocShell)
      .chromeEventHandler.ownerGlobal;
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
      let chromeWin = this._chromeWindow;
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
      return this._strBundle.pwmgr.formatStringFromName(
        key, formatArgs, formatArgs.length);
    else
      return this._strBundle.pwmgr.GetStringFromName(key);
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
}; // end of LoginManagerPrompter implementation


var component = [LoginManagerPrompter];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(component);
