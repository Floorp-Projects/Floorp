/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewUtils } from "resource://gre/modules/GeckoViewUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  EventDispatcher: "resource://gre/modules/Messaging.sys.mjs",
  GeckoViewPrompter: "resource://gre/modules/GeckoViewPrompter.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "LoginInfo", () =>
  Components.Constructor(
    "@mozilla.org/login-manager/loginInfo;1",
    "nsILoginInfo",
    "init"
  )
);

export class LoginEntry {
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
    this.origin = origin ?? "";
    this.formActionOrigin = formActionOrigin ?? null;
    this.httpRealm = httpRealm ?? null;
    this.username = username ?? "";
    this.password = password ?? "";

    // Metadata.
    this.guid = guid ?? null;
    // TODO: Not supported by GV.
    this.timeCreated = timeCreated ?? null;
    this.timeLastUsed = timeLastUsed ?? null;
    this.timePasswordChanged = timePasswordChanged ?? null;
    this.timesUsed = timesUsed ?? null;
  }

  toLoginInfo() {
    const info = new lazy.LoginInfo(
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

export class Address {
  constructor({
    name,
    givenName,
    additionalName,
    familyName,
    organization,
    streetAddress,
    addressLevel1,
    addressLevel2,
    addressLevel3,
    postalCode,
    country,
    tel,
    email,
    guid,
    timeCreated,
    timeLastUsed,
    timeLastModified,
    timesUsed,
    version,
  }) {
    this.name = name ?? "";
    this.givenName = givenName ?? "";
    this.additionalName = additionalName ?? "";
    this.familyName = familyName ?? "";
    this.organization = organization ?? "";
    this.streetAddress = streetAddress ?? "";
    this.addressLevel1 = addressLevel1 ?? "";
    this.addressLevel2 = addressLevel2 ?? "";
    this.addressLevel3 = addressLevel3 ?? "";
    this.postalCode = postalCode ?? "";
    this.country = country ?? "";
    this.tel = tel ?? "";
    this.email = email ?? "";

    // Metadata.
    this.guid = guid ?? null;
    // TODO: Not supported by GV.
    this.timeCreated = timeCreated ?? null;
    this.timeLastUsed = timeLastUsed ?? null;
    this.timeLastModified = timeLastModified ?? null;
    this.timesUsed = timesUsed ?? null;
    this.version = version ?? null;
  }

  isValid() {
    return (
      (this.name ?? this.givenName ?? this.familyName) !== "" &&
      this.streetAddress !== "" &&
      this.postalCode !== ""
    );
  }

  static fromGecko(aObj) {
    return new Address({
      version: aObj.version,
      name: aObj.name,
      givenName: aObj["given-name"],
      additionalName: aObj["additional-name"],
      familyName: aObj["family-name"],
      organization: aObj.organization,
      streetAddress: aObj["street-address"],
      addressLevel1: aObj["address-level1"],
      addressLevel2: aObj["address-level2"],
      addressLevel3: aObj["address-level3"],
      postalCode: aObj["postal-code"],
      country: aObj.country,
      tel: aObj.tel,
      email: aObj.email,
      guid: aObj.guid,
      timeCreated: aObj.timeCreated,
      timeLastUsed: aObj.timeLastUsed,
      timeLastModified: aObj.timeLastModified,
      timesUsed: aObj.timesUsed,
    });
  }

  static parse(aObj) {
    const entry = new Address({});
    Object.assign(entry, aObj);

    return entry;
  }

  toGecko() {
    return {
      version: this.version,
      name: this.name,
      "given-name": this.givenName,
      "additional-name": this.additionalName,
      "family-name": this.familyName,
      organization: this.organization,
      "street-address": this.streetAddress,
      "address-level1": this.addressLevel1,
      "address-level2": this.addressLevel2,
      "address-level3": this.addressLevel3,
      "postal-code": this.postalCode,
      country: this.country,
      tel: this.tel,
      email: this.email,
      guid: this.guid,
    };
  }
}

export class CreditCard {
  constructor({
    name,
    number,
    expMonth,
    expYear,
    type,
    guid,
    timeCreated,
    timeLastUsed,
    timeLastModified,
    timesUsed,
    version,
  }) {
    this.name = name ?? "";
    this.number = number ?? "";
    this.expMonth = expMonth ?? "";
    this.expYear = expYear ?? "";
    this.type = type ?? "";

    // Metadata.
    this.guid = guid ?? null;
    // TODO: Not supported by GV.
    this.timeCreated = timeCreated ?? null;
    this.timeLastUsed = timeLastUsed ?? null;
    this.timeLastModified = timeLastModified ?? null;
    this.timesUsed = timesUsed ?? null;
    this.version = version ?? null;
  }

  isValid() {
    return (
      this.name !== "" &&
      this.number !== "" &&
      this.expMonth !== "" &&
      this.expYear !== ""
    );
  }

  static fromGecko(aObj) {
    return new CreditCard({
      version: aObj.version,
      name: aObj["cc-name"],
      number: aObj["cc-number"],
      expMonth: aObj["cc-exp-month"]?.toString(),
      expYear: aObj["cc-exp-year"]?.toString(),
      type: aObj["cc-type"],
      guid: aObj.guid,
      timeCreated: aObj.timeCreated,
      timeLastUsed: aObj.timeLastUsed,
      timeLastModified: aObj.timeLastModified,
      timesUsed: aObj.timesUsed,
    });
  }

  static parse(aObj) {
    const entry = new CreditCard({});
    Object.assign(entry, aObj);

    return entry;
  }

  toGecko() {
    return {
      version: this.version,
      "cc-name": this.name,
      "cc-number": this.number,
      "cc-exp-month": this.expMonth,
      "cc-exp-year": this.expYear,
      "cc-type": this.type,
      guid: this.guid,
    };
  }
}

export class SelectOption {
  // Sync with Autocomplete.SelectOption.Hint in Autocomplete.java.
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

export const GeckoViewAutocomplete = {
  /** current opened prompt */
  _prompt: null,

  /**
   * Delegates login entry fetching for the given domain to the attached
   * LoginStorage GeckoView delegate.
   *
   * @param aDomain
   *        The domain string to fetch login entries for. If null, all logins
   *        will be fetched.
   * @return {Promise}
   *         Resolves with an array of login objects or null.
   *         Rejected if no delegate is attached.
   *         Login object string properties:
   *         { guid, origin, formActionOrigin, httpRealm, username, password }
   */
  fetchLogins(aDomain = null) {
    debug`fetchLogins for ${aDomain ?? "All domains"}`;

    return lazy.EventDispatcher.instance.sendRequestForResult({
      type: "GeckoView:Autocomplete:Fetch:Login",
      domain: aDomain,
    });
  },

  /**
   * Delegates credit card entry fetching to the attached LoginStorage
   * GeckoView delegate.
   *
   * @return {Promise}
   *         Resolves with an array of credit card objects or null.
   *         Rejected if no delegate is attached.
   *         Login object string properties:
   *         { guid, name, number, expMonth, expYear, type }
   */
  fetchCreditCards() {
    debug`fetchCreditCards`;

    return lazy.EventDispatcher.instance.sendRequestForResult({
      type: "GeckoView:Autocomplete:Fetch:CreditCard",
    });
  },

  /**
   * Delegates address entry fetching to the attached LoginStorage
   * GeckoView delegate.
   *
   * @return {Promise}
   *         Resolves with an array of address objects or null.
   *         Rejected if no delegate is attached.
   *         Login object string properties:
   *         { guid, name, givenName, additionalName, familyName,
   *           organization, streetAddress, addressLevel1, addressLevel2,
   *           addressLevel3, postalCode, country, tel, email }
   */
  fetchAddresses() {
    debug`fetchAddresses`;

    return lazy.EventDispatcher.instance.sendRequestForResult({
      type: "GeckoView:Autocomplete:Fetch:Address",
    });
  },

  /**
   * Delegates credit card entry saving to the attached LoginStorage GeckoView delegate.
   * Call this when a new or modified credit card entry has been submitted.
   *
   * @param aCreditCard The {CreditCard} to be saved.
   */
  onCreditCardSave(aCreditCard) {
    debug`onCreditCardSave ${aCreditCard}`;

    lazy.EventDispatcher.instance.sendRequest({
      type: "GeckoView:Autocomplete:Save:CreditCard",
      creditCard: aCreditCard,
    });
  },

  /**
   * Delegates address entry saving to the attached LoginStorage GeckoView delegate.
   * Call this when a new or modified address entry has been submitted.
   *
   * @param aAddress The {Address} to be saved.
   */
  onAddressSave(aAddress) {
    debug`onAddressSave ${aAddress}`;

    lazy.EventDispatcher.instance.sendRequest({
      type: "GeckoView:Autocomplete:Save:Address",
      address: aAddress,
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

    lazy.EventDispatcher.instance.sendRequest({
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

    lazy.EventDispatcher.instance.sendRequest({
      type: "GeckoView:Autocomplete:Used:Login",
      usedFields: UsedField.PASSWORD,
      login: aLogin,
    });
  },

  _numActiveSelections: 0,

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

      const prompt = new lazy.GeckoViewPrompter(aBrowser.ownerGlobal);
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
      this._prompt = prompt;
    });
  },

  /**
   * Delegates credit card entry selection.
   * Call this when there are multiple credit card entry option for a form to delegate
   * the selection.
   *
   * @param aBrowser The browser instance the triggered the selection.
   * @param aOptions The list of {SelectOption} depicting viable options.
   */
  onCreditCardSelect(aBrowser, aOptions) {
    debug`onCreditCardSelect ${aOptions}`;

    return new Promise((resolve, reject) => {
      if (!aBrowser || !aOptions) {
        debug`onCreditCardSelect Rejecting - no browser or options provided`;
        reject();
        return;
      }

      const prompt = new lazy.GeckoViewPrompter(aBrowser.ownerGlobal);
      prompt.asyncShowPrompt(
        {
          type: "Autocomplete:Select:CreditCard",
          options: aOptions,
        },
        result => {
          if (!result || !result.selection) {
            reject();
            return;
          }

          const option = new SelectOption({
            value: CreditCard.parse(result.selection.value),
            hint: result.selection.hint,
          });
          resolve(option);
        }
      );
      this._prompt = prompt;
    });
  },

  /**
   * Delegates address entry selection.
   * Call this when there are multiple address entry option for a form to delegate
   * the selection.
   *
   * @param aBrowser The browser instance the triggered the selection.
   * @param aOptions The list of {SelectOption} depicting viable options.
   */
  onAddressSelect(aBrowser, aOptions) {
    debug`onAddressSelect ${aOptions}`;

    return new Promise((resolve, reject) => {
      if (!aBrowser || !aOptions) {
        debug`onAddressSelect Rejecting - no browser or options provided`;
        reject();
        return;
      }

      const prompt = new lazy.GeckoViewPrompter(aBrowser.ownerGlobal);
      prompt.asyncShowPrompt(
        {
          type: "Autocomplete:Select:Address",
          options: aOptions,
        },
        result => {
          if (!result || !result.selection) {
            reject();
            return;
          }

          const option = new SelectOption({
            value: Address.parse(result.selection.value),
            hint: result.selection.hint,
          });
          resolve(option);
        }
      );
      this._prompt = prompt;
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

    // TODO: Replace this string with more robust mechanics.
    let selectionType = null;
    const selectOptions = [];

    for (const option of options) {
      switch (option.style) {
        case "insecureWarning": {
          // We depend on the insecure warning to be the first option.
          insecureHint = SelectOption.Hint.INSECURE_FORM;
          break;
        }
        case "generatedPassword": {
          selectionType = "login";
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
          selectionType = "login";
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
        case "autofill-profile": {
          const comment = JSON.parse(option.comment);
          debug`delegateSelection ${comment}`;
          const creditCard = CreditCard.fromGecko(comment);
          const address = Address.fromGecko(comment);
          if (creditCard.isValid()) {
            selectionType = "creditCard";
            selectOptions.push(
              new SelectOption({
                value: creditCard,
                hint: insecureHint,
              })
            );
          } else if (address.isValid()) {
            selectionType = "address";
            selectOptions.push(
              new SelectOption({
                value: address,
                hint: insecureHint,
              })
            );
          }
          break;
        }
        default:
          debug`delegateSelection - ignoring unknown option style ${option.style}`;
      }
    }

    if (selectOptions.length < 1) {
      debug`Abort delegateSelection - no valid options provided`;
      return;
    }

    if (this._numActiveSelections > 0) {
      debug`Abort delegateSelection - there is already one delegation active`;
      return;
    }

    ++this._numActiveSelections;

    let selectedOption = null;
    const browser = browsingContext.top.embedderElement;
    if (selectionType === "login") {
      selectedOption = await this.onLoginSelect(browser, selectOptions).catch(
        _ => {
          debug`No GV delegate attached`;
        }
      );
    } else if (selectionType === "creditCard") {
      selectedOption = await this.onCreditCardSelect(
        browser,
        selectOptions
      ).catch(_ => {
        debug`No GV delegate attached`;
      });
    } else if (selectionType === "address") {
      selectedOption = await this.onAddressSelect(browser, selectOptions).catch(
        _ => {
          debug`No GV delegate attached`;
        }
      );
    }

    // prompt is closed now.
    this._prompt = null;

    --this._numActiveSelections;

    debug`delegateSelection selected option: ${selectedOption}`;

    if (selectionType === "login") {
      const selectedLogin = selectedOption?.value?.toLoginInfo();

      if (!selectedLogin) {
        debug`Abort delegateSelection - no login entry selected`;
        return;
      }

      debug`delegateSelection - filling form`;

      const actor =
        browsingContext.currentWindowGlobal.getActor("LoginManager");

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
    } else if (selectionType === "creditCard") {
      const selectedCreditCard = selectedOption?.value?.toGecko();
      const actor =
        browsingContext.currentWindowGlobal.getActor("FormAutofill");

      actor.sendAsyncMessage("FormAutofill:FillForm", selectedCreditCard);
    } else if (selectionType === "address") {
      const selectedAddress = selectedOption?.value?.toGecko();
      const actor =
        browsingContext.currentWindowGlobal.getActor("FormAutofill");

      actor.sendAsyncMessage("FormAutofill:FillForm", selectedAddress);
    }

    debug`delegateSelection - form filled`;
  },

  delegateDismiss() {
    debug`delegateDismiss`;

    this._prompt?.dismiss();
  },
};

const { debug } = GeckoViewUtils.initLogging("GeckoViewAutocomplete");
