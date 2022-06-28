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

const lazy = {};

/* eslint-disable block-scoped-var, no-var */

ChromeUtils.defineModuleGetter(
  lazy,
  "LoginHelper",
  "resource://gre/modules/LoginHelper.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "usernameAutocompleteSearch",
  "@mozilla.org/autocomplete/search;1?name=login-doorhanger-username",
  "nsIAutoCompleteSimpleSearch"
);

XPCOMUtils.defineLazyGetter(lazy, "strBundle", () => {
  return Services.strings.createBundle(
    "chrome://passwordmgr/locale/passwordmgr.properties"
  );
});

const LoginInfo = Components.Constructor(
  "@mozilla.org/login-manager/loginInfo;1",
  "nsILoginInfo",
  "init"
);

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
const PROMPT_NOTNOW_OR_DONTUPDATE = 2;
const PROMPT_NEVER = 3;
const PROMPT_DELETE = 3;

/**
 * The minimum age of a doorhanger in ms before it will get removed after a locationchange
 */
const NOTIFICATION_TIMEOUT_MS = 10 * 1000; // 10 seconds

/**
 * The minimum age of an attention-requiring dismissed doorhanger in ms
 * before it will get removed after a locationchange
 */
const ATTENTION_NOTIFICATION_TIMEOUT_MS = 60 * 1000; // 1 minute

function autocompleteSelected(popup) {
  let doc = popup.ownerDocument;
  let nameField = doc.getElementById("password-notification-username");
  let passwordField = doc.getElementById("password-notification-password");

  let activeElement = nameField.ownerDocument.activeElement;
  if (activeElement == nameField) {
    popup.onUsernameSelect();
  } else if (activeElement == passwordField) {
    popup.onPasswordSelect();
  }
}

const observer = {
  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

  // nsIObserver
  observe(subject, topic, data) {
    switch (topic) {
      case "autocomplete-did-enter-text": {
        let input = subject.QueryInterface(Ci.nsIAutoCompleteInput);
        autocompleteSelected(input.popupElement);
        break;
      }
    }
  },
};

/**
 * Implements interfaces for prompting the user to enter/save/change login info
 * found in HTML forms.
 */
class LoginManagerPrompter {
  get classID() {
    return Components.ID("{c47ff942-9678-44a5-bc9b-05e0d676c79c}");
  }

  get QueryInterface() {
    return ChromeUtils.generateQI(["nsILoginManagerPrompter"]);
  }

  /**
   * Called when we detect a password or username that is not yet saved as
   * an existing login.
   *
   * @param {Element} aBrowser
   *                  The browser element that the request came from.
   * @param {nsILoginInfo} aLogin
   *                       The new login from the page form.
   * @param {boolean} [dismissed = false]
   *                  If the prompt should be automatically dismissed on being shown.
   * @param {boolean} [notifySaved = false]
   *                  Whether the notification should indicate that a login has been saved
   * @param {string} [autoSavedLoginGuid = ""]
   *                 A guid value for the old login to be removed if the changes match it
   *                 to a different login
   * @param {object?} possibleValues
   *                 Contains values from anything that we think, but are not sure, might be
   *                 a username or password.  Has two properties, 'usernames' and 'passwords'.
   * @param {Set<String>} possibleValues.usernames
   * @param {Set<String>} possibleValues.passwords
   */
  promptToSavePassword(
    aBrowser,
    aLogin,
    dismissed = false,
    notifySaved = false,
    autoFilledLoginGuid = "",
    possibleValues = undefined
  ) {
    lazy.log.debug("Prompting user to save login.");
    let inPrivateBrowsing = PrivateBrowsingUtils.isBrowserPrivate(aBrowser);
    let notification = LoginManagerPrompter._showLoginCaptureDoorhanger(
      aBrowser,
      aLogin,
      "password-save",
      {
        dismissed: inPrivateBrowsing || dismissed,
        extraAttr: notifySaved ? "attention" : "",
      },
      possibleValues,
      {
        notifySaved,
        autoFilledLoginGuid,
      }
    );
    Services.obs.notifyObservers(aLogin, "passwordmgr-prompt-save");

    return {
      dismiss() {
        let { PopupNotifications } = aBrowser.ownerGlobal.wrappedJSObject;
        PopupNotifications.remove(notification);
      },
    };
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
   * @param {object?} possibleValues
   *                 Contains values from anything that we think, but are not sure, might be
   *                 a username or password.  Has two properties, 'usernames' and 'passwords'.
   * @param {Set<String>} possibleValues.usernames
   * @param {Set<String>} possibleValues.passwords
   */
  static _showLoginCaptureDoorhanger(
    browser,
    login,
    type,
    showOptions = {},
    possibleValues = undefined,
    {
      notifySaved = false,
      messageStringID,
      autoSavedLoginGuid = "",
      autoFilledLoginGuid = "",
    } = {}
  ) {
    lazy.log.debug(
      `Got autoSavedLoginGuid: ${autoSavedLoginGuid} and autoFilledLoginGuid ${autoFilledLoginGuid}.`
    );

    let saveMsgNames = {
      prompt: login.username === "" ? "saveLoginMsgNoUser2" : "saveLoginMsg2",
      buttonLabel: "saveLoginButtonAllow.label",
      buttonAccessKey: "saveLoginButtonAllow.accesskey",
      secondaryButtonLabel: "saveLoginButtonDeny.label",
      secondaryButtonAccessKey: "saveLoginButtonDeny.accesskey",
    };

    let changeMsgNames = {
      prompt:
        login.username === "" ? "updateLoginMsgNoUser3" : "updateLoginMsg3",
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

    let host = this._getShortDisplayHost(login.origin);
    let promptMsg =
      type == "password-save"
        ? this._getLocalizedString(saveMsgNames.prompt, [host])
        : this._getLocalizedString(changeMsgNames.prompt, [host]);

    let histogramName =
      type == "password-save"
        ? "PWMGR_PROMPT_REMEMBER_ACTION"
        : "PWMGR_PROMPT_UPDATE_ACTION";
    let histogram = Services.telemetry.getHistogramById(histogramName);

    let chromeDoc = browser.ownerDocument;
    let currentNotification;

    let wasModifiedEvent = {
      // Values are mutated
      did_edit_un: "false",
      did_select_un: "false",
      did_edit_pw: "false",
      did_select_pw: "false",
    };

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
      let foundLogins = lazy.LoginHelper.searchLoginsWithObject({
        formActionOrigin: login.formActionOrigin,
        origin: login.origin,
        httpRealm: login.httpRealm,
        schemeUpgrades: lazy.LoginHelper.schemeUpgrades,
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

    let onUsernameInput = () => {
      wasModifiedEvent.did_edit_un = "true";
      wasModifiedEvent.did_select_un = "false";
      onInput();
    };

    let onUsernameSelect = () => {
      wasModifiedEvent.did_edit_un = "false";
      wasModifiedEvent.did_select_un = "true";
    };

    let onPasswordInput = () => {
      wasModifiedEvent.did_edit_pw = "true";
      wasModifiedEvent.did_select_pw = "false";
      onInput();
    };

    let onPasswordSelect = () => {
      wasModifiedEvent.did_edit_pw = "false";
      wasModifiedEvent.did_select_pw = "true";
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

    let togglePopup = event => {
      event.target.parentElement
        .getElementsByClassName("ac-has-end-icon")[0]
        .toggleHistoryPopup();
    };

    let persistData = () => {
      let foundLogins = lazy.LoginHelper.searchLoginsWithObject({
        formActionOrigin: login.formActionOrigin,
        origin: login.origin,
        httpRealm: login.httpRealm,
        schemeUpgrades: lazy.LoginHelper.schemeUpgrades,
      });

      let logins = this._filterUpdatableLogins(
        login,
        foundLogins,
        autoSavedLoginGuid
      );
      let resolveBy = ["scheme", "timePasswordChanged"];
      logins = lazy.LoginHelper.dedupeLogins(
        logins,
        ["username"],
        resolveBy,
        login.origin
      );
      // sort exact username matches to the top
      logins.sort(l => (l.username == login.username ? -1 : 1));

      lazy.log.debug(`Matched ${logins.length} logins.`);

      let loginToRemove;
      let loginToUpdate = logins.shift();

      if (logins.length && logins[0].guid == autoSavedLoginGuid) {
        loginToRemove = logins.shift();
      }
      if (logins.length) {
        lazy.log.warn(
          "persistData:",
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
        lazy.log.debug(`Touch matched login: ${loginToUpdate.guid}.`);
        Services.logins.recordPasswordUse(
          loginToUpdate,
          PrivateBrowsingUtils.isBrowserPrivate(browser),
          loginToUpdate.username ? "form_password" : "form_login",
          !!autoFilledLoginGuid
        );
      } else {
        lazy.log.debug(`Update matched login: ${loginToUpdate.guid}.`);
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
        lazy.log.debug(`Removing login ${loginToRemove.guid}.`);
        Services.logins.removeLogin(loginToRemove);
      }
    };

    // The main action is the "Save" or "Update" button.
    let mainAction = {
      label: this._getLocalizedString(initialMsgNames.buttonLabel),
      accessKey: this._getLocalizedString(initialMsgNames.buttonAccessKey),
      callback: () => {
        readDataFromUI();
        if (
          type == "password-save" &&
          !Services.policies.isAllowed("removeMasterPassword")
        ) {
          if (!lazy.LoginHelper.isPrimaryPasswordSet()) {
            browser.ownerGlobal.openDialog(
              "chrome://mozapps/content/preferences/changemp.xhtml",
              "",
              "centerscreen,chrome,modal,titlebar"
            );
            if (!lazy.LoginHelper.isPrimaryPasswordSet()) {
              return;
            }
          }
        }
        histogram.add(PROMPT_ADD_OR_UPDATE);
        if (histogramName == "PWMGR_PROMPT_REMEMBER_ACTION") {
          Services.obs.notifyObservers(browser, "LoginStats:NewSavedPassword");
        } else if (histogramName == "PWMGR_PROMPT_UPDATE_ACTION") {
          Services.obs.notifyObservers(browser, "LoginStats:LoginUpdateSaved");
        } else {
          throw new Error("Unknown histogram");
        }

        let eventObject;
        if (type == "password-change") {
          eventObject = "update";
        } else if (type == "password-save") {
          eventObject = "save";
        } else {
          throw new Error(
            `Unexpected doorhanger type. Expected either 'password-save' or 'password-change', got ${type}`
          );
        }

        Services.telemetry.recordEvent(
          "pwmgr",
          "doorhanger_submitted",
          eventObject,
          null,
          wasModifiedEvent
        );

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
          histogram.add(PROMPT_NOTNOW_OR_DONTUPDATE);
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

    // Include a "Delete this login" button when updating an existing password
    if (type == "password-change") {
      secondaryActions.push({
        label: this._getLocalizedString("updateLoginButtonDelete.label"),
        accessKey: this._getLocalizedString(
          "updateLoginButtonDelete.accesskey"
        ),
        callback: async () => {
          histogram.add(PROMPT_DELETE);
          Services.obs.notifyObservers(
            null,
            "weave:telemetry:histogram",
            histogramName
          );
          const matchingLogins = await Services.logins.searchLoginsAsync({
            guid: login.guid,
            origin: login.origin,
          });
          Services.logins.removeLogin(matchingLogins[0]);
          browser.focus();
          // The "password-notification-icon" and "notification-icon-box" are hidden
          // at this point, so approximate the location with the next closest,
          // visible icon as the anchor.
          const anchor = browser.ownerDocument.getElementById("identity-icon");
          lazy.log.debug("Showing the ConfirmationHint");
          anchor.ownerGlobal.ConfirmationHint.show(anchor, "loginRemoved");
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

    let options = Object.assign(
      {
        timeout: Date.now() + timeoutMs,
        persistWhileVisible: true,
        passwordNotificationType: type,
        hideClose: true,
        eventCallback(topic) {
          switch (topic) {
            case "showing":
              lazy.log.debug("showing");
              currentNotification = this;

              // Record the first time this instance of the doorhanger is shown.
              if (!this.timeShown) {
                histogram.add(PROMPT_DISPLAYED);
                Services.obs.notifyObservers(
                  null,
                  "weave:telemetry:histogram",
                  histogramName
                );
              }

              chromeDoc
                .getElementById("password-notification-password")
                .removeAttribute("focused");
              chromeDoc
                .getElementById("password-notification-username")
                .removeAttribute("focused");
              chromeDoc
                .getElementById("password-notification-username")
                .addEventListener("input", onUsernameInput);
              chromeDoc
                .getElementById("password-notification-username")
                .addEventListener("keyup", onKeyUp);
              chromeDoc
                .getElementById("password-notification-password")
                .addEventListener("keyup", onKeyUp);
              chromeDoc
                .getElementById("password-notification-password")
                .addEventListener("input", onPasswordInput);
              chromeDoc
                .getElementById("password-notification-username-dropmarker")
                .addEventListener("click", togglePopup);

              LoginManagerPrompter._getUsernameSuggestions(
                login,
                possibleValues?.usernames
              ).then(usernameSuggestions => {
                let dropmarker = chromeDoc?.getElementById(
                  "password-notification-username-dropmarker"
                );
                if (dropmarker) {
                  dropmarker.hidden = !usernameSuggestions.length;
                }

                let usernameField = chromeDoc?.getElementById(
                  "password-notification-username"
                );
                if (usernameField) {
                  usernameField.classList.toggle(
                    "ac-has-end-icon",
                    !!usernameSuggestions.length
                  );
                }
              });

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
                  lazy.LoginHelper.isPrimaryPasswordSet() ||
                  // Don't show the toggle when the login was autofilled
                  !!autoFilledLoginGuid ||
                  // Dismissed-by-default prompts should still show the toggle.
                  (this.timeShown && this.wasDismissed) ||
                  // If we are only adding a username then the password is
                  // one that is already saved and we don't want to reveal
                  // it as the submitter of this form may not be the account
                  // owner, they may just be using the saved password.
                  (messageStringID == "updateLoginMsgAddUsername2" &&
                    login.timePasswordChanged <
                      Date.now() - VISIBILITY_TOGGLE_MAX_PW_AGE_MS);
                toggleBtn.hidden = hideToggle;
              }

              let popup = chromeDoc.getElementById("PopupAutoComplete");
              popup.onUsernameSelect = onUsernameSelect;
              popup.onPasswordSelect = onPasswordSelect;

              LoginManagerPrompter._setUsernameAutocomplete(
                login,
                possibleValues?.usernames
              );

              break;
            case "shown": {
              lazy.log.debug("shown");
              writeDataToUI();
              let anchorIcon = this.anchorElement;
              if (anchorIcon && this.options.extraAttr == "attention") {
                anchorIcon.removeAttribute("extraAttr");
                delete this.options.extraAttr;
              }
              break;
            }
            case "dismissed":
              // Note that this can run after `showing` but before `shown` upon tab switch.
              this.wasDismissed = true;
            // Fall through.
            case "removed": {
              // Note that this can run after `showing` and `shown` for the
              // notification it's replacing.
              lazy.log.debug(topic);
              currentNotification = null;

              let usernameField = chromeDoc.getElementById(
                "password-notification-username"
              );
              usernameField.removeEventListener("input", onUsernameInput);
              usernameField.removeEventListener("keyup", onKeyUp);
              let passwordField = chromeDoc.getElementById(
                "password-notification-password"
              );
              passwordField.removeEventListener("input", onPasswordInput);
              passwordField.removeEventListener("keyup", onKeyUp);
              passwordField.removeEventListener("command", onVisibilityToggle);
              chromeDoc
                .getElementById("password-notification-username-dropmarker")
                .removeEventListener("click", togglePopup);
              break;
            }
          }
          return false;
        },
      },
      showOptions
    );

    let notification = PopupNotifications.show(
      browser,
      notificationID,
      promptMsg,
      "password-notification-icon",
      mainAction,
      secondaryActions,
      options
    );

    if (notifySaved) {
      let anchor = notification.anchorElement;
      lazy.log.debug("Showing the ConfirmationHint.");
      anchor.ownerGlobal.ConfirmationHint.show(anchor, "passwordSaved");
    }

    return notification;
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
   * @param {object?} possibleValues
   *                 Contains values from anything that we think, but are not sure, might be
   *                 a username or password.  Has two properties, 'usernames' and 'passwords'.
   * @param {Set<String>} possibleValues.usernames
   * @param {Set<String>} possibleValues.passwords
   */
  promptToChangePassword(
    aBrowser,
    aOldLogin,
    aNewLogin,
    dismissed = false,
    notifySaved = false,
    autoSavedLoginGuid = "",
    autoFilledLoginGuid = "",
    possibleValues = undefined
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
      messageStringID = "updateLoginMsgAddUsername2";
    }

    let notification = LoginManagerPrompter._showLoginCaptureDoorhanger(
      aBrowser,
      login,
      "password-change",
      {
        dismissed,
        extraAttr: notifySaved ? "attention" : "",
      },
      possibleValues,
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

    return {
      dismiss() {
        let { PopupNotifications } = aBrowser.ownerGlobal.wrappedJSObject;
        PopupNotifications.remove(notification);
      },
    };
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
    lazy.log.debug(
      `Prompting user to change passowrd for username with count: ${logins.length}.`
    );

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
      lazy.log.debug(`Updating password for origin: ${aNewLogin.origin}.`);
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

  /**
   * Helper method to update and persist an existing nsILoginInfo object with new property values.
   */
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
    // Note that we don't call `recordPasswordUse` so telemetry won't record a
    // use in this case though that is normally correct since we would instead
    // record the save/update in a separate probe and recording it in both would
    // be wrong.
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
      return lazy.strBundle.formatStringFromName(key, formatArgs);
    }
    return lazy.strBundle.GetStringFromName(key);
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
      lazy.log.warn(`Couldn't process supplied URIString: ${aURIString}`);
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

  /**
   * Set the values that will be used the next time the username autocomplete popup is opened.
   *
   * @param {nsILoginInfo} login - used only for its information about the current domain.
   * @param {Set<String>?} possibleUsernames - values that we believe may be new/changed login usernames.
   */
  static async _setUsernameAutocomplete(login, possibleUsernames = new Set()) {
    let result = Cc["@mozilla.org/autocomplete/simple-result;1"].createInstance(
      Ci.nsIAutoCompleteSimpleResult
    );
    result.setDefaultIndex(0);

    let usernames = await this._getUsernameSuggestions(
      login,
      possibleUsernames
    );
    for (let { text, style } of usernames) {
      let value = text;
      let comment = "";
      let image = "";
      let _style = style;
      result.appendMatch(value, comment, image, _style);
    }

    if (usernames.length) {
      result.setSearchResult(Ci.nsIAutoCompleteResult.RESULT_SUCCESS);
    } else {
      result.setSearchResult(Ci.nsIAutoCompleteResult.RESULT_NOMATCH);
    }

    lazy.usernameAutocompleteSearch.overrideNextResult(result);
  }

  /**
   * @param {nsILoginInfo} login - used only for its information about the current domain.
   * @param {Set<String>?} possibleUsernames - values that we believe may be new/changed login usernames.
   *
   * @returns {object[]} an ordered list of usernames to be used the next time the username autocomplete popup is opened.
   */
  static async _getUsernameSuggestions(login, possibleUsernames = new Set()) {
    if (!Services.prefs.getBoolPref("signon.capture.inputChanges.enabled")) {
      return [];
    }

    // Don't reprompt for Primary Password, as we already prompted at least once
    // to show the doorhanger if it is locked
    if (!Services.logins.isLoggedIn) {
      return [];
    }

    let baseDomainLogins = await Services.logins.searchLoginsAsync({
      origin: login.origin,
      schemeUpgrades: lazy.LoginHelper.schemeUpgrades,
      acceptDifferentSubdomains: true,
    });

    let saved = baseDomainLogins.map(login => {
      return { text: login.username, style: "login" };
    });
    let possible = [...possibleUsernames].map(username => {
      return { text: username, style: "possible-username" };
    });

    return possible
      .concat(saved)
      .reduce((acc, next) => {
        let alreadyInAcc =
          acc.findIndex(entry => entry.text == next.text) != -1;
        if (!alreadyInAcc) {
          acc.push(next);
        } else if (next.style == "possible-username") {
          let existingIndex = acc.findIndex(entry => entry.text == next.text);
          acc[existingIndex] = next;
        }
        return acc;
      }, [])
      .filter(suggestion => !!suggestion.text);
  }
}

// Add this observer once for the process.
Services.obs.addObserver(observer, "autocomplete-did-enter-text");

XPCOMUtils.defineLazyGetter(lazy, "log", () => {
  return lazy.LoginHelper.createLogger("LoginManagerPrompter");
});

const EXPORTED_SYMBOLS = ["LoginManagerPrompter"];
