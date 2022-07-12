/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Interface for the storage of Form Autofill.
 *
 * The data is stored in JSON format, without indentation and the computed
 * fields, using UTF-8 encoding. With indentation and computed fields applied,
 * the schema would look like this:
 *
 * {
 *   version: 1,
 *   addresses: [
 *     {
 *       guid,                 // 12 characters
 *       version,              // schema version in integer
 *
 *       // address fields
 *       given-name,
 *       additional-name,
 *       family-name,
 *       organization,         // Company
 *       street-address,       // (Multiline)
 *       address-level3,       // Suburb/Sublocality
 *       address-level2,       // City/Town
 *       address-level1,       // Province (Standardized code if possible)
 *       postal-code,
 *       country,              // ISO 3166
 *       tel,                  // Stored in E.164 format
 *       email,
 *
 *       // computed fields (These fields are computed based on the above fields
 *       // and are not allowed to be modified directly.)
 *       name,
 *       address-line1,
 *       address-line2,
 *       address-line3,
 *       country-name,
 *       tel-country-code,
 *       tel-national,
 *       tel-area-code,
 *       tel-local,
 *       tel-local-prefix,
 *       tel-local-suffix,
 *
 *       // metadata
 *       timeCreated,          // in ms
 *       timeLastUsed,         // in ms
 *       timeLastModified,     // in ms
 *       timesUsed
 *       _sync: { ... optional sync metadata },
 *     }
 *   ],
 *   creditCards: [
 *     {
 *       guid,                 // 12 characters
 *       version,              // schema version in integer
 *
 *       // credit card fields
 *       billingAddressGUID,   // An optional GUID of an autofill address record
 *                                which may or may not exist locally.
 *
 *       cc-name,
 *       cc-number,            // will be stored in masked format (************1234)
 *                             // (see details below)
 *       cc-exp-month,
 *       cc-exp-year,          // 2-digit year will be converted to 4 digits
 *                             // upon saving
 *       cc-type,              // Optional card network id (instrument type)
 *
 *       // computed fields (These fields are computed based on the above fields
 *       // and are not allowed to be modified directly.)
 *       cc-given-name,
 *       cc-additional-name,
 *       cc-family-name,
 *       cc-number-encrypted,  // encrypted from the original unmasked "cc-number"
 *                             // (see details below)
 *       cc-exp,
 *
 *       // metadata
 *       timeCreated,          // in ms
 *       timeLastUsed,         // in ms
 *       timeLastModified,     // in ms
 *       timesUsed
 *       _sync: { ... optional sync metadata },
 *     }
 *   ]
 * }
 *
 *
 * Encrypt-related Credit Card Fields (cc-number & cc-number-encrypted):
 *
 * When saving or updating a credit-card record, the storage will encrypt the
 * value of "cc-number", store the encrypted number in "cc-number-encrypted"
 * field, and replace "cc-number" field with the masked number. These all happen
 * in "computeFields". We do reverse actions in "_stripComputedFields", which
 * decrypts "cc-number-encrypted", restores it to "cc-number", and deletes
 * "cc-number-encrypted". Therefore, calling "_stripComputedFields" followed by
 * "computeFields" can make sure the encrypt-related fields are up-to-date.
 *
 * In general, you have to decrypt the number by your own outside FormAutofillStorage
 * when necessary. However, you will get the decrypted records when querying
 * data with "rawData=true" to ensure they're ready to sync.
 *
 *
 * Sync Metadata:
 *
 * Records may also have a _sync field, which consists of:
 * {
 *   changeCounter,    // integer - the number of changes made since the last
 *                     // sync.
 *   lastSyncedFields, // object - hashes of the original values for fields
 *                     // changed since the last sync.
 * }
 *
 * Records with such a field have previously been synced. Records without such
 * a field are yet to be synced, so are treated specially in some cases (eg,
 * they don't need a tombstone, de-duping logic treats them as special etc).
 * Records without the field are always considered "dirty" from Sync's POV
 * (meaning they will be synced on the next sync), at which time they will gain
 * this new field.
 */

"use strict";

const EXPORTED_SYMBOLS = [
  "FormAutofillStorageBase",
  "CreditCardsBase",
  "AddressesBase",
];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { FormAutofill } = ChromeUtils.import(
  "resource://autofill/FormAutofill.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  CreditCard: "resource://gre/modules/CreditCard.jsm",
  FormAutofillNameUtils: "resource://autofill/FormAutofillNameUtils.jsm",
  FormAutofillUtils: "resource://autofill/FormAutofillUtils.jsm",
  OSKeyStore: "resource://gre/modules/OSKeyStore.jsm",
  PhoneNumber: "resource://autofill/phonenumberutils/PhoneNumber.jsm",
});

const CryptoHash = Components.Constructor(
  "@mozilla.org/security/hash;1",
  "nsICryptoHash",
  "initWithString"
);

const STORAGE_SCHEMA_VERSION = 1;
const ADDRESS_SCHEMA_VERSION = 1;
const CREDIT_CARD_SCHEMA_VERSION = 3;

const VALID_ADDRESS_FIELDS = [
  "given-name",
  "additional-name",
  "family-name",
  "organization",
  "street-address",
  "address-level3",
  "address-level2",
  "address-level1",
  "postal-code",
  "country",
  "tel",
  "email",
];

const STREET_ADDRESS_COMPONENTS = [
  "address-line1",
  "address-line2",
  "address-line3",
];

const TEL_COMPONENTS = [
  "tel-country-code",
  "tel-national",
  "tel-area-code",
  "tel-local",
  "tel-local-prefix",
  "tel-local-suffix",
];

const VALID_ADDRESS_COMPUTED_FIELDS = ["name", "country-name"].concat(
  STREET_ADDRESS_COMPONENTS,
  TEL_COMPONENTS
);

const VALID_CREDIT_CARD_FIELDS = [
  "billingAddressGUID",
  "cc-name",
  "cc-number",
  "cc-exp-month",
  "cc-exp-year",
  "cc-type",
];

const VALID_CREDIT_CARD_COMPUTED_FIELDS = [
  "cc-given-name",
  "cc-additional-name",
  "cc-family-name",
  "cc-number-encrypted",
  "cc-exp",
];

const INTERNAL_FIELDS = [
  "guid",
  "version",
  "timeCreated",
  "timeLastUsed",
  "timeLastModified",
  "timesUsed",
];

function sha512(string) {
  if (string == null) {
    return null;
  }
  let encoder = new TextEncoder("utf-8");
  let bytes = encoder.encode(string);
  let hash = new CryptoHash("sha512");
  hash.update(bytes, bytes.length);
  return hash.finish(/* base64 */ true);
}

/**
 * Class that manipulates records in a specified collection.
 *
 * Note that it is responsible for converting incoming data to a consistent
 * format in the storage. For example, computed fields will be transformed to
 * the original fields and 2-digit years will be calculated into 4 digits.
 */
class AutofillRecords {
  /**
   * Creates an AutofillRecords.
   *
   * @param {JSONFile} store
   *        An instance of JSONFile.
   * @param {string} collectionName
   *        A key of "store.data".
   * @param {Array.<string>} validFields
   *        A list containing non-metadata field names.
   * @param {Array.<string>} validComputedFields
   *        A list containing computed field names.
   * @param {number} schemaVersion
   *        The schema version for the new record.
   */
  constructor(
    store,
    collectionName,
    validFields,
    validComputedFields,
    schemaVersion
  ) {
    this.log = FormAutofill.defineLogGetter(
      lazy,
      "AutofillRecords:" + collectionName
    );

    this.VALID_FIELDS = validFields;
    this.VALID_COMPUTED_FIELDS = validComputedFields;

    this._store = store;
    this._collectionName = collectionName;
    this._schemaVersion = schemaVersion;

    this._initialize();
  }

  _initialize() {
    this._initializePromise = Promise.all(
      this._data.map(async (record, index) =>
        this._migrateRecord(record, index)
      )
    ).then(hasChangesArr => {
      let dataHasChanges = hasChangesArr.includes(true);
      if (dataHasChanges) {
        this._store.saveSoon();
      }
    });
  }

  /**
   * Gets the schema version number.
   *
   * @returns {number}
   *          The current schema version number.
   */
  get version() {
    return this._schemaVersion;
  }

  /**
   * Gets the data of this collection.
   *
   * @returns {array}
   *          The data object.
   */
  get _data() {
    return this._getData();
  }

  _getData() {
    return this._store.data[this._collectionName];
  }

  // Ensures that we don't try to apply synced records with newer schema
  // versions. This is a temporary measure to ensure we don't accidentally
  // bump the schema version without a syncing strategy in place (bug 1377204).
  _ensureMatchingVersion(record) {
    if (record.version != this.version) {
      throw new Error(
        `Got unknown record version ${record.version}; want ${this.version}`
      );
    }
  }

  /**
   * Initialize the records in the collection, resolves when the migration completes.
   * @returns {Promise}
   */
  initialize() {
    return this._initializePromise;
  }

  /**
   * Adds a new record.
   *
   * @param {Object} record
   *        The new record for saving.
   * @param {boolean} [options.sourceSync = false]
   *        Did sync generate this addition?
   * @returns {Promise<string>}
   *          The GUID of the newly added item..
   */
  async add(record, { sourceSync = false } = {}) {
    let recordToSave = this._clone(record);

    if (sourceSync) {
      // Remove tombstones for incoming items that were changed on another
      // device. Local deletions always lose to avoid data loss.
      let index = this._findIndexByGUID(recordToSave.guid, {
        includeDeleted: true,
      });
      if (index > -1) {
        let existing = this._data[index];
        if (existing.deleted) {
          this._data.splice(index, 1);
        } else {
          throw new Error(`Record ${recordToSave.guid} already exists`);
        }
      }
    } else if (!recordToSave.deleted) {
      this._normalizeRecord(recordToSave);
      // _normalizeRecord shouldn't do any validation (throw) because in the
      // `update` case it is called with partial records whereas
      // `_validateFields` is called with a complete one.
      this._validateFields(recordToSave);

      recordToSave.guid = this._generateGUID();
      recordToSave.version = this.version;

      // Metadata
      let now = Date.now();
      recordToSave.timeCreated = now;
      recordToSave.timeLastModified = now;
      recordToSave.timeLastUsed = 0;
      recordToSave.timesUsed = 0;
    }

    return this._saveRecord(recordToSave, { sourceSync });
  }

  async _saveRecord(record, { sourceSync = false } = {}) {
    if (!record.guid) {
      throw new Error("Record missing GUID");
    }

    let recordToSave;
    if (record.deleted) {
      if (this._findByGUID(record.guid, { includeDeleted: true })) {
        throw new Error("a record with this GUID already exists");
      }
      recordToSave = {
        guid: record.guid,
        timeLastModified: record.timeLastModified || Date.now(),
        deleted: true,
      };
    } else {
      this._ensureMatchingVersion(record);
      recordToSave = record;
      await this.computeFields(recordToSave);
    }

    if (sourceSync) {
      let sync = this._getSyncMetaData(recordToSave, true);
      sync.changeCounter = 0;
    }

    this._data.push(recordToSave);

    this.updateUseCountTelemetry();

    this._store.saveSoon();

    Services.obs.notifyObservers(
      {
        wrappedJSObject: {
          sourceSync,
          guid: record.guid,
          collectionName: this._collectionName,
        },
      },
      "formautofill-storage-changed",
      "add"
    );
    return recordToSave.guid;
  }

  _generateGUID() {
    let guid;
    while (!guid || this._findByGUID(guid)) {
      guid = Services.uuid
        .generateUUID()
        .toString()
        .replace(/[{}-]/g, "")
        .substring(0, 12);
    }
    return guid;
  }

  /**
   * Update the specified record.
   *
   * @param  {string} guid
   *         Indicates which record to update.
   * @param  {Object} record
   *         The new record used to overwrite the old one.
   * @param  {Promise<boolean>} [preserveOldProperties = false]
   *         Preserve old record's properties if they don't exist in new record.
   */
  async update(guid, record, preserveOldProperties = false) {
    this.log.debug(`update: ${guid}`);

    let recordFoundIndex = this._findIndexByGUID(guid);
    if (recordFoundIndex == -1) {
      throw new Error("No matching record.");
    }

    // Clone the record before modifying it to avoid exposing incomplete changes.
    let recordFound = this._clone(this._data[recordFoundIndex]);
    await this._stripComputedFields(recordFound);

    let recordToUpdate = this._clone(record);
    this._normalizeRecord(recordToUpdate, true);

    let hasValidField = false;
    for (let field of this.VALID_FIELDS) {
      let oldValue = recordFound[field];
      let newValue = recordToUpdate[field];

      // Resume the old field value in the perserve case
      if (preserveOldProperties && newValue === undefined) {
        newValue = oldValue;
      }

      if (newValue === undefined || newValue === "") {
        delete recordFound[field];
      } else {
        hasValidField = true;
        recordFound[field] = newValue;
      }

      this._maybeStoreLastSyncedField(recordFound, field, oldValue);
    }

    if (!hasValidField) {
      throw new Error("Record contains no valid field.");
    }

    // _normalizeRecord above is called with the `record` argument provided to
    // `update` which may not contain all resulting fields when
    // `preserveOldProperties` is used. This means we need to validate for
    // missing fields after we compose the record (`recordFound`) with the stored
    // record like we do in the loop above.
    this._validateFields(recordFound);

    recordFound.timeLastModified = Date.now();
    let syncMetadata = this._getSyncMetaData(recordFound);
    if (syncMetadata) {
      syncMetadata.changeCounter += 1;
    }

    await this.computeFields(recordFound);
    this._data[recordFoundIndex] = recordFound;

    this._store.saveSoon();

    Services.obs.notifyObservers(
      {
        wrappedJSObject: {
          guid,
          collectionName: this._collectionName,
        },
      },
      "formautofill-storage-changed",
      "update"
    );
  }

  /**
   * Notifies the storage of the use of the specified record, so we can update
   * the metadata accordingly. This does not bump the Sync change counter, since
   * we don't sync `timesUsed` or `timeLastUsed`.
   *
   * @param  {string} guid
   *         Indicates which record to be notified.
   */
  notifyUsed(guid) {
    this.log.debug("notifyUsed:", guid);

    let recordFound = this._findByGUID(guid);
    if (!recordFound) {
      throw new Error("No matching record.");
    }

    recordFound.timesUsed++;
    recordFound.timeLastUsed = Date.now();

    this.updateUseCountTelemetry();

    this._store.saveSoon();
    Services.obs.notifyObservers(
      {
        wrappedJSObject: {
          guid,
          collectionName: this._collectionName,
        },
      },
      "formautofill-storage-changed",
      "notifyUsed"
    );
  }

  updateUseCountTelemetry() {}

  /**
   * Removes the specified record. No error occurs if the record isn't found.
   *
   * @param  {string} guid
   *         Indicates which record to remove.
   * @param  {boolean} [options.sourceSync = false]
   *         Did Sync generate this removal?
   */
  remove(guid, { sourceSync = false } = {}) {
    this.log.debug("remove:", guid);

    if (sourceSync) {
      this._removeSyncedRecord(guid);
    } else {
      let index = this._findIndexByGUID(guid, { includeDeleted: false });
      if (index == -1) {
        this.log.warn("attempting to remove non-existing entry", guid);
        return;
      }
      let existing = this._data[index];
      if (existing.deleted) {
        return; // already a tombstone - don't touch it.
      }
      let existingSync = this._getSyncMetaData(existing);
      if (existingSync) {
        // existing sync metadata means it has been synced. This means we must
        // leave a tombstone behind.
        this._data[index] = {
          guid,
          timeLastModified: Date.now(),
          deleted: true,
          _sync: existingSync,
        };
        existingSync.changeCounter++;
      } else {
        // If there's no sync meta-data, this record has never been synced, so
        // we can delete it.
        this._data.splice(index, 1);
      }
    }

    this.updateUseCountTelemetry();

    this._store.saveSoon();
    Services.obs.notifyObservers(
      {
        wrappedJSObject: {
          sourceSync,
          guid,
          collectionName: this._collectionName,
        },
      },
      "formautofill-storage-changed",
      "remove"
    );
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
    this.log.debug(`get: ${guid}`);

    let recordFound = this._findByGUID(guid);
    if (!recordFound) {
      return null;
    }

    // The record is cloned to avoid accidental modifications from outside.
    let clonedRecord = this._cloneAndCleanUp(recordFound);
    if (rawData) {
      await this._stripComputedFields(clonedRecord);
    } else {
      this._recordReadProcessor(clonedRecord);
    }
    return clonedRecord;
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
    this.log.debug(`getAll. includeDeleted = ${includeDeleted}`);

    let records = this._data.filter(r => !r.deleted || includeDeleted);
    // Records are cloned to avoid accidental modifications from outside.
    let clonedRecords = records.map(r => this._cloneAndCleanUp(r));
    await Promise.all(
      clonedRecords.map(async record => {
        if (rawData) {
          await this._stripComputedFields(record);
        } else {
          this._recordReadProcessor(record);
        }
      })
    );
    return clonedRecords;
  }

  /**
   * Return all saved field names in the collection.
   *
   * @returns {Promise<Set>} Set containing saved field names.
   */
  async getSavedFieldNames() {
    this.log.debug("getSavedFieldNames");

    let records = this._data.filter(r => !r.deleted);
    records
      .map(record => this._cloneAndCleanUp(record))
      .forEach(record => this._recordReadProcessor(record));

    let fieldNames = new Set();
    for (let record of records) {
      for (let fieldName of Object.keys(record)) {
        if (INTERNAL_FIELDS.includes(fieldName) || !record[fieldName]) {
          continue;
        }
        fieldNames.add(fieldName);
      }
    }

    return fieldNames;
  }

  /**
   * Functions intended to be used in the support of Sync.
   */

  /**
   * Stores a hash of the last synced value for a field in a locally updated
   * record. We use this value to rebuild the shared parent, or base, when
   * reconciling incoming records that may have changed on another device.
   *
   * Storing the hash of the values that we last wrote to the Sync server lets
   * us determine if a remote change conflicts with a local change. If the
   * hashes for the base, current local value, and remote value all differ, we
   * have a conflict.
   *
   * These fields are not themselves synced, and will be removed locally as
   * soon as we have successfully written the record to the Sync server - so
   * it is expected they will not remain for long, as changes which cause a
   * last synced field to be written will itself cause a sync.
   *
   * We also skip this for updates made by Sync, for internal fields, for
   * records that haven't been uploaded yet, and for fields which have already
   * been changed since the last sync.
   *
   * @param   {Object} record
   *          The updated local record.
   * @param   {string} field
   *          The field name.
   * @param   {string} lastSyncedValue
   *          The last synced field value.
   */
  _maybeStoreLastSyncedField(record, field, lastSyncedValue) {
    let sync = this._getSyncMetaData(record);
    if (!sync) {
      // The record hasn't been uploaded yet, so we can't end up with merge
      // conflicts.
      return;
    }
    let alreadyChanged = field in sync.lastSyncedFields;
    if (alreadyChanged) {
      // This field was already changed multiple times since the last sync.
      return;
    }
    let newValue = record[field];
    if (lastSyncedValue != newValue) {
      sync.lastSyncedFields[field] = sha512(lastSyncedValue);
    }
  }

  /**
   * Attempts a three-way merge between a changed local record, an incoming
   * remote record, and the shared parent that we synthesize from the last
   * synced fields - see _maybeStoreLastSyncedField.
   *
   * @param   {Object} strippedLocalRecord
   *          The changed local record, currently in storage. Computed fields
   *          are stripped.
   * @param   {Object} remoteRecord
   *          The remote record.
   * @returns {Object|null}
   *          The merged record, or `null` if there are conflicts and the
   *          records can't be merged.
   */
  _mergeSyncedRecords(strippedLocalRecord, remoteRecord) {
    let sync = this._getSyncMetaData(strippedLocalRecord, true);

    // Copy all internal fields from the remote record. We'll update their
    // values in `_replaceRecordAt`.
    let mergedRecord = {};
    for (let field of INTERNAL_FIELDS) {
      if (remoteRecord[field] != null) {
        mergedRecord[field] = remoteRecord[field];
      }
    }

    for (let field of this.VALID_FIELDS) {
      let isLocalSame = false;
      let isRemoteSame = false;
      if (field in sync.lastSyncedFields) {
        // If the field has changed since the last sync, compare hashes to
        // determine if the local and remote values are different. Hashing is
        // expensive, but we don't expect this to happen frequently.
        let lastSyncedValue = sync.lastSyncedFields[field];
        isLocalSame = lastSyncedValue == sha512(strippedLocalRecord[field]);
        isRemoteSame = lastSyncedValue == sha512(remoteRecord[field]);
      } else {
        // Otherwise, if the field hasn't changed since the last sync, we know
        // it's the same locally.
        isLocalSame = true;
        isRemoteSame = strippedLocalRecord[field] == remoteRecord[field];
      }

      let value;
      if (isLocalSame && isRemoteSame) {
        // Local and remote are the same; doesn't matter which one we pick.
        value = strippedLocalRecord[field];
      } else if (isLocalSame && !isRemoteSame) {
        value = remoteRecord[field];
      } else if (!isLocalSame && isRemoteSame) {
        // We don't need to bump the change counter when taking the local
        // change, because the counter must already be > 0 if we're attempting
        // a three-way merge.
        value = strippedLocalRecord[field];
      } else if (strippedLocalRecord[field] == remoteRecord[field]) {
        // Shared parent doesn't match either local or remote, but the values
        // are identical, so there's no conflict.
        value = strippedLocalRecord[field];
      } else {
        // Both local and remote changed to different values. We'll need to fork
        // the local record to resolve the conflict.
        return null;
      }

      if (value != null) {
        mergedRecord[field] = value;
      }
    }

    return mergedRecord;
  }

  /**
   * Replaces a local record with a remote or merged record, copying internal
   * fields and Sync metadata.
   *
   * @param   {number} index
   * @param   {Object} remoteRecord
   * @param   {Promise<boolean>} [options.keepSyncMetadata = false]
   *          Should we copy Sync metadata? This is true if `remoteRecord` is a
   *          merged record with local changes that we need to upload. Passing
   *          `keepSyncMetadata` retains the record's change counter and
   *          last synced fields, so that we don't clobber the local change if
   *          the sync is interrupted after the record is merged, but before
   *          it's uploaded.
   */
  async _replaceRecordAt(
    index,
    remoteRecord,
    { keepSyncMetadata = false } = {}
  ) {
    let localRecord = this._data[index];
    let newRecord = this._clone(remoteRecord);

    await this._stripComputedFields(newRecord);

    this._data[index] = newRecord;

    if (keepSyncMetadata) {
      // It's safe to move the Sync metadata from the old record to the new
      // record, since we always clone records when we return them, and we
      // never hand out references to the metadata object via public methods.
      newRecord._sync = localRecord._sync;
    } else {
      // As a side effect, `_getSyncMetaData` marks the record as syncing if the
      // existing `localRecord` is a dupe of `remoteRecord`, and we're replacing
      // local with remote.
      let sync = this._getSyncMetaData(newRecord, true);
      sync.changeCounter = 0;
    }

    if (
      !newRecord.timeCreated ||
      localRecord.timeCreated < newRecord.timeCreated
    ) {
      newRecord.timeCreated = localRecord.timeCreated;
    }

    if (
      !newRecord.timeLastModified ||
      localRecord.timeLastModified > newRecord.timeLastModified
    ) {
      newRecord.timeLastModified = localRecord.timeLastModified;
    }

    // Copy local-only fields from the existing local record.
    for (let field of ["timeLastUsed", "timesUsed"]) {
      if (localRecord[field] != null) {
        newRecord[field] = localRecord[field];
      }
    }

    await this.computeFields(newRecord);
  }

  /**
   * Clones a local record, giving the clone a new GUID and Sync metadata. The
   * original record remains unchanged in storage.
   *
   * @param   {Object} strippedLocalRecord
   *          The local record. Computed fields are stripped.
   * @returns {string}
   *          A clone of the local record with a new GUID.
   */
  async _forkLocalRecord(strippedLocalRecord) {
    let forkedLocalRecord = this._cloneAndCleanUp(strippedLocalRecord);
    forkedLocalRecord.guid = this._generateGUID();

    // Give the record fresh Sync metadata and bump its change counter as a
    // side effect. This also excludes the forked record from de-duping on the
    // next sync, if the current sync is interrupted before the record can be
    // uploaded.
    this._getSyncMetaData(forkedLocalRecord, true);

    await this.computeFields(forkedLocalRecord);
    this._data.push(forkedLocalRecord);

    return forkedLocalRecord;
  }

  /**
   * Reconciles an incoming remote record into the matching local record. This
   * method is only used by Sync; other callers should use `merge`.
   *
   * @param   {Object} remoteRecord
   *          The incoming record. `remoteRecord` must not be a tombstone, and
   *          must have a matching local record with the same GUID. Use
   *          `add` to insert remote records that don't exist locally, and
   *          `remove` to apply remote tombstones.
   * @returns {Promise<Object>}
   *          A `{forkedGUID}` tuple. `forkedGUID` is `null` if the merge
   *          succeeded without conflicts, or a new GUID referencing the
   *          existing locally modified record if the conflicts could not be
   *          resolved.
   */
  async reconcile(remoteRecord) {
    this._ensureMatchingVersion(remoteRecord);
    if (remoteRecord.deleted) {
      throw new Error(`Can't reconcile tombstone ${remoteRecord.guid}`);
    }

    let localIndex = this._findIndexByGUID(remoteRecord.guid);
    if (localIndex < 0) {
      throw new Error(`Record ${remoteRecord.guid} not found`);
    }

    let localRecord = this._data[localIndex];
    let sync = this._getSyncMetaData(localRecord, true);

    let forkedGUID = null;

    if (sync.changeCounter === 0) {
      // Local not modified. Replace local with remote.
      await this._replaceRecordAt(localIndex, remoteRecord, {
        keepSyncMetadata: false,
      });
    } else {
      let strippedLocalRecord = this._clone(localRecord);
      await this._stripComputedFields(strippedLocalRecord);

      let mergedRecord = this._mergeSyncedRecords(
        strippedLocalRecord,
        remoteRecord
      );
      if (mergedRecord) {
        // Local and remote modified, but we were able to merge. Replace the
        // local record with the merged record.
        await this._replaceRecordAt(localIndex, mergedRecord, {
          keepSyncMetadata: true,
        });
      } else {
        // Merge conflict. Fork the local record, then replace the original
        // with the merged record.
        let forkedLocalRecord = await this._forkLocalRecord(
          strippedLocalRecord
        );
        forkedGUID = forkedLocalRecord.guid;
        await this._replaceRecordAt(localIndex, remoteRecord, {
          keepSyncMetadata: false,
        });
      }
    }

    this._store.saveSoon();
    Services.obs.notifyObservers(
      {
        wrappedJSObject: {
          sourceSync: true,
          guid: remoteRecord.guid,
          forkedGUID,
          collectionName: this._collectionName,
        },
      },
      "formautofill-storage-changed",
      "reconcile"
    );

    return { forkedGUID };
  }

  _removeSyncedRecord(guid) {
    let index = this._findIndexByGUID(guid, { includeDeleted: true });
    if (index == -1) {
      // Removing a record we don't know about. It may have been synced and
      // removed by another device before we saw it. Store the tombstone in
      // case the server is later wiped and we need to reupload everything.
      let tombstone = {
        guid,
        timeLastModified: Date.now(),
        deleted: true,
      };

      let sync = this._getSyncMetaData(tombstone, true);
      sync.changeCounter = 0;
      this._data.push(tombstone);
      return;
    }

    let existing = this._data[index];
    let sync = this._getSyncMetaData(existing, true);
    if (sync.changeCounter > 0) {
      // Deleting a record with unsynced local changes. To avoid potential
      // data loss, we ignore the deletion in favor of the changed record.
      this.log.info(
        "Ignoring deletion for record with local changes",
        existing
      );
      return;
    }

    if (existing.deleted) {
      this.log.info("Ignoring deletion for tombstone", existing);
      return;
    }

    // Removing a record that's not changed locally, and that's not already
    // deleted. Replace the record with a synced tombstone.
    this._data[index] = {
      guid,
      timeLastModified: Date.now(),
      deleted: true,
      _sync: sync,
    };
  }

  /**
   * Provide an object that describes the changes to sync.
   *
   * This is called at the start of the sync process to determine what needs
   * to be updated on the server. As the server is updated, sync will update
   * entries in the returned object, and when sync is complete it will pass
   * the object to pushSyncChanges, which will apply the changes to the store.
   *
   * @returns {object}
   *          An object describing the changes to sync.
   */
  pullSyncChanges() {
    let changes = {};

    let profiles = this._data;
    for (let profile of profiles) {
      let sync = this._getSyncMetaData(profile, true);
      if (sync.changeCounter < 1) {
        if (sync.changeCounter != 0) {
          this.log.error("negative change counter", profile);
        }
        continue;
      }
      changes[profile.guid] = {
        profile,
        counter: sync.changeCounter,
        modified: profile.timeLastModified,
        synced: false,
      };
    }
    this._store.saveSoon();

    return changes;
  }

  /**
   * Apply the metadata changes made by Sync.
   *
   * This is called with metadata about what was synced - see pullSyncChanges.
   *
   * @param {object} changes
   *        The possibly modified object obtained via pullSyncChanges.
   */
  pushSyncChanges(changes) {
    for (let [guid, { counter, synced }] of Object.entries(changes)) {
      if (!synced) {
        continue;
      }
      let recordFound = this._findByGUID(guid, { includeDeleted: true });
      if (!recordFound) {
        this.log.warn("No profile found to persist changes for guid " + guid);
        continue;
      }
      let sync = this._getSyncMetaData(recordFound, true);
      sync.changeCounter = Math.max(0, sync.changeCounter - counter);
      if (sync.changeCounter === 0) {
        // Clear the shared parent fields once we've uploaded all pending
        // changes, since the server now matches what we have locally.
        sync.lastSyncedFields = {};
      }
    }
    this._store.saveSoon();
  }

  /**
   * Reset all sync metadata for all items.
   *
   * This is called when Sync is disconnected from this device. All sync
   * metadata for all items is removed.
   */
  resetSync() {
    for (let record of this._data) {
      delete record._sync;
    }
    // XXX - we should probably also delete all tombstones?
    this.log.info("All sync metadata was reset");
  }

  /**
   * Changes the GUID of an item. This should be called only by Sync. There
   * must be an existing record with oldID and it must never have been synced
   * or an error will be thrown. There must be no existing record with newID.
   *
   * No tombstone will be created for the old GUID - we check it hasn't
   * been synced, so no tombstone is necessary.
   *
   * @param   {string} oldID
   *          GUID of the existing item to change the GUID of.
   * @param   {string} newID
   *          The new GUID for the item.
   */
  changeGUID(oldID, newID) {
    this.log.debug("changeGUID: ", oldID, newID);
    if (oldID == newID) {
      throw new Error("changeGUID: old and new IDs are the same");
    }
    if (this._findIndexByGUID(newID) >= 0) {
      throw new Error("changeGUID: record with destination id exists already");
    }

    let index = this._findIndexByGUID(oldID);
    let profile = this._data[index];
    if (!profile) {
      throw new Error("changeGUID: no source record");
    }
    if (this._getSyncMetaData(profile)) {
      throw new Error("changeGUID: existing record has already been synced");
    }

    profile.guid = newID;

    this._store.saveSoon();
  }

  // Used to get, and optionally create, sync metadata. Brand new records will
  // *not* have sync meta-data - it will be created when they are first
  // synced.
  _getSyncMetaData(record, forceCreate = false) {
    if (!record._sync && forceCreate) {
      // create default metadata and indicate we need to save.
      record._sync = {
        changeCounter: 1,
        lastSyncedFields: {},
      };
      this._store.saveSoon();
    }
    return record._sync;
  }

  /**
   * Finds a local record with matching common fields and a different GUID.
   * Sync uses this method to find and update unsynced local records with
   * fields that match incoming remote records. This avoids creating
   * duplicate profiles with the same information.
   *
   * @param   {Object} remoteRecord
   *          The remote record.
   * @returns {Promise<string|null>}
   *          The GUID of the matching local record, or `null` if no records
   *          match.
   */
  async findDuplicateGUID(remoteRecord) {
    if (!remoteRecord.guid) {
      throw new Error("Record missing GUID");
    }
    this._ensureMatchingVersion(remoteRecord);
    if (remoteRecord.deleted) {
      // Tombstones don't carry enough info to de-dupe, and we should have
      // handled them separately when applying the record.
      throw new Error("Tombstones can't have duplicates");
    }
    let localRecords = this._data;
    for (let localRecord of localRecords) {
      if (localRecord.deleted) {
        continue;
      }
      if (localRecord.guid == remoteRecord.guid) {
        throw new Error(`Record ${remoteRecord.guid} already exists`);
      }
      if (this._getSyncMetaData(localRecord)) {
        // This local record has already been uploaded, so it can't be a dupe of
        // another incoming item.
        continue;
      }

      // Ignore computed fields when matching records as they aren't synced at all.
      let strippedLocalRecord = this._clone(localRecord);
      await this._stripComputedFields(strippedLocalRecord);

      let keys = new Set(Object.keys(remoteRecord));
      for (let key of Object.keys(strippedLocalRecord)) {
        keys.add(key);
      }
      // Ignore internal fields when matching records. Internal fields are synced,
      // but almost certainly have different values than the local record, and
      // we'll update them in `reconcile`.
      for (let field of INTERNAL_FIELDS) {
        keys.delete(field);
      }
      if (!keys.size) {
        // This shouldn't ever happen; a valid record will always have fields
        // that aren't computed or internal. Sync can't do anything about that,
        // so we ignore the dubious local record instead of throwing.
        continue;
      }
      let same = true;
      for (let key of keys) {
        // For now, we ensure that both (or neither) records have the field
        // with matching values. This doesn't account for the version yet
        // (bug 1377204).
        same =
          key in strippedLocalRecord == key in remoteRecord &&
          strippedLocalRecord[key] == remoteRecord[key];
        if (!same) {
          break;
        }
      }
      if (same) {
        return strippedLocalRecord.guid;
      }
    }
    return null;
  }

  /**
   * Internal helper functions.
   */

  _clone(record) {
    return Object.assign({}, record);
  }

  _cloneAndCleanUp(record) {
    let result = {};
    for (let key in record) {
      // Do not expose hidden fields and fields with empty value (mainly used
      // as placeholders of the computed fields).
      if (!key.startsWith("_") && record[key] !== "") {
        result[key] = record[key];
      }
    }
    return result;
  }

  _findByGUID(guid, { includeDeleted = false } = {}) {
    let found = this._findIndexByGUID(guid, { includeDeleted });
    return found < 0 ? undefined : this._data[found];
  }

  _findIndexByGUID(guid, { includeDeleted = false } = {}) {
    return this._data.findIndex(record => {
      return record.guid == guid && (!record.deleted || includeDeleted);
    });
  }

  async _migrateRecord(record, index) {
    let hasChanges = false;

    if (record.deleted) {
      return hasChanges;
    }

    if (!record.version || isNaN(record.version) || record.version < 1) {
      this.log.warn("Invalid record version:", record.version);

      // Force to run the migration.
      record.version = 0;
    }

    if (record.version < this.version) {
      hasChanges = true;

      record = await this._computeMigratedRecord(record);

      if (record.deleted) {
        // record is deleted by _computeMigratedRecord(),
        // go ahead and put it in the store.
        this._data[index] = record;
        return hasChanges;
      }

      // Compute the computed fields before putting it to store.
      await this.computeFields(record);
      this._data[index] = record;

      return hasChanges;
    }

    hasChanges |= await this.computeFields(record);
    return hasChanges;
  }

  _normalizeRecord(record, preserveEmptyFields = false) {
    this._normalizeFields(record);

    for (let key in record) {
      if (!this.VALID_FIELDS.includes(key)) {
        throw new Error(`"${key}" is not a valid field.`);
      }
      if (typeof record[key] !== "string" && typeof record[key] !== "number") {
        throw new Error(
          `"${key}" contains invalid data type: ${typeof record[key]}`
        );
      }
      if (!preserveEmptyFields && record[key] === "") {
        delete record[key];
      }
    }

    if (!Object.keys(record).length) {
      throw new Error("Record contains no valid field.");
    }
  }

  /**
   * Merge the record if storage has multiple mergeable records.
   * @param {Object} targetRecord
   *        The record for merge.
   * @param {boolean} [strict = false]
   *        In strict merge mode, we'll treat the subset record with empty field
   *        as unable to be merged, but mergeable if in non-strict mode.
   * @returns {Array.<string>}
   *          Return an array of the merged GUID string.
   */
  async mergeToStorage(targetRecord, strict = false) {
    let mergedGUIDs = [];
    for (let record of this._data) {
      if (
        !record.deleted &&
        (await this.mergeIfPossible(record.guid, targetRecord, strict))
      ) {
        mergedGUIDs.push(record.guid);
      }
    }
    this.log.debug(
      "Existing records matching and merging count is",
      mergedGUIDs.length
    );
    return mergedGUIDs;
  }

  /**
   * Unconditionally remove all data and tombstones for this collection.
   */
  removeAll({ sourceSync = false } = {}) {
    this._store.data[this._collectionName] = [];
    this._store.saveSoon();
    Services.obs.notifyObservers(
      {
        wrappedJSObject: {
          sourceSync,
          collectionName: this._collectionName,
        },
      },
      "formautofill-storage-changed",
      "removeAll"
    );
  }

  /**
   * Strip the computed fields based on the record version.
   * @param   {Object} record      The record to migrate
   * @returns {Object}             Migrated record.
   *                               Record is always cloned, with version updated,
   *                               with computed fields stripped.
   *                               Could be a tombstone record, if the record
   *                               should be discorded.
   */
  async _computeMigratedRecord(record) {
    if (!record.deleted) {
      record = this._clone(record);
      await this._stripComputedFields(record);
      record.version = this.version;
    }
    return record;
  }

  async _stripComputedFields(record) {
    this.VALID_COMPUTED_FIELDS.forEach(field => delete record[field]);
  }

  // An interface to be inherited.
  _recordReadProcessor(record) {}

  // An interface to be inherited.
  async computeFields(record) {}

  /**
   * An interface to be inherited to mutate the argument to normalize it.
   *
   * @param {object} partialRecord containing the record passed by the consumer of
   *                               storage and in the case of `update` with
   *                               `preserveOldProperties` will only include the
   *                               properties that the user is changing so the
   *                               lack of a field doesn't mean that the record
   *                               won't have that field.
   */
  _normalizeFields(partialRecord) {}

  /**
   * An interface to be inherited to validate that the complete record is
   * consistent and isn't missing required fields. Overrides should throw for
   * invalid records.
   *
   * @param {object} record containing the complete record that would be stored
   *                        if this doesn't throw due to an error.
   * @throws
   */
  _validateFields(record) {}

  // An interface to be inherited.
  async mergeIfPossible(guid, record, strict) {}
}

class AddressesBase extends AutofillRecords {
  constructor(store) {
    super(
      store,
      "addresses",
      VALID_ADDRESS_FIELDS,
      VALID_ADDRESS_COMPUTED_FIELDS,
      ADDRESS_SCHEMA_VERSION
    );
  }

  _recordReadProcessor(address) {
    if (address.country && !FormAutofill.countries.has(address.country)) {
      delete address.country;
      delete address["country-name"];
    }
  }

  async computeFields(address) {
    // NOTE: Remember to bump the schema version number if any of the existing
    //       computing algorithm changes. (No need to bump when just adding new
    //       computed fields.)

    // NOTE: Computed fields should be always present in the storage no matter
    //       it's empty or not.

    let hasNewComputedFields = false;

    if (address.deleted) {
      return hasNewComputedFields;
    }

    // Compute name
    if (!("name" in address)) {
      let name = lazy.FormAutofillNameUtils.joinNameParts({
        given: address["given-name"],
        middle: address["additional-name"],
        family: address["family-name"],
      });
      address.name = name;
      hasNewComputedFields = true;
    }

    // Compute address lines
    if (!("address-line1" in address)) {
      let streetAddress = [];
      if (address["street-address"]) {
        streetAddress = address["street-address"]
          .split("\n")
          .map(s => s.trim());
      }
      for (let i = 0; i < 3; i++) {
        address["address-line" + (i + 1)] = streetAddress[i] || "";
      }
      if (streetAddress.length > 3) {
        address["address-line3"] = lazy.FormAutofillUtils.toOneLineAddress(
          streetAddress.splice(2)
        );
      }
      hasNewComputedFields = true;
    }

    // Compute country name
    if (!("country-name" in address)) {
      if (address.country) {
        try {
          address[
            "country-name"
          ] = Services.intl.getRegionDisplayNames(undefined, [address.country]);
        } catch (e) {
          address["country-name"] = "";
        }
      } else {
        address["country-name"] = "";
      }
      hasNewComputedFields = true;
    }

    // Compute tel
    if (!("tel-national" in address)) {
      if (address.tel) {
        let tel = lazy.PhoneNumber.Parse(
          address.tel,
          address.country || FormAutofill.DEFAULT_REGION
        );
        if (tel) {
          if (tel.countryCode) {
            address["tel-country-code"] = tel.countryCode;
          }
          if (tel.nationalNumber) {
            address["tel-national"] = tel.nationalNumber;
          }

          // PhoneNumberUtils doesn't support parsing the components of a telephone
          // number so we hard coded the parser for US numbers only. We will need
          // to figure out how to parse numbers from other regions when we support
          // new countries in the future.
          if (tel.nationalNumber && tel.countryCode == "+1") {
            let telComponents = tel.nationalNumber.match(
              /(\d{3})((\d{3})(\d{4}))$/
            );
            if (telComponents) {
              address["tel-area-code"] = telComponents[1];
              address["tel-local"] = telComponents[2];
              address["tel-local-prefix"] = telComponents[3];
              address["tel-local-suffix"] = telComponents[4];
            }
          }
        } else {
          // Treat "tel" as "tel-national" directly if it can't be parsed.
          address["tel-national"] = address.tel;
        }
      }

      TEL_COMPONENTS.forEach(c => {
        address[c] = address[c] || "";
      });
    }

    return hasNewComputedFields;
  }

  _normalizeFields(address) {
    this._normalizeName(address);
    this._normalizeAddress(address);
    this._normalizeCountry(address);
    this._normalizeTel(address);
  }

  _normalizeName(address) {
    if (address.name) {
      let nameParts = lazy.FormAutofillNameUtils.splitName(address.name);
      if (!address["given-name"] && nameParts.given) {
        address["given-name"] = nameParts.given;
      }
      if (!address["additional-name"] && nameParts.middle) {
        address["additional-name"] = nameParts.middle;
      }
      if (!address["family-name"] && nameParts.family) {
        address["family-name"] = nameParts.family;
      }
    }
    delete address.name;
  }

  _normalizeAddress(address) {
    if (STREET_ADDRESS_COMPONENTS.some(c => !!address[c])) {
      // Treat "street-address" as "address-line1" if it contains only one line
      // and "address-line1" is omitted.
      if (
        !address["address-line1"] &&
        address["street-address"] &&
        !address["street-address"].includes("\n")
      ) {
        address["address-line1"] = address["street-address"];
        delete address["street-address"];
      }

      // Concatenate "address-line*" if "street-address" is omitted.
      if (!address["street-address"]) {
        address["street-address"] = STREET_ADDRESS_COMPONENTS.map(
          c => address[c]
        )
          .join("\n")
          .replace(/\n+$/, "");
      }
    }
    STREET_ADDRESS_COMPONENTS.forEach(c => delete address[c]);
  }

  _normalizeCountry(address) {
    let country;

    if (address.country) {
      country = address.country.toUpperCase();
    } else if (address["country-name"]) {
      country = lazy.FormAutofillUtils.identifyCountryCode(
        address["country-name"]
      );
    }

    // Only values included in the region list will be saved.
    let hasLocalizedName = false;
    try {
      if (country) {
        let localizedName = Services.intl.getRegionDisplayNames(undefined, [
          country,
        ]);
        hasLocalizedName = localizedName != country;
      }
    } catch (e) {}

    if (country && hasLocalizedName) {
      address.country = country;
    } else {
      delete address.country;
    }

    delete address["country-name"];
  }

  _normalizeTel(address) {
    if (address.tel || TEL_COMPONENTS.some(c => !!address[c])) {
      lazy.FormAutofillUtils.compressTel(address);

      let possibleRegion = address.country || FormAutofill.DEFAULT_REGION;
      let tel = lazy.PhoneNumber.Parse(address.tel, possibleRegion);

      if (tel && tel.internationalNumber) {
        // Force to save numbers in E.164 format if parse success.
        address.tel = tel.internationalNumber;
      }
    }
    TEL_COMPONENTS.forEach(c => delete address[c]);
  }

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
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }
}

class CreditCardsBase extends AutofillRecords {
  constructor(store) {
    super(
      store,
      "creditCards",
      VALID_CREDIT_CARD_FIELDS,
      VALID_CREDIT_CARD_COMPUTED_FIELDS,
      CREDIT_CARD_SCHEMA_VERSION
    );
    Services.obs.addObserver(this, "formautofill-storage-changed");
  }

  observe(subject, topic, data) {
    switch (topic) {
      case "formautofill-storage-changed":
        let count = this._data.filter(entry => !entry.deleted).length;
        Services.telemetry.scalarSet(
          "formautofill.creditCards.autofill_profiles_count",
          count
        );
        break;
    }
  }

  async computeFields(creditCard) {
    // NOTE: Remember to bump the schema version number if any of the existing
    //       computing algorithm changes. (No need to bump when just adding new
    //       computed fields.)

    // NOTE: Computed fields should be always present in the storage no matter
    //       it's empty or not.

    let hasNewComputedFields = false;

    if (creditCard.deleted) {
      return hasNewComputedFields;
    }

    if ("cc-number" in creditCard && !("cc-type" in creditCard)) {
      let type = lazy.CreditCard.getType(creditCard["cc-number"]);
      if (type) {
        creditCard["cc-type"] = type;
      }
    }

    // Compute split names
    if (!("cc-given-name" in creditCard)) {
      let nameParts = lazy.FormAutofillNameUtils.splitName(
        creditCard["cc-name"]
      );
      creditCard["cc-given-name"] = nameParts.given;
      creditCard["cc-additional-name"] = nameParts.middle;
      creditCard["cc-family-name"] = nameParts.family;
      hasNewComputedFields = true;
    }

    // Compute credit card expiration date
    if (!("cc-exp" in creditCard)) {
      if (creditCard["cc-exp-month"] && creditCard["cc-exp-year"]) {
        creditCard["cc-exp"] =
          String(creditCard["cc-exp-year"]) +
          "-" +
          String(creditCard["cc-exp-month"]).padStart(2, "0");
      } else {
        creditCard["cc-exp"] = "";
      }
      hasNewComputedFields = true;
    }

    // Encrypt credit card number
    await this._encryptNumber(creditCard);

    return hasNewComputedFields;
  }

  async _encryptNumber(creditCard) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  async _computeMigratedRecord(creditCard) {
    if (creditCard["cc-number-encrypted"]) {
      switch (creditCard.version) {
        case 1:
        case 2: {
          // We cannot decrypt the data, so silently remove the record for
          // the user.
          if (creditCard.deleted) {
            break;
          }

          this.log.warn(
            "Removing version",
            creditCard.version,
            "credit card record to migrate to new encryption:",
            creditCard.guid
          );

          // Replace the record with a tombstone record here,
          // regardless of existence of sync metadata.
          let existingSync = this._getSyncMetaData(creditCard);
          creditCard = {
            guid: creditCard.guid,
            timeLastModified: Date.now(),
            deleted: true,
          };

          if (existingSync) {
            creditCard._sync = existingSync;
            existingSync.changeCounter++;
          }
          break;
        }

        default:
          throw new Error(
            "Unknown credit card version to migrate: " + creditCard.version
          );
      }
    }
    return super._computeMigratedRecord(creditCard);
  }

  async _stripComputedFields(creditCard) {
    if (creditCard["cc-number-encrypted"]) {
      try {
        creditCard["cc-number"] = await lazy.OSKeyStore.decrypt(
          creditCard["cc-number-encrypted"]
        );
      } catch (ex) {
        if (ex.result == Cr.NS_ERROR_ABORT) {
          throw ex;
        }
        // Quietly recover from encryption error,
        // so existing credit card entry with undecryptable number
        // can be updated.
      }
    }
    await super._stripComputedFields(creditCard);
  }

  _normalizeFields(creditCard) {
    this._normalizeCCName(creditCard);
    this._normalizeCCNumber(creditCard);
    this._normalizeCCExpirationDate(creditCard);
  }

  _normalizeCCName(creditCard) {
    if (
      creditCard["cc-given-name"] ||
      creditCard["cc-additional-name"] ||
      creditCard["cc-family-name"]
    ) {
      if (!creditCard["cc-name"]) {
        creditCard["cc-name"] = lazy.FormAutofillNameUtils.joinNameParts({
          given: creditCard["cc-given-name"],
          middle: creditCard["cc-additional-name"],
          family: creditCard["cc-family-name"],
        });
      }
    }
    delete creditCard["cc-given-name"];
    delete creditCard["cc-additional-name"];
    delete creditCard["cc-family-name"];
  }

  _normalizeCCNumber(creditCard) {
    if (!("cc-number" in creditCard)) {
      return;
    }
    if (!lazy.CreditCard.isValidNumber(creditCard["cc-number"])) {
      delete creditCard["cc-number"];
      return;
    }
    let card = new lazy.CreditCard({ number: creditCard["cc-number"] });
    creditCard["cc-number"] = card.number;
  }

  _normalizeCCExpirationDate(creditCard) {
    let normalizedExpiration = lazy.CreditCard.normalizeExpiration({
      expirationMonth: creditCard["cc-exp-month"],
      expirationYear: creditCard["cc-exp-year"],
      expirationString: creditCard["cc-exp"],
    });
    if (normalizedExpiration.month) {
      creditCard["cc-exp-month"] = normalizedExpiration.month;
    } else {
      delete creditCard["cc-exp-month"];
    }
    if (normalizedExpiration.year) {
      creditCard["cc-exp-year"] = normalizedExpiration.year;
    } else {
      delete creditCard["cc-exp-year"];
    }
    delete creditCard["cc-exp"];
  }

  _validateFields(creditCard) {
    if (!creditCard["cc-number"]) {
      throw new Error("Missing/invalid cc-number");
    }
  }

  _ensureMatchingVersion(record) {
    if (!record.version || isNaN(record.version) || record.version < 1) {
      throw new Error(
        `Got invalid record version ${record.version}; want ${this.version}`
      );
    }

    if (record.version < this.version) {
      switch (record.version) {
        case 1:
        case 2:
          // The difference between version 1 and 2 is only about the encryption
          // method used for the cc-number-encrypted field.
          // The difference between version 2 and 3 is the name of the OS
          // key encryption record.
          // As long as the record is already decrypted, it is safe to bump the
          // version directly.
          if (!record["cc-number-encrypted"]) {
            record.version = this.version;
          } else {
            throw new Error(
              "Could not migrate record version:",
              record.version,
              "->",
              this.version
            );
          }
          break;
        default:
          throw new Error(
            "Unknown credit card version to match: " + record.version
          );
      }
    }

    return super._ensureMatchingVersion(record);
  }

  /**
   * Normalize the given record and return the first matched guid if storage has the same record.
   * @param {Object} targetCreditCard
   *        The credit card for duplication checking.
   * @returns {Promise<string|null>}
   *          Return the first guid if storage has the same credit card and null otherwise.
   */
  async getDuplicateGuid(targetCreditCard) {
    let clonedTargetCreditCard = this._clone(targetCreditCard);
    this._normalizeRecord(clonedTargetCreditCard);
    if (!clonedTargetCreditCard["cc-number"]) {
      return null;
    }

    for (let creditCard of this._data) {
      if (creditCard.deleted) {
        continue;
      }

      let decrypted = await lazy.OSKeyStore.decrypt(
        creditCard["cc-number-encrypted"],
        false
      );

      if (decrypted == clonedTargetCreditCard["cc-number"]) {
        return creditCard.guid;
      }
    }
    return null;
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
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  updateUseCountTelemetry() {
    let histogram = Services.telemetry.getHistogramById("CREDITCARD_NUM_USES");
    histogram.clear();

    let records = this._data.filter(r => !r.deleted);

    for (let record of records) {
      histogram.add(record.timesUsed);
    }
  }
}

class FormAutofillStorageBase {
  constructor(path) {
    this._path = path;
    this._initializePromise = null;
    this.INTERNAL_FIELDS = INTERNAL_FIELDS;
  }

  get version() {
    return STORAGE_SCHEMA_VERSION;
  }

  get addresses() {
    return this.getAddresses();
  }

  get creditCards() {
    return this.getCreditCards();
  }

  getAddresses() {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  getCreditCards() {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  /**
   * Initialize storage to memory.
   *
   * @returns {Promise}
   * @resolves When the operation finished successfully.
   * @rejects  JavaScript exception.
   */
  initialize() {
    if (!this._initializePromise) {
      this._store = this._initializeStore();
      this._initializePromise = this._store.load().then(() => {
        let initializeAutofillRecords = [
          this.addresses.initialize(),
          this.creditCards.initialize(),
        ];
        return Promise.all(initializeAutofillRecords);
      });
    }
    return this._initializePromise;
  }

  _initializeStore() {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  // For test only.
  _saveImmediately() {
    return this._store._save();
  }

  _finalize() {
    return this._store.finalize();
  }
}
