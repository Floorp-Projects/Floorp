/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implements an interface of the storage of Form Autofill.
 */

"use strict";

// We expose a singleton from this module. Some tests may import the
// constructor via a backstage pass.
const EXPORTED_SYMBOLS = ["formAutofillStorage", "FormAutofillStorage"];

const { FormAutofill } = ChromeUtils.import(
  "resource://autofill/FormAutofill.jsm"
);

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

const {
  FormAutofillStorageBase,
  CreditCardsBase,
  AddressesBase,
} = ChromeUtils.import("resource://autofill/FormAutofillStorageBase.jsm");

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  CreditCard: "resource://gre/modules/CreditCard.jsm",
  FormAutofillUtils: "resource://autofill/FormAutofillUtils.jsm",
  JSONFile: "resource://gre/modules/JSONFile.jsm",
  OSKeyStore: "resource://gre/modules/OSKeyStore.jsm",
});

const PROFILE_JSON_FILE_NAME = "autofill-profiles.json";

class Addresses extends AddressesBase {
  /**
   * Merge new address into the specified address if mergeable.
   *
   * @param  {string} guid
   *         Indicates which address to merge.
   * @param  {Object} address
   *         The new address used to merge into the old one.
   * @param  {boolean} strict
   *         In strict merge mode, we'll treat the subset record with empty field
   *         as unable to be merged, but mergeable if in non-strict mode.
   * @returns {Promise<boolean>}
   *          Return true if address is merged into target with specific guid or false if not.
   */
  async mergeIfPossible(guid, address, strict) {
    this.log.debug(`mergeIfPossible: ${guid}`);

    let addressFound = this._findByGUID(guid);
    if (!addressFound) {
      throw new Error("No matching address.");
    }

    let addressToMerge = this._clone(address);
    this._normalizeRecord(addressToMerge, strict);
    let hasMatchingField = false;

    let country =
      addressFound.country ||
      addressToMerge.country ||
      FormAutofill.DEFAULT_REGION;
    let collators = lazy.FormAutofillUtils.getSearchCollators(country);
    for (let field of this.VALID_FIELDS) {
      let existingField = addressFound[field];
      let incomingField = addressToMerge[field];
      if (incomingField !== undefined && existingField !== undefined) {
        if (incomingField != existingField) {
          // Treat "street-address" as mergeable if their single-line versions
          // match each other.
          if (
            field == "street-address" &&
            lazy.FormAutofillUtils.compareStreetAddress(
              existingField,
              incomingField,
              collators
            )
          ) {
            // Keep the street-address in storage if its amount of lines is greater than
            // or equal to the incoming one.
            if (
              existingField.split("\n").length >=
              incomingField.split("\n").length
            ) {
              // Replace the incoming field with the one in storage so it will
              // be further merged back to storage.
              addressToMerge[field] = existingField;
            }
          } else if (
            field != "street-address" &&
            lazy.FormAutofillUtils.strCompare(
              existingField,
              incomingField,
              collators
            )
          ) {
            addressToMerge[field] = existingField;
          } else {
            this.log.debug("Conflicts: field", field, "has different value.");
            return false;
          }
        }
        hasMatchingField = true;
      }
    }

    // We merge the address only when at least one field has the same value.
    if (!hasMatchingField) {
      this.log.debug("Unable to merge because no field has the same value");
      return false;
    }

    // Early return if the data is the same or subset.
    let noNeedToUpdate = this.VALID_FIELDS.every(field => {
      // When addressFound doesn't contain a field, it's unnecessary to update
      // if the same field in addressToMerge is omitted or an empty string.
      if (addressFound[field] === undefined) {
        return !addressToMerge[field];
      }

      // When addressFound contains a field, it's unnecessary to update if
      // the same field in addressToMerge is omitted or a duplicate.
      return (
        addressToMerge[field] === undefined ||
        addressFound[field] === addressToMerge[field]
      );
    });
    if (noNeedToUpdate) {
      return true;
    }

    await this.update(guid, addressToMerge, true);
    return true;
  }
}

class CreditCards extends CreditCardsBase {
  constructor(store) {
    super(store);
  }

  async _encryptNumber(creditCard) {
    if (!("cc-number-encrypted" in creditCard)) {
      if ("cc-number" in creditCard) {
        let ccNumber = creditCard["cc-number"];
        if (lazy.CreditCard.isValidNumber(ccNumber)) {
          creditCard["cc-number"] = lazy.CreditCard.getLongMaskedNumber(
            ccNumber
          );
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

  /**
   * Merge new credit card into the specified record if cc-number is identical.
   * (Note that credit card records always do non-strict merge.)
   *
   * @param  {string} guid
   *         Indicates which credit card to merge.
   * @param  {Object} creditCard
   *         The new credit card used to merge into the old one.
   * @returns {boolean}
   *          Return true if credit card is merged into target with specific guid or false if not.
   */
  async mergeIfPossible(guid, creditCard) {
    this.log.debug(`mergeIfPossible: ${guid}`);

    // Credit card number is required since it also must match.
    if (!creditCard["cc-number"]) {
      return false;
    }

    // Query raw data for comparing the decrypted credit card number
    let creditCardFound = await this.get(guid, { rawData: true });
    if (!creditCardFound) {
      throw new Error("No matching credit card.");
    }

    let creditCardToMerge = this._clone(creditCard);
    this._normalizeRecord(creditCardToMerge);

    for (let field of this.VALID_FIELDS) {
      let existingField = creditCardFound[field];

      // Make sure credit card field is existed and have value
      if (
        field == "cc-number" &&
        (!existingField || !creditCardToMerge[field])
      ) {
        return false;
      }

      if (!creditCardToMerge[field] && typeof existingField != "undefined") {
        creditCardToMerge[field] = existingField;
      }

      let incomingField = creditCardToMerge[field];
      if (incomingField && existingField) {
        if (incomingField != existingField) {
          this.log.debug("Conflicts: field", field, "has different value.");
          return false;
        }
      }
    }

    // Early return if the data is the same.
    let exactlyMatch = this.VALID_FIELDS.every(
      field => creditCardFound[field] === creditCardToMerge[field]
    );
    if (exactlyMatch) {
      return true;
    }

    await this.update(guid, creditCardToMerge, true);
    return true;
  }
}

class FormAutofillStorage extends FormAutofillStorageBase {
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
const formAutofillStorage = new FormAutofillStorage(
  OS.Path.join(OS.Constants.Path.profileDir, PROFILE_JSON_FILE_NAME)
);
