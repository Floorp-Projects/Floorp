/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implements an interface of the storage of Form Autofill.
 */

// We expose a singleton from this module. Some tests may import the
// constructor via a backstage pass.
import {
  AddressesBase,
  CreditCardsBase,
  FormAutofillStorageBase,
} from "resource://autofill/FormAutofillStorageBase.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  CreditCard: "resource://gre/modules/CreditCard.sys.mjs",
  JSONFile: "resource://gre/modules/JSONFile.sys.mjs",
  OSKeyStore: "resource://gre/modules/OSKeyStore.sys.mjs",
});

const PROFILE_JSON_FILE_NAME = "autofill-profiles.json";

class Addresses extends AddressesBase {}

class CreditCards extends CreditCardsBase {
  constructor(store) {
    super(store);
  }

  async _encryptNumber(creditCard) {
    if (!("cc-number-encrypted" in creditCard)) {
      if ("cc-number" in creditCard) {
        let ccNumber = creditCard["cc-number"];
        if (lazy.CreditCard.isValidNumber(ccNumber)) {
          creditCard["cc-number"] =
            lazy.CreditCard.getLongMaskedNumber(ccNumber);
        } else {
          // Credit card numbers can be entered on versions of Firefox that don't validate
          // the number and then synced to this version of Firefox. Therefore, mask the
          // full number if the number is invalid on this version.
          creditCard["cc-number"] = "*".repeat(ccNumber.length);
        }
        creditCard["cc-number-encrypted"] = await lazy.OSKeyStore.encrypt(
          ccNumber
        );
      } else {
        creditCard["cc-number-encrypted"] = "";
      }
    }
  }
}

export class FormAutofillStorage extends FormAutofillStorageBase {
  constructor(path) {
    super(path);
  }

  getAddresses() {
    if (!this._addresses) {
      this._store.ensureDataReady();
      this._addresses = new Addresses(this._store);
    }
    return this._addresses;
  }

  getCreditCards() {
    if (!this._creditCards) {
      this._store.ensureDataReady();
      this._creditCards = new CreditCards(this._store);
    }
    return this._creditCards;
  }

  /**
   * Loads the profile data from file to memory.
   *
   * @returns {JSONFile}
   *          The JSONFile store.
   */
  _initializeStore() {
    return new lazy.JSONFile({
      path: this._path,
      dataPostProcessor: this._dataPostProcessor.bind(this),
    });
  }

  _dataPostProcessor(data) {
    data.version = this.version;
    if (!data.addresses) {
      data.addresses = [];
    }
    if (!data.creditCards) {
      data.creditCards = [];
    }
    return data;
  }
}

// The singleton exposed by this module.
export const formAutofillStorage = new FormAutofillStorage(
  PathUtils.join(PathUtils.profileDir, PROFILE_JSON_FILE_NAME)
);
