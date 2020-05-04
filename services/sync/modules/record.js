/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = [
  "WBORecord",
  "RecordManager",
  "RawCryptoWrapper",
  "CryptoWrapper",
  "CollectionKeyManager",
  "Collection",
];

const CRYPTO_COLLECTION = "crypto";
const KEYS_WBO = "keys";

const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
const {
  DEFAULT_DOWNLOAD_BATCH_SIZE,
  DEFAULT_KEYBUNDLE_NAME,
} = ChromeUtils.import("resource://services-sync/constants.js");
const { BulkKeyBundle } = ChromeUtils.import(
  "resource://services-sync/keys.js"
);
const { Weave } = ChromeUtils.import("resource://services-sync/main.js");
const { Resource } = ChromeUtils.import("resource://services-sync/resource.js");
const { Utils } = ChromeUtils.import("resource://services-sync/util.js");
const { Async } = ChromeUtils.import("resource://services-common/async.js");
const { CommonUtils } = ChromeUtils.import(
  "resource://services-common/utils.js"
);

/**
 * The base class for all Sync basic storage objects (BSOs). This is the format
 * used to store all records on the Sync server. In an earlier version of the
 * Sync protocol, BSOs used to be called WBOs, or Weave Basic Objects. This
 * class retains the old name.
 *
 * @class
 * @param {String} collection The collection name for this BSO.
 * @param {String} id         The ID of this BSO.
 */
function WBORecord(collection, id) {
  this.data = {};
  this.payload = {};
  this.collection = collection; // Optional.
  this.id = id; // Optional.
}
WBORecord.prototype = {
  _logName: "Sync.Record.WBO",

  get sortindex() {
    if (this.data.sortindex) {
      return this.data.sortindex;
    }
    return 0;
  },

  // Get thyself from your URI, then deserialize.
  // Set thine 'response' field.
  async fetch(resource) {
    if (!(resource instanceof Resource)) {
      throw new Error("First argument must be a Resource instance.");
    }

    let r = await resource.get();
    if (r.success) {
      this.deserialize(r.obj); // Warning! Muffles exceptions!
    }
    this.response = r;
    return this;
  },

  upload(resource) {
    if (!(resource instanceof Resource)) {
      throw new Error("First argument must be a Resource instance.");
    }

    return resource.put(this);
  },

  // Take a base URI string, with trailing slash, and return the URI of this
  // WBO based on collection and ID.
  uri(base) {
    if (this.collection && this.id) {
      let url = CommonUtils.makeURI(base + this.collection + "/" + this.id);
      url.QueryInterface(Ci.nsIURL);
      return url;
    }
    return null;
  },

  deserialize: function deserialize(json) {
    if (!json || typeof json !== "object") {
      throw new TypeError("Can't deserialize record from: " + json);
    }
    this.data = json;
    try {
      // The payload is likely to be JSON, but if not, keep it as a string
      this.payload = JSON.parse(this.payload);
    } catch (ex) {}
  },

  toJSON: function toJSON() {
    // Copy fields from data to be stringified, making sure payload is a string
    let obj = {};
    for (let [key, val] of Object.entries(this.data)) {
      obj[key] = key == "payload" ? JSON.stringify(val) : val;
    }
    if (this.ttl) {
      obj.ttl = this.ttl;
    }
    return obj;
  },

  toString: function toString() {
    return (
      "{ " +
      "id: " +
      this.id +
      "  " +
      "index: " +
      this.sortindex +
      "  " +
      "modified: " +
      this.modified +
      "  " +
      "ttl: " +
      this.ttl +
      "  " +
      "payload: " +
      JSON.stringify(this.payload) +
      " }"
    );
  },
};

Utils.deferGetSet(WBORecord, "data", [
  "id",
  "modified",
  "sortindex",
  "payload",
]);

/**
 * An encrypted BSO record. This subclass handles encrypting and decrypting the
 * BSO payload, but doesn't parse or interpret the cleartext string. Subclasses
 * must override `transformBeforeEncrypt` and `transformAfterDecrypt` to process
 * the cleartext.
 *
 * This class is only exposed for bridged engines, which handle serialization
 * and deserialization in Rust. Sync engines implemented in JS should subclass
 * `CryptoWrapper` instead, which takes care of transforming the cleartext into
 * an object, and ensuring its contents are valid.
 *
 * @class
 * @template Cleartext
 * @param    {String} collection The collection name for this BSO.
 * @param    {String} id         The ID of this BSO.
 */
function RawCryptoWrapper(collection, id) {
  // Setting properties before calling the superclass constructor isn't allowed
  // in new-style classes (`class MyRecord extends RawCryptoWrapper`), but
  // allowed with plain functions. This is also why `defaultCleartext` is a
  // method, and not simply set in the subclass constructor.
  this.cleartext = this.defaultCleartext();
  WBORecord.call(this, collection, id);
  this.ciphertext = null;
}
RawCryptoWrapper.prototype = {
  __proto__: WBORecord.prototype,
  _logName: "Sync.Record.RawCryptoWrapper",

  /**
   * Returns the default empty cleartext for this record type. This is exposed
   * as a method so that subclasses can override it, and access the default
   * cleartext in their constructors. `CryptoWrapper`, for example, overrides
   * this to return an empty object, so that initializing the `id` in its
   * constructor calls its overridden `id` setter.
   *
   * @returns {Cleartext} An empty cleartext.
   */
  defaultCleartext() {
    return null;
  },

  /**
   * Transforms the cleartext into a string that can be encrypted and wrapped
   * in a BSO payload. This is called before uploading the record to the server.
   *
   * @param   {Cleartext} outgoingCleartext The cleartext to upload.
   * @returns {String}                      The serialized cleartext.
   */
  transformBeforeEncrypt(outgoingCleartext) {
    throw new TypeError("Override to stringify outgoing records");
  },

  /**
   * Transforms an incoming cleartext string into an instance of the
   * `Cleartext` type. This is called when fetching the record from the
   * server.
   *
   * @param   {String} incomingCleartext The decrypted cleartext string.
   * @returns {Cleartext}                The parsed cleartext.
   */
  transformAfterDecrypt(incomingCleartext) {
    throw new TypeError("Override to parse incoming records");
  },

  ciphertextHMAC: function ciphertextHMAC(keyBundle) {
    let hasher = keyBundle.sha256HMACHasher;
    if (!hasher) {
      throw new Error("Cannot compute HMAC without an HMAC key.");
    }

    return CommonUtils.bytesAsHex(Utils.digestBytes(this.ciphertext, hasher));
  },

  /*
   * Don't directly use the sync key. Instead, grab a key for this
   * collection, which is decrypted with the sync key.
   *
   * Cache those keys; invalidate the cache if the time on the keys collection
   * changes, or other auth events occur.
   *
   * Optional key bundle overrides the collection key lookup.
   */
  async encrypt(keyBundle) {
    if (!keyBundle) {
      throw new Error("A key bundle must be supplied to encrypt.");
    }

    this.IV = Weave.Crypto.generateRandomIV();
    this.ciphertext = await Weave.Crypto.encrypt(
      this.transformBeforeEncrypt(this.cleartext),
      keyBundle.encryptionKeyB64,
      this.IV
    );
    this.hmac = this.ciphertextHMAC(keyBundle);
    this.cleartext = null;
  },

  // Optional key bundle.
  async decrypt(keyBundle) {
    if (!this.ciphertext) {
      throw new Error("No ciphertext: nothing to decrypt?");
    }

    if (!keyBundle) {
      throw new Error("A key bundle must be supplied to decrypt.");
    }

    // Authenticate the encrypted blob with the expected HMAC
    let computedHMAC = this.ciphertextHMAC(keyBundle);

    if (computedHMAC != this.hmac) {
      Utils.throwHMACMismatch(this.hmac, computedHMAC);
    }

    let cleartext = await Weave.Crypto.decrypt(
      this.ciphertext,
      keyBundle.encryptionKeyB64,
      this.IV
    );
    this.cleartext = this.transformAfterDecrypt(cleartext);
    this.ciphertext = null;

    return this.cleartext;
  },
};

Utils.deferGetSet(RawCryptoWrapper, "payload", ["ciphertext", "IV", "hmac"]);

/**
 * An encrypted BSO record with a JSON payload. All engines implemented in JS
 * should subclass this class to describe their own record types.
 *
 * @class
 * @param {String} collection The collection name for this BSO.
 * @param {String} id         The ID of this BSO.
 */
function CryptoWrapper(collection, id) {
  RawCryptoWrapper.call(this, collection, id);
}
CryptoWrapper.prototype = {
  __proto__: RawCryptoWrapper.prototype,
  _logName: "Sync.Record.CryptoWrapper",

  defaultCleartext() {
    return {};
  },

  transformBeforeEncrypt(cleartext) {
    return JSON.stringify(cleartext);
  },

  transformAfterDecrypt(cleartext) {
    // Handle invalid data here. Elsewhere we assume that cleartext is an object.
    let json_result = JSON.parse(cleartext);

    if (!(json_result && json_result instanceof Object)) {
      throw new Error(
        `Decryption failed: result is <${json_result}>, not an object.`
      );
    }

    // Verify that the encrypted id matches the requested record's id.
    if (json_result.id != this.id) {
      throw new Error(`Record id mismatch: ${json_result.id} != ${this.id}`);
    }

    return json_result;
  },

  cleartextToString() {
    return JSON.stringify(this.cleartext);
  },

  toString: function toString() {
    let payload = this.deleted ? "DELETED" : this.cleartextToString();

    return (
      "{ " +
      "id: " +
      this.id +
      "  " +
      "index: " +
      this.sortindex +
      "  " +
      "modified: " +
      this.modified +
      "  " +
      "ttl: " +
      this.ttl +
      "  " +
      "payload: " +
      payload +
      "  " +
      "collection: " +
      (this.collection || "undefined") +
      " }"
    );
  },

  // The custom setter below masks the parent's getter, so explicitly call it :(
  get id() {
    return super.id;
  },

  // Keep both plaintext and encrypted versions of the id to verify integrity
  set id(val) {
    super.id = val;
    return (this.cleartext.id = val);
  },
};

Utils.deferGetSet(CryptoWrapper, "cleartext", "deleted");

/**
 * An interface and caching layer for records.
 */
function RecordManager(service) {
  this.service = service;

  this._log = Log.repository.getLogger(this._logName);
  this._records = {};
}
RecordManager.prototype = {
  _recordType: CryptoWrapper,
  _logName: "Sync.RecordManager",

  async import(url) {
    this._log.trace("Importing record: " + (url.spec ? url.spec : url));
    try {
      // Clear out the last response with empty object if GET fails
      this.response = {};
      this.response = await this.service.resource(url).get();

      // Don't parse and save the record on failure
      if (!this.response.success) {
        return null;
      }

      let record = new this._recordType(url);
      record.deserialize(this.response.obj);

      return this.set(url, record);
    } catch (ex) {
      if (Async.isShutdownException(ex)) {
        throw ex;
      }
      this._log.debug("Failed to import record", ex);
      return null;
    }
  },

  get(url) {
    // Use a url string as the key to the hash
    let spec = url.spec ? url.spec : url;
    if (spec in this._records) {
      return Promise.resolve(this._records[spec]);
    }
    return this.import(url);
  },

  set: function RecordMgr_set(url, record) {
    let spec = url.spec ? url.spec : url;
    return (this._records[spec] = record);
  },

  contains: function RecordMgr_contains(url) {
    if ((url.spec || url) in this._records) {
      return true;
    }
    return false;
  },

  clearCache: function recordMgr_clearCache() {
    this._records = {};
  },

  del: function RecordMgr_del(url) {
    delete this._records[url];
  },
};

/**
 * Keeps track of mappings between collection names ('tabs') and KeyBundles.
 *
 * You can update this thing simply by giving it /info/collections. It'll
 * use the last modified time to bring itself up to date.
 */
function CollectionKeyManager(lastModified, default_, collections) {
  this.lastModified = lastModified || 0;
  this._default = default_ || null;
  this._collections = collections || {};

  this._log = Log.repository.getLogger("Sync.CollectionKeyManager");
}

// TODO: persist this locally as an Identity. Bug 610913.
// Note that the last modified time needs to be preserved.
CollectionKeyManager.prototype = {
  /**
   * Generate a new CollectionKeyManager that has the same attributes
   * as this one.
   */
  clone() {
    const newCollections = {};
    for (let c in this._collections) {
      newCollections[c] = this._collections[c];
    }

    return new CollectionKeyManager(
      this.lastModified,
      this._default,
      newCollections
    );
  },

  // Return information about old vs new keys:
  // * same: true if two collections are equal
  // * changed: an array of collection names that changed.
  _compareKeyBundleCollections: function _compareKeyBundleCollections(m1, m2) {
    let changed = [];

    function process(m1, m2) {
      for (let k1 in m1) {
        let v1 = m1[k1];
        let v2 = m2[k1];
        if (!(v1 && v2 && v1.equals(v2))) {
          changed.push(k1);
        }
      }
    }

    // Diffs both ways.
    process(m1, m2);
    process(m2, m1);

    // Return a sorted, unique array.
    changed.sort();
    let last;
    changed = changed.filter(x => x != last && (last = x));
    return { same: changed.length == 0, changed };
  },

  get isClear() {
    return !this._default;
  },

  clear: function clear() {
    this._log.info("Clearing collection keys...");
    this.lastModified = 0;
    this._collections = {};
    this._default = null;
  },

  keyForCollection(collection) {
    if (collection && this._collections[collection]) {
      return this._collections[collection];
    }

    return this._default;
  },

  /**
   * If `collections` (an array of strings) is provided, iterate
   * over it and generate random keys for each collection.
   * Create a WBO for the given data.
   */
  _makeWBO(collections, defaultBundle) {
    let wbo = new CryptoWrapper(CRYPTO_COLLECTION, KEYS_WBO);
    let c = {};
    for (let k in collections) {
      c[k] = collections[k].keyPairB64;
    }
    wbo.cleartext = {
      default: defaultBundle ? defaultBundle.keyPairB64 : null,
      collections: c,
      collection: CRYPTO_COLLECTION,
      id: KEYS_WBO,
    };
    return wbo;
  },

  /**
   * Create a WBO for the current keys.
   */
  asWBO(collection, id) {
    return this._makeWBO(this._collections, this._default);
  },

  /**
   * Compute a new default key, and new keys for any specified collections.
   */
  async newKeys(collections) {
    let newDefaultKeyBundle = await this.newDefaultKeyBundle();

    let newColls = {};
    if (collections) {
      for (let c of collections) {
        let b = new BulkKeyBundle(c);
        await b.generateRandom();
        newColls[c] = b;
      }
    }
    return [newDefaultKeyBundle, newColls];
  },

  /**
   * Generates new keys, but does not replace our local copy. Use this to
   * verify an upload before storing.
   */
  async generateNewKeysWBO(collections) {
    let newDefaultKey, newColls;
    [newDefaultKey, newColls] = await this.newKeys(collections);

    return this._makeWBO(newColls, newDefaultKey);
  },

  /**
   * Create a new default key.
   *
   * @returns {BulkKeyBundle}
   */
  async newDefaultKeyBundle() {
    const key = new BulkKeyBundle(DEFAULT_KEYBUNDLE_NAME);
    await key.generateRandom();
    return key;
  },

  /**
   * Create a new default key and store it as this._default, since without one you cannot use setContents.
   */
  async generateDefaultKey() {
    this._default = await this.newDefaultKeyBundle();
  },

  /**
   * Return true if keys are already present for each of the given
   * collections.
   */
  hasKeysFor(collections) {
    // We can't use filter() here because sometimes collections is an iterator.
    for (let collection of collections) {
      if (!this._collections[collection]) {
        return false;
      }
    }
    return true;
  },

  /**
   * Return a new CollectionKeyManager that has keys for each of the
   * given collections (creating new ones for collections where we
   * don't already have keys).
   */
  async ensureKeysFor(collections) {
    const newKeys = Object.assign({}, this._collections);
    for (let c of collections) {
      if (newKeys[c]) {
        continue; // don't replace existing keys
      }

      const b = new BulkKeyBundle(c);
      await b.generateRandom();
      newKeys[c] = b;
    }
    return new CollectionKeyManager(this.lastModified, this._default, newKeys);
  },

  // Take the fetched info/collections WBO, checking the change
  // time of the crypto collection.
  updateNeeded(info_collections) {
    this._log.info(
      "Testing for updateNeeded. Last modified: " + this.lastModified
    );

    // No local record of modification time? Need an update.
    if (!this.lastModified) {
      return true;
    }

    // No keys on the server? We need an update, though our
    // update handling will be a little more drastic...
    if (!(CRYPTO_COLLECTION in info_collections)) {
      return true;
    }

    // Otherwise, we need an update if our modification time is stale.
    return info_collections[CRYPTO_COLLECTION] > this.lastModified;
  },

  //
  // Set our keys and modified time to the values fetched from the server.
  // Returns one of three values:
  //
  // * If the default key was modified, return true.
  // * If the default key was not modified, but per-collection keys were,
  //   return an array of such.
  // * Otherwise, return false -- we were up-to-date.
  //
  setContents: function setContents(payload, modified) {
    let self = this;

    this._log.info(
      "Setting collection keys contents. Our last modified: " +
        this.lastModified +
        ", input modified: " +
        modified +
        "."
    );

    if (!payload) {
      throw new Error("No payload in CollectionKeyManager.setContents().");
    }

    if (!payload.default) {
      this._log.warn("No downloaded default key: this should not occur.");
      this._log.warn("Not clearing local keys.");
      throw new Error(
        "No default key in CollectionKeyManager.setContents(). Cannot proceed."
      );
    }

    // Process the incoming default key.
    let b = new BulkKeyBundle(DEFAULT_KEYBUNDLE_NAME);
    b.keyPairB64 = payload.default;
    let newDefault = b;

    // Process the incoming collections.
    let newCollections = {};
    if ("collections" in payload) {
      this._log.info("Processing downloaded per-collection keys.");
      let colls = payload.collections;
      for (let k in colls) {
        let v = colls[k];
        if (v) {
          let keyObj = new BulkKeyBundle(k);
          keyObj.keyPairB64 = v;
          newCollections[k] = keyObj;
        }
      }
    }

    // Check to see if these are already our keys.
    let sameDefault = this._default && this._default.equals(newDefault);
    let collComparison = this._compareKeyBundleCollections(
      newCollections,
      this._collections
    );
    let sameColls = collComparison.same;

    if (sameDefault && sameColls) {
      self._log.info("New keys are the same as our old keys!");
      if (modified) {
        self._log.info("Bumped local modified time.");
        self.lastModified = modified;
      }
      return false;
    }

    // Make sure things are nice and tidy before we set.
    this.clear();

    this._log.info("Saving downloaded keys.");
    this._default = newDefault;
    this._collections = newCollections;

    // Always trust the server.
    if (modified) {
      self._log.info("Bumping last modified to " + modified);
      self.lastModified = modified;
    }

    return sameDefault ? collComparison.changed : true;
  },

  async updateContents(syncKeyBundle, storage_keys) {
    let log = this._log;
    log.info("Updating collection keys...");

    // storage_keys is a WBO, fetched from storage/crypto/keys.
    // Its payload is the default key, and a map of collections to keys.
    // We lazily compute the key objects from the strings we're given.

    let payload;
    try {
      payload = await storage_keys.decrypt(syncKeyBundle);
    } catch (ex) {
      log.warn("Got exception decrypting storage keys with sync key.", ex);
      log.info("Aborting updateContents. Rethrowing.");
      throw ex;
    }

    let r = this.setContents(payload, storage_keys.modified);
    log.info("Collection keys updated.");
    return r;
  },
};

function Collection(uri, recordObj, service) {
  if (!service) {
    throw new Error("Collection constructor requires a service.");
  }

  Resource.call(this, uri);

  // This is a bit hacky, but gets the job done.
  let res = service.resource(uri);
  this.authenticator = res.authenticator;

  this._recordObj = recordObj;
  this._service = service;

  this._full = false;
  this._ids = null;
  this._limit = 0;
  this._older = 0;
  this._newer = 0;
  this._data = [];
  // optional members used by batch upload operations.
  this._batch = null;
  this._commit = false;
  // Used for batch download operations -- note that this is explicitly an
  // opaque value and not (necessarily) a number.
  this._offset = null;
}
Collection.prototype = {
  __proto__: Resource.prototype,
  _logName: "Sync.Collection",

  _rebuildURL: function Coll__rebuildURL() {
    // XXX should consider what happens if it's not a URL...
    this.uri.QueryInterface(Ci.nsIURL);

    let args = [];
    if (this.older) {
      args.push("older=" + this.older);
    }
    if (this.newer) {
      args.push("newer=" + this.newer);
    }
    if (this.full) {
      args.push("full=1");
    }
    if (this.sort) {
      args.push("sort=" + this.sort);
    }
    if (this.ids != null) {
      args.push("ids=" + this.ids);
    }
    if (this.limit > 0 && this.limit != Infinity) {
      args.push("limit=" + this.limit);
    }
    if (this._batch) {
      args.push("batch=" + encodeURIComponent(this._batch));
    }
    if (this._commit) {
      args.push("commit=true");
    }
    if (this._offset) {
      args.push("offset=" + encodeURIComponent(this._offset));
    }

    this.uri = this.uri
      .mutate()
      .setQuery(args.length > 0 ? "?" + args.join("&") : "")
      .finalize();
  },

  // get full items
  get full() {
    return this._full;
  },
  set full(value) {
    this._full = value;
    this._rebuildURL();
  },

  // Apply the action to a certain set of ids
  get ids() {
    return this._ids;
  },
  set ids(value) {
    this._ids = value;
    this._rebuildURL();
  },

  // Limit how many records to get
  get limit() {
    return this._limit;
  },
  set limit(value) {
    this._limit = value;
    this._rebuildURL();
  },

  // get only items modified before some date
  get older() {
    return this._older;
  },
  set older(value) {
    this._older = value;
    this._rebuildURL();
  },

  // get only items modified since some date
  get newer() {
    return this._newer;
  },
  set newer(value) {
    this._newer = value;
    this._rebuildURL();
  },

  // get items sorted by some criteria. valid values:
  // oldest (oldest first)
  // newest (newest first)
  // index
  get sort() {
    return this._sort;
  },
  set sort(value) {
    if (value && value != "oldest" && value != "newest" && value != "index") {
      throw new TypeError(
        `Illegal value for sort: "${value}" (should be "oldest", "newest", or "index").`
      );
    }
    this._sort = value;
    this._rebuildURL();
  },

  get offset() {
    return this._offset;
  },
  set offset(value) {
    this._offset = value;
    this._rebuildURL();
  },

  // Set information about the batch for this request.
  get batch() {
    return this._batch;
  },
  set batch(value) {
    this._batch = value;
    this._rebuildURL();
  },

  get commit() {
    return this._commit;
  },
  set commit(value) {
    this._commit = value && true;
    this._rebuildURL();
  },

  // Similar to get(), but will page through the items `batchSize` at a time,
  // deferring calling the record handler until we've gotten them all.
  //
  // Returns the last response processed, and doesn't run the record handler
  // on any items if a non-success status is received while downloading the
  // records (or if a network error occurs).
  async getBatched(batchSize = DEFAULT_DOWNLOAD_BATCH_SIZE) {
    let totalLimit = Number(this.limit) || Infinity;
    if (batchSize <= 0 || batchSize >= totalLimit) {
      throw new Error("Invalid batch size");
    }

    if (!this.full) {
      throw new Error("getBatched is unimplemented for guid-only GETs");
    }

    // _onComplete and _onProgress are reset after each `get` by Resource.
    let { _onComplete, _onProgress } = this;
    let recordBuffer = [];
    let resp;
    try {
      let lastModifiedTime;
      this.limit = batchSize;

      do {
        this._onProgress = _onProgress;
        this._onComplete = _onComplete;
        if (batchSize + recordBuffer.length > totalLimit) {
          this.limit = totalLimit - recordBuffer.length;
        }
        this._log.trace("Performing batched GET", {
          limit: this.limit,
          offset: this.offset,
        });
        // Actually perform the request
        resp = await this.get();
        if (!resp.success) {
          recordBuffer = [];
          break;
        }
        for (let json of resp.obj) {
          let record = new this._recordObj();
          record.deserialize(json);
          recordBuffer.push(record);
        }

        // Initialize last modified, or check that something broken isn't happening.
        let lastModified = resp.headers["x-last-modified"];
        if (!lastModifiedTime) {
          lastModifiedTime = lastModified;
          this.setHeader("X-If-Unmodified-Since", lastModified);
        } else if (lastModified != lastModifiedTime) {
          // Should be impossible -- We'd get a 412 in this case.
          throw new Error(
            "X-Last-Modified changed in the middle of a download batch! " +
              `${lastModified} => ${lastModifiedTime}`
          );
        }

        // If this is missing, we're finished.
        this.offset = resp.headers["x-weave-next-offset"];
      } while (this.offset && totalLimit > recordBuffer.length);
    } finally {
      // Ensure we undo any temporary state so that subsequent calls to get()
      // or getBatched() work properly. We do this before calling the record
      // handler so that we can more convincingly pretend to be a normal get()
      // call. Note: we're resetting these to the values they had before this
      // function was called.
      this._limit = totalLimit;
      this._offset = null;
      delete this._headers["x-if-unmodified-since"];
      this._rebuildURL();
    }
    return { response: resp, records: recordBuffer };
  },

  // This object only supports posting via the postQueue object.
  post() {
    throw new Error(
      "Don't directly post to a collection - use newPostQueue instead"
    );
  },

  newPostQueue(log, timestamp, postCallback) {
    let poster = (data, headers, batch, commit) => {
      this.batch = batch;
      this.commit = commit;
      for (let [header, value] of headers) {
        this.setHeader(header, value);
      }
      return Resource.prototype.post.call(this, data);
    };
    return new PostQueue(
      poster,
      timestamp,
      this._service.serverConfiguration || {},
      log,
      postCallback
    );
  },
};

// These are limits for requests provided by the server at the
// info/configuration endpoint -- server documentation is available here:
// http://moz-services-docs.readthedocs.io/en/latest/storage/apis-1.5.html#api-instructions
//
// All are optional, however we synthesize (non-infinite) default values for the
// "max_request_bytes" and "max_record_payload_bytes" options. For the others,
// we ignore them (we treat the limit is infinite) if they're missing.
//
// These are also the only ones that all servers (even batching-disabled
// servers) should support, at least once this sync-serverstorage patch is
// everywhere https://github.com/mozilla-services/server-syncstorage/pull/74
//
// Batching enabled servers also limit the amount of payload data and number
// of and records we can send in a single post as well as in the whole batch.
// Note that the byte limits for these there are just with respect to the
// *payload* data, e.g. the data appearing in the payload property (a
// string) of the object.
//
// Note that in practice, these limits should be sensible, but the code makes
// no assumptions about this. If we hit any of the limits, we perform the
// corresponding action (e.g. submit a request, possibly committing the
// current batch).
const DefaultPostQueueConfig = Object.freeze({
  // Number of total bytes allowed in a request
  max_request_bytes: 260 * 1024,

  // Maximum number of bytes allowed in the "payload" property of a record.
  max_record_payload_bytes: 256 * 1024,

  // The limit for how many bytes worth of data appearing in "payload"
  // properties are allowed in a single post.
  max_post_bytes: Infinity,

  // The limit for the number of records allowed in a single post.
  max_post_records: Infinity,

  // The limit for how many bytes worth of data appearing in "payload"
  // properties are allowed in a batch. (Same as max_post_bytes, but for
  // batches).
  max_total_bytes: Infinity,

  // The limit for the number of records allowed in a single post. (Same
  // as max_post_records, but for batches).
  max_total_records: Infinity,
});

// Manages a pair of (byte, count) limits for a PostQueue, such as
// (max_post_bytes, max_post_records) or (max_total_bytes, max_total_records).
class LimitTracker {
  constructor(maxBytes, maxRecords) {
    this.maxBytes = maxBytes;
    this.maxRecords = maxRecords;
    this.curBytes = 0;
    this.curRecords = 0;
  }

  clear() {
    this.curBytes = 0;
    this.curRecords = 0;
  }

  canAddRecord(payloadSize) {
    // The record counts are inclusive, but depending on the version of the
    // server, the byte counts may or may not be inclusive (See
    // https://github.com/mozilla-services/server-syncstorage/issues/73).
    return (
      this.curRecords + 1 <= this.maxRecords &&
      this.curBytes + payloadSize < this.maxBytes
    );
  }

  canNeverAdd(recordSize) {
    return recordSize >= this.maxBytes;
  }

  didAddRecord(recordSize) {
    if (!this.canAddRecord(recordSize)) {
      // This is a bug, caller is expected to call canAddRecord first.
      throw new Error(
        "LimitTracker.canAddRecord must be checked before adding record"
      );
    }
    this.curRecords += 1;
    this.curBytes += recordSize;
  }
}

/* A helper to manage the posting of records while respecting the various
   size limits.

   This supports the concept of a server-side "batch". The general idea is:
   * We queue as many records as allowed in memory, then make a single POST.
   * This first POST (optionally) gives us a batch ID, which we use for
     all subsequent posts, until...
   * At some point we hit a batch-maximum, and jump through a few hoops to
     commit the current batch (ie, all previous POSTs) and start a new one.
   * Eventually commit the final batch.

  In most cases we expect there to be exactly 1 batch consisting of possibly
  multiple POSTs.
*/
function PostQueue(poster, timestamp, serverConfig, log, postCallback) {
  // The "post" function we should use when it comes time to do the post.
  this.poster = poster;
  this.log = log;

  let config = Object.assign({}, DefaultPostQueueConfig, serverConfig);

  if (!serverConfig.max_request_bytes && serverConfig.max_post_bytes) {
    // Use max_post_bytes for max_request_bytes if it's missing. Only needed
    // until server-syncstorage/pull/74 is everywhere, and even then it's
    // unnecessary if the server limits are configured sanely (there's no
    // guarantee of -- at least before that is fully deployed)
    config.max_request_bytes = serverConfig.max_post_bytes;
  }

  this.log.trace("new PostQueue config (after defaults): ", config);

  // The callback we make with the response when we do get around to making the
  // post (which could be during any of the enqueue() calls or the final flush())
  // This callback may be called multiple times and must not add new items to
  // the queue.
  // The second argument passed to this callback is a boolean value that is true
  // if we're in the middle of a batch, and false if either the batch is
  // complete, or it's a post to a server that does not understand batching.
  this.postCallback = postCallback;

  // Tracks the count and combined payload size for the records we've queued
  // so far but are yet to POST.
  this.postLimits = new LimitTracker(
    config.max_post_bytes,
    config.max_post_records
  );

  // As above, but for the batch size.
  this.batchLimits = new LimitTracker(
    config.max_total_bytes,
    config.max_total_records
  );

  // Limit for the size of `this.queued` before we do a post.
  this.maxRequestBytes = config.max_request_bytes;

  // Limit for the size of incoming record payloads.
  this.maxPayloadBytes = config.max_record_payload_bytes;

  // The string where we are capturing the stringified version of the records
  // queued so far. It will always be invalid JSON as it is always missing the
  // closing bracket. It's also used to track whether or not we've gone past
  // maxRequestBytes.
  this.queued = "";

  // The ID of our current batch. Can be undefined (meaning we are yet to make
  // the first post of a patch, so don't know if we have a batch), null (meaning
  // we've made the first post but the server response indicated no batching
  // semantics), otherwise we have made the first post and it holds the batch ID
  // returned from the server.
  this.batchID = undefined;

  // Time used for X-If-Unmodified-Since -- should be the timestamp from the last GET.
  this.lastModified = timestamp;
}

PostQueue.prototype = {
  async enqueue(record) {
    // We want to ensure the record has a .toJSON() method defined - even
    // though JSON.stringify() would implicitly call it, the stringify might
    // still work even if it isn't defined, which isn't what we want.
    let jsonRepr = record.toJSON();
    if (!jsonRepr) {
      throw new Error(
        "You must only call this with objects that explicitly support JSON"
      );
    }

    let bytes = JSON.stringify(jsonRepr);

    // We use the payload size for the LimitTrackers, since that's what the
    // byte limits other than max_request_bytes refer to.
    let payloadLength = jsonRepr.payload.length;

    // The `+ 2` is to account for the 2-byte (maximum) overhead (one byte for
    // the leading comma or "[", which all records will have, and the other for
    // the final trailing "]", only present for the last record).
    let encodedLength = bytes.length + 2;

    // Check first if there's some limit that indicates we cannot ever enqueue
    // this record.
    let isTooBig =
      this.postLimits.canNeverAdd(payloadLength) ||
      this.batchLimits.canNeverAdd(payloadLength) ||
      encodedLength >= this.maxRequestBytes ||
      payloadLength >= this.maxPayloadBytes;

    if (isTooBig) {
      return {
        enqueued: false,
        error: new Error("Single record too large to submit to server"),
      };
    }

    let canPostRecord = this.postLimits.canAddRecord(payloadLength);
    let canBatchRecord = this.batchLimits.canAddRecord(payloadLength);
    let canSendRecord =
      this.queued.length + encodedLength < this.maxRequestBytes;

    if (!canPostRecord || !canBatchRecord || !canSendRecord) {
      this.log.trace("PostQueue flushing: ", {
        canPostRecord,
        canSendRecord,
        canBatchRecord,
      });
      // We need to write the queue out before handling this one, but we only
      // commit the batch (and thus start a new one) if the record couldn't fit
      // inside the batch.
      await this.flush(!canBatchRecord);
    }

    this.postLimits.didAddRecord(payloadLength);
    this.batchLimits.didAddRecord(payloadLength);

    // Either a ',' or a '[' depending on whether this is the first record.
    this.queued += this.queued.length ? "," : "[";
    this.queued += bytes;
    return { enqueued: true };
  },

  async flush(finalBatchPost) {
    if (!this.queued) {
      // nothing queued - we can't be in a batch, and something has gone very
      // bad if we think we are.
      if (this.batchID) {
        throw new Error(
          `Flush called when no queued records but we are in a batch ${this.batchID}`
        );
      }
      return;
    }
    // the batch query-param and headers we'll send.
    let batch;
    let headers = [];
    if (this.batchID === undefined) {
      // First commit in a (possible) batch.
      batch = "true";
    } else if (this.batchID) {
      // We have an existing batch.
      batch = this.batchID;
    } else {
      // Not the first post and we know we have no batch semantics.
      batch = null;
    }

    headers.push(["x-if-unmodified-since", this.lastModified]);

    let numQueued = this.postLimits.curRecords;
    this.log.info(
      `Posting ${numQueued} records of ${this.queued.length +
        1} bytes with batch=${batch}`
    );
    let queued = this.queued + "]";
    if (finalBatchPost) {
      this.batchLimits.clear();
    }
    this.postLimits.clear();
    this.queued = "";
    let response = await this.poster(
      queued,
      headers,
      batch,
      !!(finalBatchPost && this.batchID !== null)
    );

    if (!response.success) {
      this.log.trace("Server error response during a batch", response);
      // not clear what we should do here - we expect the consumer of this to
      // abort by throwing in the postCallback below.
      await this.postCallback(this, response, !finalBatchPost);
      return;
    }

    if (finalBatchPost) {
      this.log.trace("Committed batch", this.batchID);
      this.batchID = undefined; // we are now in "first post for the batch" state.
      this.lastModified = response.headers["x-last-modified"];
      await this.postCallback(this, response, false);
      return;
    }

    if (response.status != 202) {
      if (this.batchID) {
        throw new Error(
          "Server responded non-202 success code while a batch was in progress"
        );
      }
      this.batchID = null; // no batch semantics are in place.
      this.lastModified = response.headers["x-last-modified"];
      await this.postCallback(this, response, false);
      return;
    }

    // this response is saying the server has batch semantics - we should
    // always have a batch ID in the response.
    let responseBatchID = response.obj.batch;
    this.log.trace("Server responsed 202 with batch", responseBatchID);
    if (!responseBatchID) {
      this.log.error(
        "Invalid server response: 202 without a batch ID",
        response
      );
      throw new Error("Invalid server response: 202 without a batch ID");
    }

    if (this.batchID === undefined) {
      this.batchID = responseBatchID;
      if (!this.lastModified) {
        this.lastModified = response.headers["x-last-modified"];
        if (!this.lastModified) {
          throw new Error("Batch response without x-last-modified");
        }
      }
    }

    if (this.batchID != responseBatchID) {
      throw new Error(
        `Invalid client/server batch state - client has ${this.batchID}, server has ${responseBatchID}`
      );
    }

    await this.postCallback(this, response, true);
  },
};
