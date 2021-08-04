/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implements an interface of the storage of Form Autofill for GeckoView.
 */

"use strict";

// We expose a singleton from this module. Some tests may import the
// constructor via a backstage pass.
this.EXPORTED_SYMBOLS = ["formAutofillStorage", "FormAutofillStorage"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const {
  FormAutofillStorageBase,
  CreditCardsBase,
  AddressesBase,
} = ChromeUtils.import("resource://autofill/FormAutofillStorageBase.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  GeckoViewAutocomplete: "resource://gre/modules/GeckoViewAutocomplete.jsm",
  CreditCard: "resource://gre/modules/GeckoViewAutocomplete.jsm",
  Address: "resource://gre/modules/GeckoViewAutocomplete.jsm",
  JSONFile: "resource://gre/modules/JSONFile.jsm",
});

class GeckoViewStorage extends JSONFile {
  constructor() {
    super({ path: null });
  }

  async updateCreditCards() {
    const creditCards = await GeckoViewAutocomplete.fetchCreditCards().then(
      results => results?.map(r => CreditCard.parse(r).toGecko()) ?? [],
      _ => []
    );
    super.data.creditCards = creditCards;
  }

  async updateAddresses() {
    const addresses = await GeckoViewAutocomplete.fetchAddresses().then(
      results => results?.map(r => Address.parse(r).toGecko()) ?? [],
      _ => []
    );
    super.data.addresses = addresses;
  }

  async load() {
    super.data = { creditCards: {}, addresses: {} };
    await this.updateCreditCards();
    await this.updateAddresses();
  }

  ensureDataReady() {
    if (this.dataReady) {
      return;
    }
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  async _save() {
    // TODO: Implement saving support in bug 1703977.
  }
}

class Addresses extends AddressesBase {
  // Override AutofillRecords methods.

  _initialize() {
    this._initializePromise = Promise.resolve();
  }

  async _saveRecord(record, { sourceSync = false } = {}) {
    GeckoViewAutocomplete.onAddressSave(Address.fromGecko(record));
  }

  /**
   * Returns the record with the specified GUID.
   *
   * @param   {string} guid
   *          Indicates which record to retrieve.
   * @param   {boolean} [options.rawData = false]
   *          Returns a raw record without modifications and the computed fields
   *          (this includes private fields)
   * @returns {Promise<Object>}
   *          A clone of the record.
   */
  async get(guid, { rawData = false } = {}) {
    await this._store.updateAddresses();
    return super.get(guid, { rawData });
  }

  /**
   * Returns all records.
   *
   * @param   {boolean} [options.rawData = false]
   *          Returns raw records without modifications and the computed fields.
   * @param   {boolean} [options.includeDeleted = false]
   *          Also return any tombstone records.
   * @returns {Promise<Array.<Object>>}
   *          An array containing clones of all records.
   */
  async getAll({ rawData = false, includeDeleted = false } = {}) {
    await this._store.updateAddresses();
    return super.getAll({ rawData, includeDeleted });
  }

  /**
   * Return all saved field names in the collection.
   *
   * @returns {Set} Set containing saved field names.
   */
  async getSavedFieldNames() {
    await this._store.updateAddresses();
    return super.getSavedFieldNames();
  }

  async reconcile(remoteRecord) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  async findDuplicateGUID(remoteRecord) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  async mergeToStorage(targetRecord, strict = false) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }
}

class CreditCards extends CreditCardsBase {
  async _encryptNumber(creditCard) {
    // Don't encrypt or obfuscate for GV, since we don't store or show
    // the number. The API has to always provide the original number.
  }

  // Override AutofillRecords methods.

  _initialize() {
    this._initializePromise = Promise.resolve();
  }

  async _saveRecord(record, { sourceSync = false } = {}) {
    GeckoViewAutocomplete.onCreditCardSave(CreditCard.fromGecko(record));
  }

  /**
   * Returns the record with the specified GUID.
   *
   * @param   {string} guid
   *          Indicates which record to retrieve.
   * @param   {boolean} [options.rawData = false]
   *          Returns a raw record without modifications and the computed fields
   *          (this includes private fields)
   * @returns {Promise<Object>}
   *          A clone of the record.
   */
  async get(guid, { rawData = false } = {}) {
    await this._store.updateCreditCards();
    return super.get(guid, { rawData });
  }

  /**
   * Returns all records.
   *
   * @param   {boolean} [options.rawData = false]
   *          Returns raw records without modifications and the computed fields.
   * @param   {boolean} [options.includeDeleted = false]
   *          Also return any tombstone records.
   * @returns {Promise<Array.<Object>>}
   *          An array containing clones of all records.
   */
  async getAll({ rawData = false, includeDeleted = false } = {}) {
    await this._store.updateCreditCards();
    return super.getAll({ rawData, includeDeleted });
  }

  /**
   * Return all saved field names in the collection.
   *
   * @returns {Set} Set containing saved field names.
   */
  async getSavedFieldNames() {
    await this._store.updateCreditCards();
    return super.getSavedFieldNames();
  }

  async reconcile(remoteRecord) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  async findDuplicateGUID(remoteRecord) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  async mergeToStorage(targetRecord, strict = false) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }
}

class FormAutofillStorage extends FormAutofillStorageBase {
  constructor() {
    super(null);
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
   * Initializes the in-memory async store API.
   * @returns {JSONFile}
   *          The JSONFile store.
   */
  _initializeStore() {
    return new GeckoViewStorage();
  }
}

// The singleton exposed by this module.
this.formAutofillStorage = new FormAutofillStorage();
