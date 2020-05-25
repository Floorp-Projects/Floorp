/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = [
  "GeckoViewAutocomplete",
  "LoginEntry",
  "SelectOption",
];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  GeckoViewPrompter: "resource://gre/modules/GeckoViewPrompter.jsm",
});

XPCOMUtils.defineLazyGetter(this, "LoginInfo", () =>
  Components.Constructor(
    "@mozilla.org/login-manager/loginInfo;1",
    "nsILoginInfo",
    "init"
  )
);

class LoginEntry {
  constructor({
    origin,
    formActionOrigin,
    httpRealm,
    username,
    password,
    guid,
    timeCreated,
    timeLastUsed,
    timePasswordChanged,
    timesUsed,
  }) {
    this.origin = origin ?? null;
    this.formActionOrigin = formActionOrigin ?? null;
    this.httpRealm = httpRealm ?? null;
    this.username = username ?? null;
    this.password = password ?? null;

    // Metadata.
    this.guid = guid ?? null;
    // TODO: Not supported by GV.
    this.timeCreated = timeCreated ?? null;
    this.timeLastUsed = timeLastUsed ?? null;
    this.timePasswordChanged = timePasswordChanged ?? null;
    this.timesUsed = timesUsed ?? null;
  }

  toLoginInfo() {
    const info = new LoginInfo(
      this.origin,
      this.formActionOrigin,
      this.httpRealm,
      this.username,
      this.password
    );

    // Metadata.
    info.QueryInterface(Ci.nsILoginMetaInfo);
    info.guid = this.guid;
    info.timeCreated = this.timeCreated;
    info.timeLastUsed = this.timeLastUsed;
    info.timePasswordChanged = this.timePasswordChanged;
    info.timesUsed = this.timesUsed;

    return info;
  }

  static parse(aObj) {
    const entry = new LoginEntry({});
    Object.assign(entry, aObj);

    return entry;
  }

  static fromLoginInfo(aInfo) {
    const entry = new LoginEntry({});
    entry.origin = aInfo.origin;
    entry.formActionOrigin = aInfo.formActionOrigin;
    entry.httpRealm = aInfo.httpRealm;
    entry.username = aInfo.username;
    entry.password = aInfo.password;

    // Metadata.
    aInfo.QueryInterface(Ci.nsILoginMetaInfo);
    entry.guid = aInfo.guid;
    entry.timeCreated = aInfo.timeCreated;
    entry.timeLastUsed = aInfo.timeLastUsed;
    entry.timePasswordChanged = aInfo.timePasswordChanged;
    entry.timesUsed = aInfo.timesUsed;

    return entry;
  }
}

class SelectOption {
  // Sync with Autocomplete.LoginSelectOption.Hint in Autocomplete.java.
  static Hint = {
    NONE: 0,
    GENERATED: 1 << 0,
    INSECURE_FORM: 1 << 1,
    DUPLICATE_USERNAME: 1 << 2,
    MATCHING_ORIGIN: 1 << 3,
  };

  constructor({ value, hint }) {
    this.value = value ?? null;
    this.hint = hint ?? SelectOption.Hint.NONE;
  }
}

// Sync with Autocomplete.UsedField in Autocomplete.java.
const UsedField = { PASSWORD: 1 };

const GeckoViewAutocomplete = {
  /**
   * Delegates login entry fetching for the given domain to the attached
   * LoginStorage GeckoView delegate.
   *
   * @param aDomain
   *        The domain string to fetch login entries for.
   * @return {Promise}
   *         Resolves with an array of login objects or null.
   *         Rejected if no delegate is attached.
   *         Login object string properties:
   *         { guid, origin, formActionOrigin, httpRealm, username, password }
   */
  fetchLogins(aDomain) {
    debug`fetchLogins for ${aDomain}`;

    return EventDispatcher.instance.sendRequestForResult({
      type: "GeckoView:Autocomplete:Fetch:Login",
      domain: aDomain,
    });
  },

  /**
   * Delegates login entry saving to the attached LoginStorage GeckoView delegate.
   * Call this when a new login entry or a new password for an existing login
   * entry has been submitted.
   *
   * @param aLogin The {LoginEntry} to be saved.
   */
  onLoginSave(aLogin) {
    debug`onLoginSave ${aLogin}`;

    EventDispatcher.instance.sendRequest({
      type: "GeckoView:Autocomplete:Save:Login",
      login: aLogin,
    });
  },

  /**
   * Delegates login entry password usage to the attached LoginStorage GeckoView
   * delegate.
   * Call this when the password of an existing login entry, as returned by
   * fetchLogins, has been used for autofill.
   *
   * @param aLogin The {LoginEntry} whose password was used.
   */
  onLoginPasswordUsed(aLogin) {
    debug`onLoginUsed ${aLogin}`;

    EventDispatcher.instance.sendRequest({
      type: "GeckoView:Autocomplete:Used:Login",
      usedFields: UsedField.PASSWORD,
      login: aLogin,
    });
  },

  _numActiveOnLoginSelect: 0,
  /**
   * Delegates login entry selection.
   * Call this when there are multiple login entry option for a form to delegate
   * the selection.
   *
   * @param aBrowser The browser instance the triggered the selection.
   * @param aOptions The list of {SelectOption} depicting viable options.
   */
  onLoginSelect(aBrowser, aOptions) {
    debug`onLoginSelect ${aOptions}`;

    return new Promise((resolve, reject) => {
      if (!aBrowser || !aOptions) {
        debug`onLoginSelect Rejecting - no browser or options provided`;
        reject();
        return;
      }

      const prompt = new GeckoViewPrompter(aBrowser.ownerGlobal);
      prompt.asyncShowPrompt(
        {
          type: "Autocomplete:Select:Login",
          options: aOptions,
        },
        result => {
          if (!result || !result.selection) {
            reject();
            return;
          }

          const option = new SelectOption({
            value: LoginEntry.parse(result.selection.value),
            hint: result.selection.hint,
          });
          resolve(option);
        }
      );
    });
  },

  async delegateSelection({
    browsingContext,
    options,
    inputElementIdentifier,
    formOrigin,
  }) {
    debug`delegateSelection ${options}`;

    if (!options.length) {
      return;
    }

    let insecureHint = SelectOption.Hint.NONE;
    let loginStyle = null;

    const selectOptions = [];

    for (const option of options) {
      switch (option.style) {
        case "insecureWarning": {
          // We depend on the insecure warning to be the first option.
          insecureHint = SelectOption.Hint.INSECURE_FORM;
          break;
        }
        case "generatedPassword": {
          const comment = JSON.parse(option.comment);
          selectOptions.push(
            new SelectOption({
              value: new LoginEntry({
                password: comment.generatedPassword,
              }),
              hint: SelectOption.Hint.GENERATED | insecureHint,
            })
          );
          break;
        }
        case "login":
        // Fallthrough.
        case "loginWithOrigin": {
          loginStyle = option.style;
          const comment = JSON.parse(option.comment);

          let hint = SelectOption.Hint.NONE | insecureHint;
          if (comment.isDuplicateUsername) {
            hint |= SelectOption.Hint.DUPLICATE_USERNAME;
          }
          if (comment.isOriginMatched) {
            hint |= SelectOption.Hint.MATCHING_ORIGIN;
          }

          selectOptions.push(
            new SelectOption({
              value: LoginEntry.parse(comment.login),
              hint,
            })
          );
          break;
        }
      }
    }

    if (selectOptions.length < 1) {
      debug`Abort delegateSelection - no valid options provided`;
      return;
    }

    if (this._numActiveOnLoginSelect > 0) {
      debug`Abort delegateSelection - there is already one delegation active`;
      return;
    }

    ++this._numActiveOnLoginSelect;

    const browser = browsingContext.top.embedderElement;
    const selectedOption = await this.onLoginSelect(
      browser,
      selectOptions
    ).catch(_ => {
      debug`No GV delegate attached`;
    });

    --this._numActiveOnLoginSelect;

    debug`delegateSelection selected option: ${selectedOption}`;
    const selectedLogin = selectedOption?.value?.toLoginInfo();

    if (!selectedLogin) {
      debug`Abort delegateSelection - no login entry selected`;
      return;
    }

    debug`delegateSelection - filling form`;

    const actor = browsingContext.currentWindowGlobal.getActor("LoginManager");

    await actor.fillForm({
      browser,
      inputElementIdentifier,
      loginFormOrigin: formOrigin,
      login: selectedLogin,
      style:
        selectedOption.hint & SelectOption.Hint.GENERATED
          ? "generatedPassword"
          : loginStyle,
    });

    debug`delegateSelection - form filled`;
  },
};

const { debug } = GeckoViewUtils.initLogging("GeckoViewAutocomplete");
