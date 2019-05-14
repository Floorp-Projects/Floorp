/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  DoorHanger: "resource://gre/modules/Prompt.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

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
  classID: Components.ID("97d12931-abe2-11df-94e2-0800200c9a66"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsILoginManagerPrompter]),

  _factory: null,
  _window: null,
  _debug: false, // mirrors signon.debug

  __strBundle: null, // String bundle for L10N
  get _strBundle() {
    if (!this.__strBundle) {
      this.__strBundle = {
        pwmgr: Services.strings.createBundle("chrome://browser/locale/passwordmgr.properties"),
        brand: Services.strings.createBundle("chrome://branding/locale/brand.properties"),
      };

      if (!this.__strBundle)
        throw new Error("String bundle for Login Manager not present!");
    }

    return this.__strBundle;
  },

  __ellipsis: null,
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
  log: function(message) {
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
  init: function(aWindow, aFactory) {
    this._window = aWindow;
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
  promptToSavePassword: function(aLogin, dismissed) {
    this._showSaveLoginNotification(aLogin, dismissed);
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
   * @param dismissed
   *        A boolean indicating if a prompt is dismissed by default.
   */
  _showLoginNotification: function(aBody, aButtons, aUsername, aPassword, dismissed = false) {
    let actionText = {
      text: aUsername,
      type: "EDIT",
      bundle: { username: aUsername,
      password: aPassword },
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
      actionText: actionText,
      dismissed,
    };

    let win = (this._browser && this._browser.contentWindow) || this._window;
    DoorHanger.show(win, aBody, "password", aButtons, options, "LOGIN");
  },

  /*
   * _showSaveLoginNotification
   *
   * Displays a notification doorhanger (rather than a popup), to allow the user to
   * save the specified login. This allows the user to see the results of
   * their login, and only save a login which they know worked.
   *
   */
  _showSaveLoginNotification: function(aLogin, dismissed) {
    let brandShortName = this._strBundle.brand.GetStringFromName("brandShortName");
    let notificationText  = this._getLocalizedString("saveLogin", [brandShortName]);

    // The callbacks in |buttons| have a closure to access the variables
    // in scope here; set one to |Services.logins| so we can get back to pwmgr
    // without a getService() call.
    var pwmgr = Services.logins;
    let promptHistogram = Services.telemetry.getHistogramById("PWMGR_PROMPT_REMEMBER_ACTION");

    var buttons = [
      {
        label: this._getLocalizedString("neverButton"),
        callback: function() {
          promptHistogram.add(PROMPT_NEVER);
          pwmgr.setLoginSavingEnabled(aLogin.hostname, false);
        },
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
        positive: true,
      },
    ];

    this._showLoginNotification(notificationText, buttons, aLogin.username, aLogin.password, dismissed);
  },

  /*
   * promptToChangePassword
   *
   * Called when we think we detect a password change for an existing
   * login, when the form being submitted contains multiple password
   * fields.
   *
   */
  promptToChangePassword: function(aOldLogin, aNewLogin, dismissed) {
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
  _showChangeLoginNotification: function(aOldLogin, aNewPassword, dismissed) {
    var notificationText;
    if (aOldLogin.username) {
      let displayUser = this._sanitizeUsername(aOldLogin.username);
      notificationText  = this._getLocalizedString("updatePassword", [displayUser]);
    } else {
      notificationText  = this._getLocalizedString("updatePasswordNoUser");
    }

    var self = this;
    let promptHistogram = Services.telemetry.getHistogramById("PWMGR_PROMPT_UPDATE_ACTION");

    var buttons = [
      {
        label: this._getLocalizedString("dontUpdateButton"),
        callback:  function() {
          promptHistogram.add(PROMPT_NOTNOW);
          // do nothing
        },
      },
      {
        label: this._getLocalizedString("updateButton"),
        callback:  function(checked, response) {
          let password = response ? response.password : aNewPassword;
          self._updateLogin(aOldLogin, password);

          promptHistogram.add(PROMPT_UPDATE);
        },
        positive: true,
      },
    ];

    this._showLoginNotification(notificationText, buttons, aOldLogin.username, aNewPassword, dismissed);
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
   */
  promptToChangePasswordWithUsernames: function(logins, aNewLogin) {
    var usernames = logins.map(l => l.username);
    var dialogText  = this._getLocalizedString("userSelectText2");
    var dialogTitle = this._getLocalizedString("passwordChangeTitle");
    var selectedIndex = { value: null };

    // If user selects ok, outparam.value is set to the index
    // of the selected username.
    var ok = Services.prompt.select(null,
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
  _updateLogin: function(login, newPassword) {
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
    Services.logins.modifyLogin(login, propBag);
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
  _getLocalizedString: function(key, formatArgs) {
    if (formatArgs)
      return this._strBundle.pwmgr.formatStringFromName(
        key, formatArgs, formatArgs.length);
    return this._strBundle.pwmgr.GetStringFromName(key);
  },

  /*
   * _sanitizeUsername
   *
   * Sanitizes the specified username, by stripping quotes and truncating if
   * it's too long. This helps prevent an evil site from messing with the
   * "save password?" prompt too much.
   */
  _sanitizeUsername: function(username) {
    if (username.length > 30) {
      username = username.substring(0, 30);
      username += this._ellipsis;
    }
    return username.replace(/['"]/g, "");
  },
}; // end of LoginManagerPrompter implementation


var component = [LoginManagerPrompter];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(component);
