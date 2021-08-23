/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["LoginStorageDelegate"];

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  GeckoViewAutocomplete: "resource://gre/modules/GeckoViewAutocomplete.jsm",
  GeckoViewPrompter: "resource://gre/modules/GeckoViewPrompter.jsm",
  LoginEntry: "resource://gre/modules/GeckoViewAutocomplete.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

const { debug, warn } = GeckoViewUtils.initLogging("LoginStorageDelegate");

// Sync with  LoginSaveOption.Hint in Autocomplete.java.
const LoginStorageHint = {
  NONE: 0,
  GENERATED: 1 << 0,
  LOW_CONFIDENCE: 1 << 1,
};

class LoginStorageDelegate {
  _createMessage({ dismissed, autoSavedLoginGuid }, aLogins) {
    let hint = LoginStorageHint.NONE;
    if (dismissed) {
      hint |= LoginStorageHint.LOW_CONFIDENCE;
    }
    if (autoSavedLoginGuid) {
      hint |= LoginStorageHint.GENERATED;
    }
    return {
      // Sync with GeckoSession.handlePromptEvent.
      type: "Autocomplete:Save:Login",
      hint,
      logins: aLogins,
    };
  }

  promptToSavePassword(
    aBrowser,
    aLogin,
    dismissed = false,
    notifySaved = false
  ) {
    const prompt = new GeckoViewPrompter(aBrowser.ownerGlobal);
    prompt.asyncShowPrompt(
      this._createMessage({ dismissed }, [LoginEntry.fromLoginInfo(aLogin)]),
      result => {
        const selectedLogin = result?.selection?.value;

        if (!selectedLogin) {
          return;
        }

        const loginInfo = LoginEntry.parse(selectedLogin).toLoginInfo();
        Services.obs.notifyObservers(loginInfo, "passwordmgr-prompt-save");

        GeckoViewAutocomplete.onLoginSave(selectedLogin);
      }
    );

    return {
      dismiss() {
        prompt.dismiss();
      },
    };
  }

  promptToChangePassword(
    aBrowser,
    aOldLogin,
    aNewLogin,
    dismissed = false,
    notifySaved = false,
    autoSavedLoginGuid = ""
  ) {
    const newLogin = LoginEntry.fromLoginInfo(aOldLogin || aNewLogin);
    const oldGuid = (aOldLogin && newLogin.guid) || null;
    newLogin.origin = aNewLogin.origin;
    newLogin.formActionOrigin = aNewLogin.formActionOrigin;
    newLogin.password = aNewLogin.password;
    newLogin.username = aNewLogin.username;

    const prompt = new GeckoViewPrompter(aBrowser.ownerGlobal);
    prompt.asyncShowPrompt(
      this._createMessage({ dismissed, autoSavedLoginGuid }, [newLogin]),
      result => {
        const selectedLogin = result?.selection?.value;

        if (!selectedLogin) {
          return;
        }

        GeckoViewAutocomplete.onLoginSave(selectedLogin);

        const loginInfo = LoginEntry.parse(selectedLogin).toLoginInfo();
        Services.obs.notifyObservers(
          loginInfo,
          "passwordmgr-prompt-change",
          oldGuid
        );
      }
    );

    return {
      dismiss() {
        prompt.dismiss();
      },
    };
  }

  promptToChangePasswordWithUsernames(aBrowser, aLogins, aNewLogin) {
    this.promptToChangePassword(aBrowser, null /* oldLogin */, aNewLogin);
  }
}

LoginStorageDelegate.prototype.classID = Components.ID(
  "{3d765750-1c3d-11ea-aaef-0800200c9a66}"
);
LoginStorageDelegate.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsILoginManagerPrompter",
]);
