/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = [
  "GeckoViewAutocomplete",
  "LoginEntry",
  "CreditCard",
  "Address",
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

class Address {
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
    this.name = name ?? null;
    this.givenName = givenName ?? null;
    this.additionalName = additionalName ?? null;
    this.familyName = familyName ?? null;
    this.organization = organization ?? null;
    this.streetAddress = streetAddress ?? null;
    this.addressLevel1 = addressLevel1 ?? null;
    this.addressLevel2 = addressLevel2 ?? null;
    this.addressLevel3 = addressLevel3 ?? null;
    this.postalCode = postalCode ?? null;
    this.country = country ?? null;
    this.tel = tel ?? null;
    this.email = email ?? null;

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
      (this.name ?? this.givenName ?? this.familyName) !== null &&
      this.streetAddress !== null &&
      this.postalCode !== null
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

class CreditCard {
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
    this.name = name ?? null;
    this.number = number ?? null;
    this.expMonth = expMonth ?? null;
    this.expYear = expYear ?? null;
    this.type = type ?? null;

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
      this.name !== null &&
      this.number !== null &&
      this.expMonth !== null &&
      this.expYear !== null
    );
  }

  static fromGecko(aObj) {
    return new CreditCard({
      version: aObj.version,
      name: aObj["cc-name"],
      number: aObj["cc-number"],
      expMonth: aObj["cc-exp-month"],
      expYear: aObj["cc-exp-year"],
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

class SelectOption {
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

const GeckoViewAutocomplete = {
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

    return EventDispatcher.instance.sendRequestForResult({
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

    return EventDispatcher.instance.sendRequestForResult({
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

    return EventDispatcher.instance.sendRequestForResult({
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

    EventDispatcher.instance.sendRequest({
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

    EventDispatcher.instance.sendRequest({
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

      const prompt = new GeckoViewPrompter(aBrowser.ownerGlobal);
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

      const prompt = new GeckoViewPrompter(aBrowser.ownerGlobal);
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

    --this._numActiveSelections;

    debug`delegateSelection selected option: ${selectedOption}`;

    if (selectionType === "login") {
      const selectedLogin = selectedOption?.value?.toLoginInfo();

      if (!selectedLogin) {
        debug`Abort delegateSelection - no login entry selected`;
        return;
      }

      debug`delegateSelection - filling form`;

      const actor = browsingContext.currentWindowGlobal.getActor(
        "LoginManager"
      );

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
      const actor = browsingContext.currentWindowGlobal.getActor(
        "FormAutofill"
      );

      actor.sendAsyncMessage("FormAutofill:FillForm", selectedCreditCard);
    } else if (selectionType === "address") {
      const selectedAddress = selectedOption?.value?.toGecko();
      const actor = browsingContext.currentWindowGlobal.getActor(
        "FormAutofill"
      );

      actor.sendAsyncMessage("FormAutofill:FillForm", selectedAddress);
    }

    debug`delegateSelection - form filled`;
  },
};

const { debug } = GeckoViewUtils.initLogging("GeckoViewAutocomplete");
