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

/* eslint-disable block-scoped-var, no-var */

ChromeUtils.defineModuleGetter(
  this,
  "LoginHelper",
  "resource://gre/modules/LoginHelper.jsm"
);

XPCOMUtils.defineLazyGetter(this, "strBundle", () => {
  return Services.strings.createBundle(
    "chrome://passwordmgr/locale/passwordmgr.properties"
  );
});

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
 */
const PROMPT_DISPLAYED = 0;

const PROMPT_ADD_OR_UPDATE = 1;
const PROMPT_NOTNOW = 2;
const PROMPT_NEVER = 3;

/**
 * The minimum age of a doorhanger in ms before it will get removed after a locationchange
 */
const NOTIFICATION_TIMEOUT_MS = 10 * 1000; // 10 seconds

/**
 * The minimum age of an attention-requiring dismissed doorhanger in ms
 * before it will get removed after a locationchange
 */
const ATTENTION_NOTIFICATION_TIMEOUT_MS = 60 * 1000; // 1 minute

/**
 * Implements interfaces for prompting the user to enter/save/change login info
 * found in HTML forms.
 */
class LoginManagerPrompter {
  get classID() {
    return Components.ID("{c47ff942-9678-44a5-bc9b-05e0d676c79c}");
  }

  get QueryInterface() {
    return ChromeUtils.generateQI([Ci.nsILoginManagerPrompter]);
  }

  promptToSavePassword(
    aBrowser,
    aLogin,
    dismissed = false,
    notifySaved = false,
    autoFilledLoginGuid = ""
  ) {
    log.debug("promptToSavePassword");
    let inPrivateBrowsing = PrivateBrowsingUtils.isBrowserPrivate(aBrowser);
    LoginManagerPrompter._showLoginCaptureDoorhanger(
      aBrowser,
      aLogin,
      "password-save",
      {
        dismissed: inPrivateBrowsing || dismissed,
        extraAttr: notifySaved ? "attention" : "",
      },
      {
        notifySaved,
        autoFilledLoginGuid,
      }
    );
    Services.obs.notifyObservers(aLogin, "passwordmgr-prompt-save");
  }

  /**
   * Displays the PopupNotifications.jsm doorhanger for password save or change.
   *
   * @param {Element} browser
   *        The browser to show the doorhanger on.
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
   *        A string guid value for the auto-saved login to be removed if the changes
   *        match it to a different login
   * @param {string} [options.autoFilledLoginGuid = ""]
   *        A string guid value for the autofilled login
   */
  static _showLoginCaptureDoorhanger(
    browser,
    login,
    type,
    showOptions = {},
    {
      notifySaved = false,
      messageStringID,
      autoSavedLoginGuid = "",
      autoFilledLoginGuid = "",
    } = {}
  ) {
    log.debug(
      `_showLoginCaptureDoorhanger, got autoSavedLoginGuid: ${autoSavedLoginGuid}`
    );
    log.debug(
      `_showLoginCaptureDoorhanger, got autoFilledLoginGuid: ${autoFilledLoginGuid}`
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
      if (!login.password.length) {
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
      let msgNames = !logins.length ? saveMsgNames : changeMsgNames;

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
      let nameField = chromeDoc.getElementById(
        "password-notification-username"
      );
      nameField.placeholder = usernamePlaceholder;
      nameField.value = login.username;

      let toggleCheckbox = chromeDoc.getElementById(
        "password-notification-visibilityToggle"
      );
      toggleCheckbox.removeAttribute("checked");
      let passwordField = chromeDoc.getElementById(
        "password-notification-password"
      );
      // Ensure the type is reset so the field is masked.
      passwordField.type = "password";
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

    let onKeyUp = e => {
      if (e.key == "Enter") {
        e.target.closest("popupnotification").button.doCommand();
      }
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

      log.debug(`persistData: Matched ${logins.length} logins`);

      let loginToRemove;
      let loginToUpdate = logins.shift();

      if (logins.length && logins[0].guid == autoSavedLoginGuid) {
        loginToRemove = logins.shift();
      }
      if (logins.length) {
        log.warn(
          logins.length,
          "other updatable logins!",
          logins.map(l => l.guid),
          "loginToUpdate:",
          loginToUpdate && loginToUpdate.guid,
          "loginToRemove:",
          loginToRemove && loginToRemove.guid
        );
        // Proceed with updating the login with the best username match rather
        // than returning and losing the edit.
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
        log.debug("persistData: Touch matched login", loginToUpdate.guid);
        Services.logins.recordPasswordUse(loginToUpdate);
      } else {
        log.debug("persistData: Update matched login", loginToUpdate.guid);
        this._updateLogin(loginToUpdate, login);
        // notify that this auto-saved login has been merged
        if (loginToRemove && loginToRemove.guid == autoSavedLoginGuid) {
          Services.obs.notifyObservers(
            loginToRemove,
            "passwordmgr-autosaved-login-merged"
          );
        }
      }

      if (loginToRemove) {
        log.debug("persistData: removing login", loginToRemove.guid);
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

    // .wrappedJSObject needed here -- see bug 422974 comment 5.
    let { PopupNotifications } = browser.ownerGlobal.wrappedJSObject;

    let notificationID = "password";
    // keep attention notifications around for longer after a locationchange
    const timeoutMs =
      showOptions.dismissed && showOptions.extraAttr == "attention"
        ? ATTENTION_NOTIFICATION_TIMEOUT_MS
        : NOTIFICATION_TIMEOUT_MS;
    let notification = PopupNotifications.show(
      browser,
      notificationID,
      promptMsg,
      "password-notification-icon",
      mainAction,
      secondaryActions,
      Object.assign(
        {
          timeout: Date.now() + timeoutMs,
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
                  .getElementById("password-notification-username")
                  .addEventListener("keyup", onKeyUp);
                chromeDoc
                  .getElementById("password-notification-password")
                  .addEventListener("keyup", onKeyUp);
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
                    // Don't show the toggle when the login was autofilled
                    !!autoFilledLoginGuid ||
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
                  .getElementById("password-notification-username")
                  .removeEventListener("keyup", onKeyUp);
                chromeDoc
                  .getElementById("password-notification-password")
                  .removeEventListener("input", onInput);
                chromeDoc
                  .getElementById("password-notification-password")
                  .removeEventListener("keyup", onKeyUp);
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
      let anchor = notification.anchorElement;
      log.debug("Showing the ConfirmationHint");
      anchor.ownerGlobal.ConfirmationHint.show(anchor, "passwordSaved");
    }
  }

  /**
   * Called when we think we detect a password or username change for
   * an existing login, when the form being submitted contains multiple
   * password fields.
   *
   * @param {Element} aBrowser
   *                  The browser element that the request came from.
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
    aBrowser,
    aOldLogin,
    aNewLogin,
    dismissed = false,
    notifySaved = false,
    autoSavedLoginGuid = "",
    autoFilledLoginGuid = ""
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

    LoginManagerPrompter._showLoginCaptureDoorhanger(
      aBrowser,
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
        autoFilledLoginGuid,
      }
    );

    let oldGUID = aOldLogin.QueryInterface(Ci.nsILoginMetaInfo).guid;
    Services.obs.notifyObservers(
      aNewLogin,
      "passwordmgr-prompt-change",
      oldGUID
    );
  }

  /**
   * Called when we detect a password change in a form submission, but we
   * don't know which existing login (username) it's for. Asks the user
   * to select a username and confirm the password change.
   *
   * Note: The caller doesn't know the username for aNewLogin, so this
   *       function fills in .username and .usernameField with the values
   *       from the login selected by the user.
   */
  promptToChangePasswordWithUsernames(browser, logins, aNewLogin) {
    log.debug("promptToChangePasswordWithUsernames with count:", logins.length);

    var usernames = logins.map(
      l => l.username || LoginManagerPrompter._getLocalizedString("noUsername")
    );
    var dialogText = LoginManagerPrompter._getLocalizedString(
      "userSelectText2"
    );
    var dialogTitle = LoginManagerPrompter._getLocalizedString(
      "passwordChangeTitle"
    );
    var selectedIndex = { value: null };

    // If user selects ok, outparam.value is set to the index
    // of the selected username.
    var ok = Services.prompt.select(
      browser.ownerGlobal,
      dialogTitle,
      dialogText,
      usernames,
      selectedIndex
    );
    if (ok) {
      // Now that we know which login to use, modify its password.
      var selectedLogin = logins[selectedIndex.value];
      log.debug("Updating password for user", selectedLogin.username);
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
      LoginManagerPrompter._updateLogin(selectedLogin, newLoginWithUsername);
    }
  }

  /* ---------- Internal Methods ---------- */

  static _updateLogin(login, aNewLogin) {
    var now = Date.now();
    var propBag = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
      Ci.nsIWritablePropertyBag
    );
    propBag.setProperty("formActionOrigin", aNewLogin.formActionOrigin);
    propBag.setProperty("origin", aNewLogin.origin);
    propBag.setProperty("password", aNewLogin.password);
    propBag.setProperty("username", aNewLogin.username);
    // Explicitly set the password change time here (even though it would
    // be changed automatically), to ensure that it's exactly the same
    // value as timeLastUsed.
    propBag.setProperty("timePasswordChanged", now);
    propBag.setProperty("timeLastUsed", now);
    propBag.setProperty("timesUsedIncrement", 1);
    Services.logins.modifyLogin(login, propBag);
  }

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
  static _getLocalizedString(key, formatArgs) {
    if (formatArgs) {
      return strBundle.formatStringFromName(key, formatArgs);
    }
    return strBundle.GetStringFromName(key);
  }

  /**
   * Converts a login's origin field to a short string for
   * prompting purposes. Eg, "http://foo.com" --> "foo.com", or
   * "ftp://www.site.co.uk" --> "site.co.uk".
   */
  static _getShortDisplayHost(aURIString) {
    var displayHost;

    var idnService = Cc["@mozilla.org/network/idn-service;1"].getService(
      Ci.nsIIDNService
    );
    try {
      var uri = Services.io.newURI(aURIString);
      var baseDomain = Services.eTLD.getBaseDomain(uri);
      displayHost = idnService.convertToDisplayIDN(baseDomain, {});
    } catch (e) {
      log.warn("_getShortDisplayHost couldn't process", aURIString);
    }

    if (!displayHost) {
      displayHost = aURIString;
    }

    return displayHost;
  }

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
  static _filterUpdatableLogins(aLogin, aLoginList, includeGUID) {
    return aLoginList.filter(
      l =>
        l.username == aLogin.username ||
        (l.password == aLogin.password && !l.username) ||
        (includeGUID && includeGUID == l.guid)
    );
  }
}

XPCOMUtils.defineLazyGetter(this, "log", () => {
  return LoginHelper.createLogger("LoginManagerPrompter");
});

const EXPORTED_SYMBOLS = ["LoginManagerPrompter"];
