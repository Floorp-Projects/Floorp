/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { PrivateBrowsingUtils } = ChromeUtils.import(
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
const { PromptUtils } = ChromeUtils.import(
  "resource://gre/modules/SharedPromptUtils.jsm"
);

/* eslint-disable block-scoped-var, no-var */

ChromeUtils.defineModuleGetter(
  this,
  "LoginHelper",
  "resource://gre/modules/LoginHelper.jsm"
);

const LoginInfo = Components.Constructor(
  "@mozilla.org/login-manager/loginInfo;1",
  "nsILoginInfo",
  "init"
);

const BRAND_BUNDLE = "chrome://branding/locale/brand.properties";

/**
 * The maximum age of the password in ms (using `timePasswordChanged`) whereby
 * a user can toggle the password visibility in a doorhanger to add a username to
 * a saved login.
 */
const VISIBILITY_TOGGLE_MAX_PW_AGE_MS = 2 * 60 * 1000; // 2 minutes

/**
 * Constants for password prompt telemetry.
 * Mirrored in mobile/android/components/LoginManagerPrompter.js */
const PROMPT_DISPLAYED = 0;

const PROMPT_ADD_OR_UPDATE = 1;
const PROMPT_NOTNOW = 2;
const PROMPT_NEVER = 3;

/**
 * A helper module to prevent modal auth prompt abuse.
 */
const PromptAbuseHelper = {
  getBaseDomainOrFallback(hostname) {
    try {
      return Services.eTLD.getBaseDomainFromHost(hostname);
    } catch (e) {
      return hostname;
    }
  },

  incrementPromptAbuseCounter(baseDomain, browser) {
    if (!browser) {
      return;
    }

    if (!browser.authPromptAbuseCounter) {
      browser.authPromptAbuseCounter = {};
    }

    if (!browser.authPromptAbuseCounter[baseDomain]) {
      browser.authPromptAbuseCounter[baseDomain] = 0;
    }

    browser.authPromptAbuseCounter[baseDomain] += 1;
  },

  resetPromptAbuseCounter(baseDomain, browser) {
    if (!browser || !browser.authPromptAbuseCounter) {
      return;
    }

    browser.authPromptAbuseCounter[baseDomain] = 0;
  },

  hasReachedAbuseLimit(baseDomain, browser) {
    if (!browser || !browser.authPromptAbuseCounter) {
      return false;
    }

    let abuseCounter = browser.authPromptAbuseCounter[baseDomain];
    // Allow for setting -1 to turn the feature off.
    if (this.abuseLimit < 0) {
      return false;
    }
    return !!abuseCounter && abuseCounter >= this.abuseLimit;
  },
};

XPCOMUtils.defineLazyPreferenceGetter(
  PromptAbuseHelper,
  "abuseLimit",
  "prompts.authentication_dialog_abuse_limit"
);

/**
 * Implements nsIPromptFactory
 *
 * Invoked by [toolkit/components/prompts/src/nsPrompter.js]
 */
function LoginManagerPromptFactory() {
  Services.obs.addObserver(this, "quit-application-granted", true);
  Services.obs.addObserver(this, "passwordmgr-crypto-login", true);
  Services.obs.addObserver(this, "passwordmgr-crypto-loginCanceled", true);
}

LoginManagerPromptFactory.prototype = {
  classID: Components.ID("{749e62f4-60ae-4569-a8a2-de78b649660e}"),
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIPromptFactory,
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ]),

  _asyncPrompts: {},
  _asyncPromptInProgress: false,

  observe(subject, topic, data) {
    this.log("Observed: " + topic);
    if (topic == "quit-application-granted") {
      this._cancelPendingPrompts();
    } else if (topic == "passwordmgr-crypto-login") {
      // Start processing the deferred prompters.
      this._doAsyncPrompt();
    } else if (topic == "passwordmgr-crypto-loginCanceled") {
      // User canceled a Master Password prompt, so go ahead and cancel
      // all pending auth prompts to avoid nagging over and over.
      this._cancelPendingPrompts();
    }
  },

  getPrompt(aWindow, aIID) {
    var prompt = new LoginManagerPrompter().QueryInterface(aIID);
    prompt.init(aWindow, this);
    return prompt;
  },

  _doAsyncPrompt() {
    if (this._asyncPromptInProgress) {
      this.log("_doAsyncPrompt bypassed, already in progress");
      return;
    }

    // Find the first prompt key we have in the queue
    var hashKey = null;
    for (hashKey in this._asyncPrompts) {
      break;
    }

    if (!hashKey) {
      this.log("_doAsyncPrompt:run bypassed, no prompts in the queue");
      return;
    }

    // If login manger has logins for this host, defer prompting if we're
    // already waiting on a master password entry.
    var prompt = this._asyncPrompts[hashKey];
    var prompter = prompt.prompter;
    var [origin, httpRealm] = prompter._getAuthTarget(
      prompt.channel,
      prompt.authInfo
    );
    var hasLogins = Services.logins.countLogins(origin, null, httpRealm) > 0;
    if (
      !hasLogins &&
      LoginHelper.schemeUpgrades &&
      origin.startsWith("https://")
    ) {
      let httpOrigin = origin.replace(/^https:\/\//, "http://");
      hasLogins = Services.logins.countLogins(httpOrigin, null, httpRealm) > 0;
    }
    if (hasLogins && Services.logins.uiBusy) {
      this.log("_doAsyncPrompt:run bypassed, master password UI busy");
      return;
    }

    var self = this;

    var runnable = {
      cancel: false,
      run() {
        var ok = false;
        if (!this.cancel) {
          try {
            self.log(
              "_doAsyncPrompt:run - performing the prompt for '" + hashKey + "'"
            );
            ok = prompter.promptAuth(
              prompt.channel,
              prompt.level,
              prompt.authInfo
            );
          } catch (e) {
            if (
              e instanceof Components.Exception &&
              e.result == Cr.NS_ERROR_NOT_AVAILABLE
            ) {
              self.log(
                "_doAsyncPrompt:run bypassed, UI is not available in this context"
              );
            } else {
              Cu.reportError(
                "LoginManagerPrompter: _doAsyncPrompt:run: " + e + "\n"
              );
            }
          }

          delete self._asyncPrompts[hashKey];
          prompt.inProgress = false;
          self._asyncPromptInProgress = false;
        }

        for (var consumer of prompt.consumers) {
          if (!consumer.callback) {
            // Not having a callback means that consumer didn't provide it
            // or canceled the notification
            continue;
          }

          self.log("Calling back to " + consumer.callback + " ok=" + ok);
          try {
            if (ok) {
              consumer.callback.onAuthAvailable(
                consumer.context,
                prompt.authInfo
              );
            } else {
              consumer.callback.onAuthCancelled(consumer.context, !this.cancel);
            }
          } catch (e) {
            /* Throw away exceptions caused by callback */
          }
        }
        self._doAsyncPrompt();
      },
    };

    this._asyncPromptInProgress = true;
    prompt.inProgress = true;

    Services.tm.dispatchToMainThread(runnable);
    this.log("_doAsyncPrompt:run dispatched");
  },

  _cancelPendingPrompts() {
    this.log("Canceling all pending prompts...");
    var asyncPrompts = this._asyncPrompts;
    this.__proto__._asyncPrompts = {};

    for (var hashKey in asyncPrompts) {
      let prompt = asyncPrompts[hashKey];
      // Watch out! If this prompt is currently prompting, let it handle
      // notifying the callbacks of success/failure, since it's already
      // asking the user for input. Reusing a callback can be crashy.
      if (prompt.inProgress) {
        this.log("skipping a prompt in progress");
        continue;
      }

      for (var consumer of prompt.consumers) {
        if (!consumer.callback) {
          continue;
        }

        this.log("Canceling async auth prompt callback " + consumer.callback);
        try {
          consumer.callback.onAuthCancelled(consumer.context, true);
        } catch (e) {
          /* Just ignore exceptions from the callback */
        }
      }
    }
  },
}; // end of LoginManagerPromptFactory implementation

XPCOMUtils.defineLazyGetter(
  this.LoginManagerPromptFactory.prototype,
  "log",
  () => {
    let logger = LoginHelper.createLogger("Login PromptFactory");
    return logger.log.bind(logger);
  }
);

/* ==================== LoginManagerPrompter ==================== */

/**
 * Implements interfaces for prompting the user to enter/save/change auth info.
 *
 * nsIAuthPrompt: Used by SeaMonkey, Thunderbird, but not Firefox.
 *
 * nsIAuthPrompt2: Is invoked by a channel for protocol-based authentication
 * (eg HTTP Authenticate, FTP login).
 *
 * nsILoginManagerPrompter: Used by Login Manager for saving/changing logins
 * found in HTML forms.
 */
function LoginManagerPrompter() {}

LoginManagerPrompter.prototype = {
  classID: Components.ID("{8aa66d77-1bbb-45a6-991e-b8f47751c291}"),
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIAuthPrompt,
    Ci.nsIAuthPrompt2,
    Ci.nsILoginManagerPrompter,
  ]),

  _factory: null,
  _chromeWindow: null,
  _browser: null,
  _openerBrowser: null,

  __strBundle: null, // String bundle for L10N
  get _strBundle() {
    if (!this.__strBundle) {
      this.__strBundle = Services.strings.createBundle(
        "chrome://passwordmgr/locale/passwordmgr.properties"
      );
      if (!this.__strBundle) {
        throw new Error("String bundle for Login Manager not present!");
      }
    }

    return this.__strBundle;
  },

  __ellipsis: null,
  get _ellipsis() {
    if (!this.__ellipsis) {
      this.__ellipsis = "\u2026";
      try {
        this.__ellipsis = Services.prefs.getComplexValue(
          "intl.ellipsis",
          Ci.nsIPrefLocalizedString
        ).data;
      } catch (e) {}
    }
    return this.__ellipsis;
  },

  // Whether we are in private browsing mode
  get _inPrivateBrowsing() {
    if (this._chromeWindow) {
      return PrivateBrowsingUtils.isWindowPrivate(this._chromeWindow);
    }
    // If we don't that we're in private browsing mode if the caller did
    // not provide a window.  The callers which really care about this
    // will indeed pass down a window to us, and for those who don't,
    // we can just assume that we don't want to save the entered login
    // information.
    this.log("We have no chromeWindow so assume we're in a private context");
    return true;
  },

  get _allowRememberLogin() {
    if (!this._inPrivateBrowsing) {
      return true;
    }
    return LoginHelper.privateBrowsingCaptureEnabled;
  },

  /* ---------- nsIAuthPrompt prompts ---------- */

  /**
   * Wrapper around the prompt service prompt. Saving random fields here
   * doesn't really make sense and therefore isn't implemented.
   */
  prompt(
    aDialogTitle,
    aText,
    aPasswordRealm,
    aSavePassword,
    aDefaultText,
    aResult
  ) {
    if (aSavePassword != Ci.nsIAuthPrompt.SAVE_PASSWORD_NEVER) {
      throw new Components.Exception(
        "prompt only supports SAVE_PASSWORD_NEVER",
        Cr.NS_ERROR_NOT_IMPLEMENTED
      );
    }

    this.log("===== prompt() called =====");

    if (aDefaultText) {
      aResult.value = aDefaultText;
    }

    return Services.prompt.prompt(
      this._chromeWindow,
      aDialogTitle,
      aText,
      aResult,
      null,
      {}
    );
  },

  /**
   * Looks up a username and password in the database. Will prompt the user
   * with a dialog, even if a username and password are found.
   */
  promptUsernameAndPassword(
    aDialogTitle,
    aText,
    aPasswordRealm,
    aSavePassword,
    aUsername,
    aPassword
  ) {
    this.log("===== promptUsernameAndPassword() called =====");

    if (aSavePassword == Ci.nsIAuthPrompt.SAVE_PASSWORD_FOR_SESSION) {
      throw new Components.Exception(
        "promptUsernameAndPassword doesn't support SAVE_PASSWORD_FOR_SESSION",
        Cr.NS_ERROR_NOT_IMPLEMENTED
      );
    }

    let foundLogins = null;
    var selectedLogin = null;
    var checkBox = { value: false };
    var checkBoxLabel = null;
    var [origin, realm, unused] = this._getRealmInfo(aPasswordRealm);

    // If origin is null, we can't save this login.
    if (origin) {
      var canRememberLogin = false;
      if (this._allowRememberLogin) {
        canRememberLogin =
          aSavePassword == Ci.nsIAuthPrompt.SAVE_PASSWORD_PERMANENTLY &&
          Services.logins.getLoginSavingEnabled(origin);
      }

      // if checkBoxLabel is null, the checkbox won't be shown at all.
      if (canRememberLogin) {
        checkBoxLabel = this._getLocalizedString("rememberPassword");
      }

      // Look for existing logins.
      foundLogins = Services.logins.findLogins(origin, null, realm);

      // XXX Like the original code, we can't deal with multiple
      // account selection. (bug 227632)
      if (foundLogins.length > 0) {
        selectedLogin = foundLogins[0];

        // If the caller provided a username, try to use it. If they
        // provided only a password, this will try to find a password-only
        // login (or return null if none exists).
        if (aUsername.value) {
          selectedLogin = this._repickSelectedLogin(
            foundLogins,
            aUsername.value
          );
        }

        if (selectedLogin) {
          checkBox.value = true;
          aUsername.value = selectedLogin.username;
          // If the caller provided a password, prefer it.
          if (!aPassword.value) {
            aPassword.value = selectedLogin.password;
          }
        }
      }
    }

    var ok = Services.prompt.promptUsernameAndPassword(
      this._chromeWindow,
      aDialogTitle,
      aText,
      aUsername,
      aPassword,
      checkBoxLabel,
      checkBox
    );

    if (!ok || !checkBox.value || !origin) {
      return ok;
    }

    if (!aPassword.value) {
      this.log("No password entered, so won't offer to save.");
      return ok;
    }

    // XXX We can't prompt with multiple logins yet (bug 227632), so
    // the entered login might correspond to an existing login
    // other than the one we originally selected.
    selectedLogin = this._repickSelectedLogin(foundLogins, aUsername.value);

    // If we didn't find an existing login, or if the username
    // changed, save as a new login.
    let newLogin = Cc["@mozilla.org/login-manager/loginInfo;1"].createInstance(
      Ci.nsILoginInfo
    );
    newLogin.init(
      origin,
      null,
      realm,
      aUsername.value,
      aPassword.value,
      "",
      ""
    );
    if (!selectedLogin) {
      // add as new
      this.log("New login seen for " + realm);
      Services.logins.addLogin(newLogin);
    } else if (aPassword.value != selectedLogin.password) {
      // update password
      this.log("Updating password for  " + realm);
      this._updateLogin(selectedLogin, newLogin);
    } else {
      this.log("Login unchanged, no further action needed.");
      this._updateLogin(selectedLogin);
    }

    return ok;
  },

  /**
   * If a password is found in the database for the password realm, it is
   * returned straight away without displaying a dialog.
   *
   * If a password is not found in the database, the user will be prompted
   * with a dialog with a text field and ok/cancel buttons. If the user
   * allows it, then the password will be saved in the database.
   */
  promptPassword(
    aDialogTitle,
    aText,
    aPasswordRealm,
    aSavePassword,
    aPassword
  ) {
    this.log("===== promptPassword called() =====");

    if (aSavePassword == Ci.nsIAuthPrompt.SAVE_PASSWORD_FOR_SESSION) {
      throw new Components.Exception(
        "promptPassword doesn't support SAVE_PASSWORD_FOR_SESSION",
        Cr.NS_ERROR_NOT_IMPLEMENTED
      );
    }

    var checkBox = { value: false };
    var checkBoxLabel = null;
    var [origin, realm, username] = this._getRealmInfo(aPasswordRealm);

    username = decodeURIComponent(username);

    // If origin is null, we can't save this login.
    if (origin && !this._inPrivateBrowsing) {
      var canRememberLogin =
        aSavePassword == Ci.nsIAuthPrompt.SAVE_PASSWORD_PERMANENTLY &&
        Services.logins.getLoginSavingEnabled(origin);

      // if checkBoxLabel is null, the checkbox won't be shown at all.
      if (canRememberLogin) {
        checkBoxLabel = this._getLocalizedString("rememberPassword");
      }

      if (!aPassword.value) {
        // Look for existing logins.
        var foundLogins = Services.logins.findLogins(origin, null, realm);

        // XXX Like the original code, we can't deal with multiple
        // account selection (bug 227632). We can deal with finding the
        // account based on the supplied username - but in this case we'll
        // just return the first match.
        for (var i = 0; i < foundLogins.length; ++i) {
          if (foundLogins[i].username == username) {
            aPassword.value = foundLogins[i].password;
            // wallet returned straight away, so this mimics that code
            return true;
          }
        }
      }
    }

    var ok = Services.prompt.promptPassword(
      this._chromeWindow,
      aDialogTitle,
      aText,
      aPassword,
      checkBoxLabel,
      checkBox
    );

    if (ok && checkBox.value && origin && aPassword.value) {
      var newLogin = Cc[
        "@mozilla.org/login-manager/loginInfo;1"
      ].createInstance(Ci.nsILoginInfo);
      newLogin.init(origin, null, realm, username, aPassword.value, "", "");

      this.log("New login seen for " + realm);

      Services.logins.addLogin(newLogin);
    }

    return ok;
  },

  /* ---------- nsIAuthPrompt helpers ---------- */

  /**
   * Given aRealmString, such as "http://user@example.com/foo", returns an
   * array of:
   *   - the formatted origin
   *   - the realm (origin + path)
   *   - the username, if present
   *
   * If aRealmString is in the format produced by NS_GetAuthKey for HTTP[S]
   * channels, e.g. "example.com:80 (httprealm)", null is returned for all
   * arguments to let callers know the login can't be saved because we don't
   * know whether it's http or https.
   */
  _getRealmInfo(aRealmString) {
    var httpRealm = /^.+ \(.+\)$/;
    if (httpRealm.test(aRealmString)) {
      return [null, null, null];
    }

    var uri = Services.io.newURI(aRealmString);
    var pathname = "";

    if (uri.pathQueryRef != "/") {
      pathname = uri.pathQueryRef;
    }

    var formattedOrigin = this._getFormattedOrigin(uri);

    return [formattedOrigin, formattedOrigin + pathname, uri.username];
  },

  /* ---------- nsIAuthPrompt2 prompts ---------- */

  /**
   * Implementation of nsIAuthPrompt2.
   *
   * @param {nsIChannel} aChannel
   * @param {int}        aLevel
   * @param {nsIAuthInformation} aAuthInfo
   */
  promptAuth(aChannel, aLevel, aAuthInfo) {
    var selectedLogin = null;
    var checkbox = { value: false };
    var checkboxLabel = null;
    var epicfail = false;
    var canAutologin = false;
    var notifyObj;
    var foundLogins;

    try {
      this.log("===== promptAuth called =====");

      // If the user submits a login but it fails, we need to remove the
      // notification prompt that was displayed. Conveniently, the user will
      // be prompted for authentication again, which brings us here.
      this._removeLoginNotifications();

      var [origin, httpRealm] = this._getAuthTarget(aChannel, aAuthInfo);

      // Looks for existing logins to prefill the prompt with.
      foundLogins = LoginHelper.searchLoginsWithObject({
        origin,
        httpRealm,
        schemeUpgrades: LoginHelper.schemeUpgrades,
      });
      this.log("found", foundLogins.length, "matching logins.");
      let resolveBy = ["scheme", "timePasswordChanged"];
      foundLogins = LoginHelper.dedupeLogins(
        foundLogins,
        ["username"],
        resolveBy,
        origin
      );
      this.log(foundLogins.length, "matching logins remain after deduping");

      // XXX Can't select from multiple accounts yet. (bug 227632)
      if (foundLogins.length > 0) {
        selectedLogin = foundLogins[0];
        this._SetAuthInfo(
          aAuthInfo,
          selectedLogin.username,
          selectedLogin.password
        );

        // Allow automatic proxy login
        if (
          aAuthInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY &&
          !(aAuthInfo.flags & Ci.nsIAuthInformation.PREVIOUS_FAILED) &&
          Services.prefs.getBoolPref("signon.autologin.proxy") &&
          !this._inPrivateBrowsing
        ) {
          this.log("Autologin enabled, skipping auth prompt.");
          canAutologin = true;
        }

        checkbox.value = true;
      }

      var canRememberLogin = Services.logins.getLoginSavingEnabled(origin);
      if (!this._allowRememberLogin) {
        canRememberLogin = false;
      }

      // if checkboxLabel is null, the checkbox won't be shown at all.
      notifyObj = this._getPopupNote();
      if (canRememberLogin && !notifyObj) {
        checkboxLabel = this._getLocalizedString("rememberPassword");
      }
    } catch (e) {
      // Ignore any errors and display the prompt anyway.
      epicfail = true;
      Cu.reportError(
        "LoginManagerPrompter: Epic fail in promptAuth: " + e + "\n"
      );
    }

    var ok = canAutologin;
    let browser = this._browser;
    let baseDomain;

    // We might not have a browser or browser.currentURI.host could fail
    // (e.g. on about:blank). Fall back to the subresource hostname in that case.
    try {
      let topLevelHost = browser.currentURI.host;
      baseDomain = PromptAbuseHelper.getBaseDomainOrFallback(topLevelHost);
    } catch (e) {
      baseDomain = PromptAbuseHelper.getBaseDomainOrFallback(origin);
    }

    if (!ok) {
      if (PromptAbuseHelper.hasReachedAbuseLimit(baseDomain, browser)) {
        this.log("Blocking auth dialog, due to exceeding dialog bloat limit");
        return false;
      }

      // Set up a counter for ensuring that the basic auth prompt can not
      // be abused for DOS-style attacks. With this counter, each eTLD+1
      // per browser will get a limited number of times a user can
      // cancel the prompt until we stop showing it.
      PromptAbuseHelper.incrementPromptAbuseCounter(baseDomain, browser);

      if (this._chromeWindow) {
        PromptUtils.fireDialogEvent(
          this._chromeWindow,
          "DOMWillOpenModalDialog",
          this._browser
        );
      }
      ok = Services.prompt.promptAuth(
        this._chromeWindow,
        aChannel,
        aLevel,
        aAuthInfo,
        checkboxLabel,
        checkbox
      );
    }

    let [username, password] = this._GetAuthInfo(aAuthInfo);

    // Reset the counter state if the user replied to a prompt and actually
    // tried to login (vs. simply clicking any button to get out).
    if (ok && (username || password)) {
      PromptAbuseHelper.resetPromptAbuseCounter(baseDomain, browser);
    }

    // If there's a notification prompt, use it to allow the user to
    // determine if the login should be saved. If there isn't a
    // notification prompt, only save the login if the user set the
    // checkbox to do so.
    var rememberLogin = notifyObj ? canRememberLogin : checkbox.value;
    if (!ok || !rememberLogin || epicfail) {
      return ok;
    }

    try {
      if (!password) {
        this.log("No password entered, so won't offer to save.");
        return ok;
      }

      // XXX We can't prompt with multiple logins yet (bug 227632), so
      // the entered login might correspond to an existing login
      // other than the one we originally selected.
      selectedLogin = this._repickSelectedLogin(foundLogins, username);

      // If we didn't find an existing login, or if the username
      // changed, save as a new login.
      let newLogin = Cc[
        "@mozilla.org/login-manager/loginInfo;1"
      ].createInstance(Ci.nsILoginInfo);
      newLogin.init(origin, null, httpRealm, username, password, "", "");
      if (!selectedLogin) {
        this.log(
          "New login seen for " +
            username +
            " @ " +
            origin +
            " (" +
            httpRealm +
            ")"
        );

        if (notifyObj) {
          this._showLoginCaptureDoorhanger(newLogin, "password-save", {
            dismissed: this._inPrivateBrowsing,
          });
          Services.obs.notifyObservers(newLogin, "passwordmgr-prompt-save");
        } else {
          Services.logins.addLogin(newLogin);
        }
      } else if (password != selectedLogin.password) {
        this.log(
          "Updating password for " +
            username +
            " @ " +
            origin +
            " (" +
            httpRealm +
            ")"
        );
        if (notifyObj) {
          this._showChangeLoginNotification(notifyObj, selectedLogin, newLogin);
        } else {
          this._updateLogin(selectedLogin, newLogin);
        }
      } else {
        this.log("Login unchanged, no further action needed.");
        this._updateLogin(selectedLogin);
      }
    } catch (e) {
      Cu.reportError("LoginManagerPrompter: Fail2 in promptAuth: " + e + "\n");
    }

    return ok;
  },

  asyncPromptAuth(aChannel, aCallback, aContext, aLevel, aAuthInfo) {
    var cancelable = null;

    try {
      this.log("===== asyncPromptAuth called =====");

      // If the user submits a login but it fails, we need to remove the
      // notification prompt that was displayed. Conveniently, the user will
      // be prompted for authentication again, which brings us here.
      this._removeLoginNotifications();

      cancelable = this._newAsyncPromptConsumer(aCallback, aContext);

      var [origin, httpRealm] = this._getAuthTarget(aChannel, aAuthInfo);

      var hashKey = aLevel + "|" + origin + "|" + httpRealm;
      this.log("Async prompt key = " + hashKey);
      var asyncPrompt = this._factory._asyncPrompts[hashKey];
      if (asyncPrompt) {
        this.log(
          "Prompt bound to an existing one in the queue, callback = " +
            aCallback
        );
        asyncPrompt.consumers.push(cancelable);
        return cancelable;
      }

      this.log("Adding new prompt to the queue, callback = " + aCallback);
      asyncPrompt = {
        consumers: [cancelable],
        channel: aChannel,
        authInfo: aAuthInfo,
        level: aLevel,
        inProgress: false,
        prompter: this,
      };

      this._factory._asyncPrompts[hashKey] = asyncPrompt;
      this._factory._doAsyncPrompt();
    } catch (e) {
      Cu.reportError(
        "LoginManagerPrompter: " +
          "asyncPromptAuth: " +
          e +
          "\nFalling back to promptAuth\n"
      );
      // Fail the prompt operation to let the consumer fall back
      // to synchronous promptAuth method
      throw e;
    }

    return cancelable;
  },

  /* ---------- nsILoginManagerPrompter prompts ---------- */

  init(aWindow = null, aFactory = null) {
    if (!aWindow) {
      // There may be no applicable window e.g. in a Sandbox or JSM.
      this._chromeWindow = null;
      this._browser = null;
    } else if (aWindow.isChromeWindow) {
      this._chromeWindow = aWindow;
      // needs to be set explicitly using setBrowser
      this._browser = null;
    } else {
      let { win, browser } = this._getChromeWindow(aWindow);
      this._chromeWindow = win;
      this._browser = browser;
    }
    this._openerBrowser = null;
    this._factory = aFactory || null;

    this.log("===== initialized =====");
  },

  set browser(aBrowser) {
    this._browser = aBrowser;
  },

  set openerBrowser(aOpenerBrowser) {
    this._openerBrowser = aOpenerBrowser;
  },

  promptToSavePassword(aLogin, dismissed = false, notifySaved = false) {
    this.log("promptToSavePassword");
    var notifyObj = this._getPopupNote();
    if (notifyObj) {
      this._showLoginCaptureDoorhanger(
        aLogin,
        "password-save",
        {
          dismissed: this._inPrivateBrowsing || dismissed,
          extraAttr: notifySaved ? "attention" : "",
        },
        {
          notifySaved,
        }
      );
      Services.obs.notifyObservers(aLogin, "passwordmgr-prompt-save");
    } else {
      this._showSaveLoginDialog(aLogin);
    }
  },

  /**
   * Displays the PopupNotifications.jsm doorhanger for password save or change.
   *
   * @param {nsILoginInfo} login
   *        Login to save or change. For changes, this login should contain the
   *        new password and/or username
   * @param {string} type
   *        This is "password-save" or "password-change" depending on the
   *        original notification type. This is used for telemetry and tests.
   * @param {object} showOptions
   *        Options to pass along to PopupNotifications.show().
   * @param {bool} [options.notifySaved = false]
   *        Whether to indicate to the user that the login was already saved.
   * @param {string} [options.messageStringID = undefined]
   *        An optional string ID to override the default message.
   * @param {string} [options.autoSavedLoginGuid = ""]
   *        A string guid value for the old login to be removed if the changes
   *        match it to an different login
   */
  _showLoginCaptureDoorhanger(
    login,
    type,
    showOptions = {},
    { notifySaved = false, messageStringID, autoSavedLoginGuid = "" } = {}
  ) {
    let { browser } = this._getNotifyWindow();
    if (!browser) {
      return;
    }
    this.log(
      `_showLoginCaptureDoorhanger, got autoSavedLoginGuid: ${autoSavedLoginGuid}`
    );

    let saveMsgNames = {
      prompt: login.username === "" ? "saveLoginMsgNoUser" : "saveLoginMsg",
      buttonLabel: "saveLoginButtonAllow.label",
      buttonAccessKey: "saveLoginButtonAllow.accesskey",
      secondaryButtonLabel: "saveLoginButtonDeny.label",
      secondaryButtonAccessKey: "saveLoginButtonDeny.accesskey",
    };

    let changeMsgNames = {
      prompt: login.username === "" ? "updateLoginMsgNoUser" : "updateLoginMsg",
      buttonLabel: "updateLoginButtonText",
      buttonAccessKey: "updateLoginButtonAccessKey",
      secondaryButtonLabel: "updateLoginButtonDeny.label",
      secondaryButtonAccessKey: "updateLoginButtonDeny.accesskey",
    };

    let initialMsgNames =
      type == "password-save" ? saveMsgNames : changeMsgNames;

    if (messageStringID) {
      changeMsgNames.prompt = messageStringID;
    }

    let brandBundle = Services.strings.createBundle(BRAND_BUNDLE);
    let brandShortName = brandBundle.GetStringFromName("brandShortName");
    let host = this._getShortDisplayHost(login.origin);
    let promptMsg =
      type == "password-save"
        ? this._getLocalizedString(saveMsgNames.prompt, [brandShortName, host])
        : this._getLocalizedString(changeMsgNames.prompt);

    let histogramName =
      type == "password-save"
        ? "PWMGR_PROMPT_REMEMBER_ACTION"
        : "PWMGR_PROMPT_UPDATE_ACTION";
    let histogram = Services.telemetry.getHistogramById(histogramName);
    histogram.add(PROMPT_DISPLAYED);
    Services.obs.notifyObservers(
      null,
      "weave:telemetry:histogram",
      histogramName
    );

    let chromeDoc = browser.ownerDocument;

    let currentNotification;

    let updateButtonStatus = element => {
      let mainActionButton = element.button;
      // Disable the main button inside the menu-button if the password field is empty.
      if (login.password.length == 0) {
        mainActionButton.setAttribute("disabled", true);
        chromeDoc
          .getElementById("password-notification-password")
          .classList.add("popup-notification-invalid-input");
      } else {
        mainActionButton.removeAttribute("disabled");
        chromeDoc
          .getElementById("password-notification-password")
          .classList.remove("popup-notification-invalid-input");
      }
    };

    let updateButtonLabel = () => {
      if (!currentNotification) {
        Cu.reportError("updateButtonLabel, no currentNotification");
      }
      let foundLogins = LoginHelper.searchLoginsWithObject({
        formActionOrigin: login.formActionOrigin,
        origin: login.origin,
        httpRealm: login.httpRealm,
        schemeUpgrades: LoginHelper.schemeUpgrades,
      });

      let logins = this._filterUpdatableLogins(
        login,
        foundLogins,
        autoSavedLoginGuid
      );
      let msgNames = logins.length == 0 ? saveMsgNames : changeMsgNames;

      // Update the label based on whether this will be a new login or not.
      let label = this._getLocalizedString(msgNames.buttonLabel);
      let accessKey = this._getLocalizedString(msgNames.buttonAccessKey);

      // Update the labels for the next time the panel is opened.
      currentNotification.mainAction.label = label;
      currentNotification.mainAction.accessKey = accessKey;

      // Update the labels in real time if the notification is displayed.
      let element = [...currentNotification.owner.panel.childNodes].find(
        n => n.notification == currentNotification
      );
      if (element) {
        element.setAttribute("buttonlabel", label);
        element.setAttribute("buttonaccesskey", accessKey);
        updateButtonStatus(element);
      }
    };

    let writeDataToUI = () => {
      // setAttribute is used in addition to setting the property since the
      // <textbox> binding may not be attached yet.
      chromeDoc
        .getElementById("password-notification-username")
        .setAttribute("placeholder", usernamePlaceholder);
      let nameField = chromeDoc.getElementById(
        "password-notification-username"
      );
      nameField.setAttribute("value", login.username);
      nameField.value = login.username;

      let toggleCheckbox = chromeDoc.getElementById(
        "password-notification-visibilityToggle"
      );
      toggleCheckbox.removeAttribute("checked");
      let passwordField = chromeDoc.getElementById(
        "password-notification-password"
      );
      // Ensure the type is reset so the field is masked.
      passwordField.setAttribute("type", "password");
      passwordField.setAttribute("value", login.password);
      passwordField.value = login.password;
      updateButtonLabel();
    };

    let readDataFromUI = () => {
      login.username = chromeDoc.getElementById(
        "password-notification-username"
      ).value;
      login.password = chromeDoc.getElementById(
        "password-notification-password"
      ).value;
    };

    let onInput = () => {
      readDataFromUI();
      updateButtonLabel();
    };

    let onVisibilityToggle = commandEvent => {
      let passwordField = chromeDoc.getElementById(
        "password-notification-password"
      );
      // Gets the caret position before changing the type of the textbox
      let selectionStart = passwordField.selectionStart;
      let selectionEnd = passwordField.selectionEnd;
      passwordField.setAttribute(
        "type",
        commandEvent.target.checked ? "" : "password"
      );
      if (!passwordField.hasAttribute("focused")) {
        return;
      }
      passwordField.selectionStart = selectionStart;
      passwordField.selectionEnd = selectionEnd;
    };

    let persistData = () => {
      let foundLogins = LoginHelper.searchLoginsWithObject({
        formActionOrigin: login.formActionOrigin,
        origin: login.origin,
        httpRealm: login.httpRealm,
        schemeUpgrades: LoginHelper.schemeUpgrades,
      });

      let logins = this._filterUpdatableLogins(
        login,
        foundLogins,
        autoSavedLoginGuid
      );
      let resolveBy = ["scheme", "timePasswordChanged"];
      logins = LoginHelper.dedupeLogins(
        logins,
        ["username"],
        resolveBy,
        login.origin
      );
      // sort exact username matches to the top
      logins.sort(l => (l.username == login.username ? -1 : 1));

      this.log(`persistData: Matched ${logins.length} logins`);

      let loginToRemove;
      let loginToUpdate = logins.shift();

      if (logins.length && logins[0].guid == autoSavedLoginGuid) {
        loginToRemove = logins.shift();
      }
      if (logins.length) {
        this.log(
          "multiple logins, loginToRemove:",
          loginToRemove && loginToRemove.guid
        );
        Cu.reportError("Unexpected match of multiple logins.");
        return;
      }

      if (!loginToUpdate) {
        // Create a new login, don't update an original.
        // The original login we have been provided with might have its own
        // metadata, but we don't want it propagated to the newly created one.
        Services.logins.addLogin(
          new LoginInfo(
            login.origin,
            login.formActionOrigin,
            login.httpRealm,
            login.username,
            login.password,
            login.usernameField,
            login.passwordField
          )
        );
      } else if (
        loginToUpdate.password == login.password &&
        loginToUpdate.username == login.username
      ) {
        // We only want to touch the login's use count and last used time.
        this.log("persistData: Touch matched login", loginToUpdate.guid);
        this._updateLogin(loginToUpdate);
      } else {
        this.log("persistData: Update matched login", loginToUpdate.guid);
        this._updateLogin(loginToUpdate, login);
        // notify that this auto-saved login been merged
        if (loginToRemove && loginToRemove.guid == autoSavedLoginGuid) {
          Services.obs.notifyObservers(
            loginToRemove,
            "passwordmgr-autosaved-login-merged"
          );
        }
      }

      if (loginToRemove) {
        this.log("persistData: removing login", loginToRemove.guid);
        Services.logins.removeLogin(loginToRemove);
      }
    };

    // The main action is the "Save" or "Update" button.
    let mainAction = {
      label: this._getLocalizedString(initialMsgNames.buttonLabel),
      accessKey: this._getLocalizedString(initialMsgNames.buttonAccessKey),
      callback: () => {
        histogram.add(PROMPT_ADD_OR_UPDATE);
        if (histogramName == "PWMGR_PROMPT_REMEMBER_ACTION") {
          Services.obs.notifyObservers(browser, "LoginStats:NewSavedPassword");
        }
        readDataFromUI();
        persistData();
        Services.obs.notifyObservers(
          null,
          "weave:telemetry:histogram",
          histogramName
        );
        browser.focus();
      },
    };

    let secondaryActions = [
      {
        label: this._getLocalizedString(initialMsgNames.secondaryButtonLabel),
        accessKey: this._getLocalizedString(
          initialMsgNames.secondaryButtonAccessKey
        ),
        callback: () => {
          histogram.add(PROMPT_NOTNOW);
          Services.obs.notifyObservers(
            null,
            "weave:telemetry:histogram",
            histogramName
          );
          browser.focus();
        },
      },
    ];
    // Include a "Never for this site" button when saving a new password.
    if (type == "password-save") {
      secondaryActions.push({
        label: this._getLocalizedString("saveLoginButtonNever.label"),
        accessKey: this._getLocalizedString("saveLoginButtonNever.accesskey"),
        callback: () => {
          histogram.add(PROMPT_NEVER);
          Services.obs.notifyObservers(
            null,
            "weave:telemetry:histogram",
            histogramName
          );
          Services.logins.setLoginSavingEnabled(login.origin, false);
          browser.focus();
        },
      });
    }

    let usernamePlaceholder = this._getLocalizedString("noUsernamePlaceholder");
    let togglePasswordLabel = this._getLocalizedString("togglePasswordLabel");
    let togglePasswordAccessKey = this._getLocalizedString(
      "togglePasswordAccessKey2"
    );

    let popupNote = this._getPopupNote();
    let notificationID = "password";
    popupNote.show(
      browser,
      notificationID,
      promptMsg,
      "password-notification-icon",
      mainAction,
      secondaryActions,
      Object.assign(
        {
          timeout: Date.now() + 10000,
          persistWhileVisible: true,
          passwordNotificationType: type,
          hideClose: true,
          eventCallback(topic) {
            switch (topic) {
              case "showing":
                currentNotification = this;
                chromeDoc
                  .getElementById("password-notification-password")
                  .removeAttribute("focused");
                chromeDoc
                  .getElementById("password-notification-username")
                  .removeAttribute("focused");
                chromeDoc
                  .getElementById("password-notification-username")
                  .addEventListener("input", onInput);
                chromeDoc
                  .getElementById("password-notification-password")
                  .addEventListener("input", onInput);
                let toggleBtn = chromeDoc.getElementById(
                  "password-notification-visibilityToggle"
                );

                if (
                  Services.prefs.getBoolPref(
                    "signon.rememberSignons.visibilityToggle"
                  )
                ) {
                  toggleBtn.addEventListener("command", onVisibilityToggle);
                  toggleBtn.setAttribute("label", togglePasswordLabel);
                  toggleBtn.setAttribute("accesskey", togglePasswordAccessKey);
                  let hideToggle =
                    LoginHelper.isMasterPasswordSet() ||
                    // Dismissed-by-default prompts should still show the toggle.
                    (this.timeShown && this.wasDismissed) ||
                    // If we are only adding a username then the password is
                    // one that is already saved and we don't want to reveal
                    // it as the submitter of this form may not be the account
                    // owner, they may just be using the saved password.
                    (messageStringID == "updateLoginMsgAddUsername" &&
                      login.timePasswordChanged <
                        Date.now() - VISIBILITY_TOGGLE_MAX_PW_AGE_MS);
                  toggleBtn.setAttribute("hidden", hideToggle);
                }
                break;
              case "shown": {
                writeDataToUI();
                let anchorIcon = this.anchorElement;
                if (anchorIcon && this.options.extraAttr == "attention") {
                  anchorIcon.removeAttribute("extraAttr");
                  delete this.options.extraAttr;
                }
                break;
              }
              case "dismissed":
                this.wasDismissed = true;
                readDataFromUI();
              // Fall through.
              case "removed":
                currentNotification = null;
                chromeDoc
                  .getElementById("password-notification-username")
                  .removeEventListener("input", onInput);
                chromeDoc
                  .getElementById("password-notification-password")
                  .removeEventListener("input", onInput);
                chromeDoc
                  .getElementById("password-notification-visibilityToggle")
                  .removeEventListener("command", onVisibilityToggle);
                break;
            }
            return false;
          },
        },
        showOptions
      )
    );

    if (notifySaved) {
      let notification = popupNote.getNotification(notificationID);
      let anchor = notification.anchorElement;
      anchor.ownerGlobal.ConfirmationHint.show(anchor, "passwordSaved");
    }
  },

  _removeLoginNotifications() {
    var popupNote = this._getPopupNote();
    if (popupNote) {
      popupNote = popupNote.getNotification("password");
    }
    if (popupNote) {
      popupNote.remove();
    }
  },

  /**
   * Called when we detect a new login in a form submission,
   * asks the user what to do.
   */
  _showSaveLoginDialog(aLogin) {
    const buttonFlags =
      Ci.nsIPrompt.BUTTON_POS_1_DEFAULT +
      Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_0 +
      Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_1 +
      Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_2;

    var displayHost = this._getShortDisplayHost(aLogin.origin);

    var dialogText;
    if (aLogin.username) {
      var displayUser = this._sanitizeUsername(aLogin.username);
      dialogText = this._getLocalizedString("rememberPasswordMsg", [
        displayUser,
        displayHost,
      ]);
    } else {
      dialogText = this._getLocalizedString("rememberPasswordMsgNoUsername", [
        displayHost,
      ]);
    }
    var dialogTitle = this._getLocalizedString("savePasswordTitle");
    var neverButtonText = this._getLocalizedString("neverForSiteButtonText");
    var rememberButtonText = this._getLocalizedString("rememberButtonText");
    var notNowButtonText = this._getLocalizedString("notNowButtonText");

    this.log("Prompting user to save/ignore login");
    var userChoice = Services.prompt.confirmEx(
      this._chromeWindow,
      dialogTitle,
      dialogText,
      buttonFlags,
      rememberButtonText,
      notNowButtonText,
      neverButtonText,
      null,
      {}
    );
    //  Returns:
    //   0 - Save the login
    //   1 - Ignore the login this time
    //   2 - Never save logins for this site
    if (userChoice == 2) {
      this.log("Disabling " + aLogin.origin + " logins by request.");
      Services.logins.setLoginSavingEnabled(aLogin.origin, false);
    } else if (userChoice == 0) {
      this.log("Saving login for " + aLogin.origin);
      Services.logins.addLogin(aLogin);
    } else {
      // userChoice == 1 --> just ignore the login.
      this.log("Ignoring login.");
    }

    Services.obs.notifyObservers(aLogin, "passwordmgr-prompt-save");
  },

  /**
   * Called when we think we detect a password or username change for
   * an existing login, when the form being submitted contains multiple
   * password fields.
   *
   * @param {nsILoginInfo} aOldLogin
   *                       The old login we may want to update.
   * @param {nsILoginInfo} aNewLogin
   *                       The new login from the page form.
   * @param {boolean} [dismissed = false]
   *                  If the prompt should be automatically dismissed on being shown.
   * @param {boolean} [notifySaved = false]
   *                  Whether the notification should indicate that a login has been saved
   * @param {string} [autoSavedLoginGuid = ""]
   *                 A guid value for the old login to be removed if the changes match it
   *                 to a different login
   */
  promptToChangePassword(
    aOldLogin,
    aNewLogin,
    dismissed = false,
    notifySaved = false,
    autoSavedLoginGuid = ""
  ) {
    let notifyObj = this._getPopupNote();

    if (notifyObj) {
      this._showChangeLoginNotification(
        notifyObj,
        aOldLogin,
        aNewLogin,
        dismissed,
        notifySaved,
        autoSavedLoginGuid
      );
    } else {
      this._showChangeLoginDialog(aOldLogin, aNewLogin);
    }
  },

  /**
   * Shows the Change Password popup notification.
   *
   * @param aNotifyObj
   *        A popup notification.
   *
   * @param aOldLogin
   *        The stored login we want to update.
   *
   * @param aNewLogin
   *        The login object with the changes we want to make.
   * @param dismissed
   *        A boolean indicating if the prompt should be automatically
   *        dismissed on being shown.
   * @param notifySaved
   *        A boolean value indicating whether the notification should indicate that
   *        a login has been saved
   */
  _showChangeLoginNotification(
    aNotifyObj,
    aOldLogin,
    aNewLogin,
    dismissed = false,
    notifySaved = false,
    autoSavedLoginGuid = ""
  ) {
    let login = aOldLogin.clone();
    login.origin = aNewLogin.origin;
    login.formActionOrigin = aNewLogin.formActionOrigin;
    login.password = aNewLogin.password;
    login.username = aNewLogin.username;

    let messageStringID;
    if (
      aOldLogin.username === "" &&
      login.username !== "" &&
      login.password == aOldLogin.password
    ) {
      // If the saved password matches the password we're prompting with then we
      // are only prompting to let the user add a username since there was one in
      // the form. Change the message so the purpose of the prompt is clearer.
      messageStringID = "updateLoginMsgAddUsername";
    }

    this._showLoginCaptureDoorhanger(
      login,
      "password-change",
      {
        dismissed,
        extraAttr: notifySaved ? "attention" : "",
      },
      {
        notifySaved,
        messageStringID,
        autoSavedLoginGuid,
      }
    );

    let oldGUID = aOldLogin.QueryInterface(Ci.nsILoginMetaInfo).guid;
    Services.obs.notifyObservers(
      aNewLogin,
      "passwordmgr-prompt-change",
      oldGUID
    );
  },

  /**
   * Shows the Change Password dialog.
   */
  _showChangeLoginDialog(aOldLogin, aNewLogin) {
    const buttonFlags = Ci.nsIPrompt.STD_YES_NO_BUTTONS;

    var dialogText;
    if (aOldLogin.username) {
      dialogText = this._getLocalizedString("updatePasswordMsg", [
        aOldLogin.username,
      ]);
    } else {
      dialogText = this._getLocalizedString("updatePasswordMsgNoUser");
    }

    var dialogTitle = this._getLocalizedString("passwordChangeTitle");

    // returns 0 for yes, 1 for no.
    var ok = !Services.prompt.confirmEx(
      this._chromeWindow,
      dialogTitle,
      dialogText,
      buttonFlags,
      null,
      null,
      null,
      null,
      {}
    );
    if (ok) {
      this.log("Updating password for user " + aOldLogin.username);
      this._updateLogin(aOldLogin, aNewLogin);
    }

    let oldGUID = aOldLogin.QueryInterface(Ci.nsILoginMetaInfo).guid;
    Services.obs.notifyObservers(
      aNewLogin,
      "passwordmgr-prompt-change",
      oldGUID
    );
  },

  /**
   * Called when we detect a password change in a form submission, but we
   * don't know which existing login (username) it's for. Asks the user
   * to select a username and confirm the password change.
   *
   * Note: The caller doesn't know the username for aNewLogin, so this
   *       function fills in .username and .usernameField with the values
   *       from the login selected by the user.
   */
  promptToChangePasswordWithUsernames(logins, aNewLogin) {
    this.log("promptToChangePasswordWithUsernames with count:", logins.length);

    var usernames = logins.map(
      l => l.username || this._getLocalizedString("noUsername")
    );
    var dialogText = this._getLocalizedString("userSelectText2");
    var dialogTitle = this._getLocalizedString("passwordChangeTitle");
    var selectedIndex = { value: null };

    // If user selects ok, outparam.value is set to the index
    // of the selected username.
    var ok = Services.prompt.select(
      this._chromeWindow,
      dialogTitle,
      dialogText,
      usernames,
      selectedIndex
    );
    if (ok) {
      // Now that we know which login to use, modify its password.
      var selectedLogin = logins[selectedIndex.value];
      this.log("Updating password for user " + selectedLogin.username);
      var newLoginWithUsername = Cc[
        "@mozilla.org/login-manager/loginInfo;1"
      ].createInstance(Ci.nsILoginInfo);
      newLoginWithUsername.init(
        aNewLogin.origin,
        aNewLogin.formActionOrigin,
        aNewLogin.httpRealm,
        selectedLogin.username,
        aNewLogin.password,
        selectedLogin.usernameField,
        aNewLogin.passwordField
      );
      this._updateLogin(selectedLogin, newLoginWithUsername);
    }
  },

  /* ---------- Internal Methods ---------- */

  _updateLogin(login, aNewLogin = null) {
    var now = Date.now();
    var propBag = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
      Ci.nsIWritablePropertyBag
    );
    if (aNewLogin) {
      propBag.setProperty("formActionOrigin", aNewLogin.formActionOrigin);
      propBag.setProperty("origin", aNewLogin.origin);
      propBag.setProperty("password", aNewLogin.password);
      propBag.setProperty("username", aNewLogin.username);
      // Explicitly set the password change time here (even though it would
      // be changed automatically), to ensure that it's exactly the same
      // value as timeLastUsed.
      propBag.setProperty("timePasswordChanged", now);
    }
    propBag.setProperty("timeLastUsed", now);
    propBag.setProperty("timesUsedIncrement", 1);
    Services.logins.modifyLogin(login, propBag);
  },

  /**
   * Given a content DOM window, returns the chrome window and browser it's in.
   */
  _getChromeWindow(aWindow) {
    // Handle non-e10s toolkit consumers.
    if (!Cu.isCrossProcessWrapper(aWindow)) {
      let browser = aWindow.docShell.chromeEventHandler;
      if (!browser) {
        return null;
      }

      let chromeWin = browser.ownerGlobal;
      if (!chromeWin) {
        return null;
      }

      return { win: chromeWin, browser };
    }

    return null;
  },

  _getNotifyWindow() {
    // Some sites pop up a temporary login window, which disappears
    // upon submission of credentials. We want to put the notification
    // prompt in the opener window if this seems to be happening.
    if (this._openerBrowser) {
      let chromeDoc = this._chromeWindow.document.documentElement;

      // Check to see if the current window was opened with chrome
      // disabled, and if so use the opener window. But if the window
      // has been used to visit other pages (ie, has a history),
      // assume it'll stick around and *don't* use the opener.
      if (chromeDoc.getAttribute("chromehidden") && !this._browser.canGoBack) {
        this.log("Using opener window for notification prompt.");
        return {
          win: this._openerBrowser.ownerGlobal,
          browser: this._openerBrowser,
        };
      }
    }

    return {
      win: this._chromeWindow,
      browser: this._browser,
    };
  },

  /**
   * Returns the popup notification to this prompter,
   * or null if there isn't one available.
   */
  _getPopupNote() {
    let popupNote = null;

    try {
      let { win: notifyWin } = this._getNotifyWindow();

      // .wrappedJSObject needed here -- see bug 422974 comment 5.
      popupNote = notifyWin.wrappedJSObject.PopupNotifications;
    } catch (e) {
      this.log("Popup notifications not available on window");
    }

    return popupNote;
  },

  /**
   * The user might enter a login that isn't the one we prefilled, but
   * is the same as some other existing login. So, pick a login with a
   * matching username, or return null.
   */
  _repickSelectedLogin(foundLogins, username) {
    for (var i = 0; i < foundLogins.length; i++) {
      if (foundLogins[i].username == username) {
        return foundLogins[i];
      }
    }
    return null;
  },

  /**
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
  _getLocalizedString(key, formatArgs) {
    if (formatArgs) {
      return this._strBundle.formatStringFromName(key, formatArgs);
    }
    return this._strBundle.GetStringFromName(key);
  },

  /**
   * Sanitizes the specified username, by stripping quotes and truncating if
   * it's too long. This helps prevent an evil site from messing with the
   * "save password?" prompt too much.
   */
  _sanitizeUsername(username) {
    if (username.length > 30) {
      username = username.substring(0, 30);
      username += this._ellipsis;
    }
    return username.replace(/['"]/g, "");
  },

  /**
   * The aURI parameter may either be a string uri, or an nsIURI instance.
   *
   * Returns the origin to use in a nsILoginInfo object (for example,
   * "http://example.com").
   */
  _getFormattedOrigin(aURI) {
    let uri;
    if (aURI instanceof Ci.nsIURI) {
      uri = aURI;
    } else {
      uri = Services.io.newURI(aURI);
    }

    return uri.scheme + "://" + uri.displayHostPort;
  },

  /**
   * Converts a login's origin field (a URL) to a short string for
   * prompting purposes. Eg, "http://foo.com" --> "foo.com", or
   * "ftp://www.site.co.uk" --> "site.co.uk".
   */
  _getShortDisplayHost(aURIString) {
    var displayHost;

    var idnService = Cc["@mozilla.org/network/idn-service;1"].getService(
      Ci.nsIIDNService
    );
    try {
      var uri = Services.io.newURI(aURIString);
      var baseDomain = Services.eTLD.getBaseDomain(uri);
      displayHost = idnService.convertToDisplayIDN(baseDomain, {});
    } catch (e) {
      this.log("_getShortDisplayHost couldn't process " + aURIString);
    }

    if (!displayHost) {
      displayHost = aURIString;
    }

    return displayHost;
  },

  /**
   * Returns the origin and realm for which authentication is being
   * requested, in the format expected to be used with nsILoginInfo.
   */
  _getAuthTarget(aChannel, aAuthInfo) {
    var origin, realm;

    // If our proxy is demanding authentication, don't use the
    // channel's actual destination.
    if (aAuthInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY) {
      this.log("getAuthTarget is for proxy auth");
      if (!(aChannel instanceof Ci.nsIProxiedChannel)) {
        throw new Error("proxy auth needs nsIProxiedChannel");
      }

      var info = aChannel.proxyInfo;
      if (!info) {
        throw new Error("proxy auth needs nsIProxyInfo");
      }

      // Proxies don't have a scheme, but we'll use "moz-proxy://"
      // so that it's more obvious what the login is for.
      var idnService = Cc["@mozilla.org/network/idn-service;1"].getService(
        Ci.nsIIDNService
      );
      origin =
        "moz-proxy://" +
        idnService.convertUTF8toACE(info.host) +
        ":" +
        info.port;
      realm = aAuthInfo.realm;
      if (!realm) {
        realm = origin;
      }

      return [origin, realm];
    }

    origin = this._getFormattedOrigin(aChannel.URI);

    // If a HTTP WWW-Authenticate header specified a realm, that value
    // will be available here. If it wasn't set or wasn't HTTP, we'll use
    // the formatted origin instead.
    realm = aAuthInfo.realm;
    if (!realm) {
      realm = origin;
    }

    return [origin, realm];
  },

  /**
   * Returns [username, password] as extracted from aAuthInfo (which
   * holds this info after having prompted the user).
   *
   * If the authentication was for a Windows domain, we'll prepend the
   * return username with the domain. (eg, "domain\user")
   */
  _GetAuthInfo(aAuthInfo) {
    var username, password;

    var flags = aAuthInfo.flags;
    if (flags & Ci.nsIAuthInformation.NEED_DOMAIN && aAuthInfo.domain) {
      username = aAuthInfo.domain + "\\" + aAuthInfo.username;
    } else {
      username = aAuthInfo.username;
    }

    password = aAuthInfo.password;

    return [username, password];
  },

  /**
   * Given a username (possibly in DOMAIN\user form) and password, parses the
   * domain out of the username if necessary and sets domain, username and
   * password on the auth information object.
   */
  _SetAuthInfo(aAuthInfo, username, password) {
    var flags = aAuthInfo.flags;
    if (flags & Ci.nsIAuthInformation.NEED_DOMAIN) {
      // Domain is separated from username by a backslash
      var idx = username.indexOf("\\");
      if (idx == -1) {
        aAuthInfo.username = username;
      } else {
        aAuthInfo.domain = username.substring(0, idx);
        aAuthInfo.username = username.substring(idx + 1);
      }
    } else {
      aAuthInfo.username = username;
    }
    aAuthInfo.password = password;
  },

  _newAsyncPromptConsumer(aCallback, aContext) {
    return {
      QueryInterface: ChromeUtils.generateQI([Ci.nsICancelable]),
      callback: aCallback,
      context: aContext,
      cancel() {
        this.callback.onAuthCancelled(this.context, false);
        this.callback = null;
        this.context = null;
      },
    };
  },

  /**
   * This function looks for existing logins that can be updated
   * to match a submitted login, instead of creating a new one.
   *
   * Given a login and a loginList, it filters the login list
   * to find every login with either:
   * - the same username as aLogin
   * - the same password as aLogin and an empty username
   *   so the user can add a username.
   * - the same guid as the given login when it has an empty username
   *
   * @param {nsILoginInfo} aLogin
   *                       login to use as filter.
   * @param {nsILoginInfo[]} aLoginList
   *                         Array of logins to filter.
   * @param {String} includeGUID
   *                 guid value for login that not be filtered out
   * @returns {nsILoginInfo[]} the filtered array of logins.
   */
  _filterUpdatableLogins(aLogin, aLoginList, includeGUID) {
    return aLoginList.filter(
      l =>
        l.username == aLogin.username ||
        (l.password == aLogin.password && !l.username) ||
        (includeGUID && includeGUID == l.guid)
    );
  },
}; // end of LoginManagerPrompter implementation

XPCOMUtils.defineLazyGetter(this.LoginManagerPrompter.prototype, "log", () => {
  let logger = LoginHelper.createLogger("LoginManagerPrompter");
  return logger.log.bind(logger);
});

const EXPORTED_SYMBOLS = ["LoginManagerPromptFactory", "LoginManagerPrompter"];
