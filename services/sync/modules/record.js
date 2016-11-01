/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [
  "WBORecord",
  "RecordManager",
  "CryptoWrapper",
  "CollectionKeyManager",
  "Collection",
];

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cr = Components.results;
var Cu = Components.utils;

const CRYPTO_COLLECTION = "crypto";
const KEYS_WBO = "keys";

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/keys.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-common/async.js");

this.WBORecord = function WBORecord(collection, id) {
  this.data = {};
  this.payload = {};
  this.collection = collection;      // Optional.
  this.id = id;                      // Optional.
}
WBORecord.prototype = {
  _logName: "Sync.Record.WBO",

  get sortindex() {
    if (this.data.sortindex)
      return this.data.sortindex;
    return 0;
  },

  // Get thyself from your URI, then deserialize.
  // Set thine 'response' field.
  fetch: function fetch(resource) {
    if (!resource instanceof Resource) {
      throw new Error("First argument must be a Resource instance.");
    }

    let r = resource.get();
    if (r.success) {
      this.deserialize(r);   // Warning! Muffles exceptions!
    }
    this.response = r;
    return this;
  },

  upload: function upload(resource) {
    if (!resource instanceof Resource) {
      throw new Error("First argument must be a Resource instance.");
    }

    return resource.put(this);
  },

  // Take a base URI string, with trailing slash, and return the URI of this
  // WBO based on collection and ID.
  uri: function(base) {
    if (this.collection && this.id) {
      let url = Utils.makeURI(base + this.collection + "/" + this.id);
      url.QueryInterface(Ci.nsIURL);
      return url;
    }
    return null;
  },

  deserialize: function deserialize(json) {
    this.data = json.constructor.toString() == String ? JSON.parse(json) : json;

    try {
      // The payload is likely to be JSON, but if not, keep it as a string
      this.payload = JSON.parse(this.payload);
    } catch(ex) {}
  },

  toJSON: function toJSON() {
    // Copy fields from data to be stringified, making sure payload is a string
    let obj = {};
    for (let [key, val] of Object.entries(this.data))
      obj[key] = key == "payload" ? JSON.stringify(val) : val;
    if (this.ttl)
      obj.ttl = this.ttl;
    return obj;
  },

  toString: function toString() {
    return "{ " +
      "id: "       + this.id        + "  " +
      "index: "    + this.sortindex + "  " +
      "modified: " + this.modified  + "  " +
      "ttl: "      + this.ttl       + "  " +
      "payload: "  + JSON.stringify(this.payload) +
      " }";
  }
};

Utils.deferGetSet(WBORecord, "data", ["id", "modified", "sortindex", "payload"]);

this.CryptoWrapper = function CryptoWrapper(collection, id) {
  this.cleartext = {};
  WBORecord.call(this, collection, id);
  this.ciphertext = null;
  this.id = id;
}
CryptoWrapper.prototype = {
  __proto__: WBORecord.prototype,
  _logName: "Sync.Record.CryptoWrapper",

  ciphertextHMAC: function ciphertextHMAC(keyBundle) {
    let hasher = keyBundle.sha256HMACHasher;
    if (!hasher) {
      throw "Cannot compute HMAC without an HMAC key.";
    }

    return Utils.bytesAsHex(Utils.digestUTF8(this.ciphertext, hasher));
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
  encrypt: function encrypt(keyBundle) {
    if (!keyBundle) {
      throw new Error("A key bundle must be supplied to encrypt.");
    }

    this.IV = Svc.Crypto.generateRandomIV();
    this.ciphertext = Svc.Crypto.encrypt(JSON.stringify(this.cleartext),
                                         keyBundle.encryptionKeyB64, this.IV);
    this.hmac = this.ciphertextHMAC(keyBundle);
    this.cleartext = null;
  },

  // Optional key bundle.
  decrypt: function decrypt(keyBundle) {
    if (!this.ciphertext) {
      throw "No ciphertext: nothing to decrypt?";
    }

    if (!keyBundle) {
      throw new Error("A key bundle must be supplied to decrypt.");
    }

    // Authenticate the encrypted blob with the expected HMAC
    let computedHMAC = this.ciphertextHMAC(keyBundle);

    if (computedHMAC != this.hmac) {
      Utils.throwHMACMismatch(this.hmac, computedHMAC);
    }

    // Handle invalid data here. Elsewhere we assume that cleartext is an object.
    let cleartext = Svc.Crypto.decrypt(this.ciphertext,
                                       keyBundle.encryptionKeyB64, this.IV);
    let json_result = JSON.parse(cleartext);

    if (json_result && (json_result instanceof Object)) {
      this.cleartext = json_result;
      this.ciphertext = null;
    } else {
      throw "Decryption failed: result is <" + json_result + ">, not an object.";
    }

    // Verify that the encrypted id matches the requested record's id.
    if (this.cleartext.id != this.id)
      throw "Record id mismatch: " + this.cleartext.id + " != " + this.id;

    return this.cleartext;
  },

  toString: function toString() {
    let payload = this.deleted ? "DELETED" : JSON.stringify(this.cleartext);

    return "{ " +
      "id: "         + this.id          + "  " +
      "index: "      + this.sortindex   + "  " +
      "modified: "   + this.modified    + "  " +
      "ttl: "        + this.ttl         + "  " +
      "payload: "    + payload          + "  " +
      "collection: " + (this.collection || "undefined") +
      " }";
  },

  // The custom setter below masks the parent's getter, so explicitly call it :(
  get id() {
    return WBORecord.prototype.__lookupGetter__("id").call(this);
  },

  // Keep both plaintext and encrypted versions of the id to verify integrity
  set id(val) {
    WBORecord.prototype.__lookupSetter__("id").call(this, val);
    return this.cleartext.id = val;
  },
};

Utils.deferGetSet(CryptoWrapper, "payload", ["ciphertext", "IV", "hmac"]);
Utils.deferGetSet(CryptoWrapper, "cleartext", "deleted");

/**
 * An interface and caching layer for records.
 */
this.RecordManager = function RecordManager(service) {
  this.service = service;

  this._log = Log.repository.getLogger(this._logName);
  this._records = {};
}
RecordManager.prototype = {
  _recordType: CryptoWrapper,
  _logName: "Sync.RecordManager",

  import: function RecordMgr_import(url) {
    this._log.trace("Importing record: " + (url.spec ? url.spec : url));
    try {
      // Clear out the last response with empty object if GET fails
      this.response = {};
      this.response = this.service.resource(url).get();

      // Don't parse and save the record on failure
      if (!this.response.success)
        return null;

      let record = new this._recordType(url);
      record.deserialize(this.response);

      return this.set(url, record);
    } catch (ex) {
      if (Async.isShutdownException(ex)) {
        throw ex;
      }
      this._log.debug("Failed to import record", ex);
      return null;
    }
  },

  get: function RecordMgr_get(url) {
    // Use a url string as the key to the hash
    let spec = url.spec ? url.spec : url;
    if (spec in this._records)
      return this._records[spec];
    return this.import(url);
  },

  set: function RecordMgr_set(url, record) {
    let spec = url.spec ? url.spec : url;
    return this._records[spec] = record;
  },

  contains: function RecordMgr_contains(url) {
    if ((url.spec || url) in this._records)
      return true;
    return false;
  },

  clearCache: function recordMgr_clearCache() {
    this._records = {};
  },

  del: function RecordMgr_del(url) {
    delete this._records[url];
  }
};

/**
 * Keeps track of mappings between collection names ('tabs') and KeyBundles.
 *
 * You can update this thing simply by giving it /info/collections. It'll
 * use the last modified time to bring itself up to date.
 */
this.CollectionKeyManager = function CollectionKeyManager() {
  this.lastModified = 0;
  this._collections = {};
  this._default = null;

  this._log = Log.repository.getLogger("Sync.CollectionKeyManager");
}

// TODO: persist this locally as an Identity. Bug 610913.
// Note that the last modified time needs to be preserved.
CollectionKeyManager.prototype = {

  // Return information about old vs new keys:
  // * same: true if two collections are equal
  // * changed: an array of collection names that changed.
  _compareKeyBundleCollections: function _compareKeyBundleCollections(m1, m2) {
    let changed = [];

    function process(m1, m2) {
      for (let k1 in m1) {
        let v1 = m1[k1];
        let v2 = m2[k1];
        if (!(v1 && v2 && v1.equals(v2)))
          changed.push(k1);
      }
    }

    // Diffs both ways.
    process(m1, m2);
    process(m2, m1);

    // Return a sorted, unique array.
    changed.sort();
    let last;
    changed = changed.filter(x => (x != last) && (last = x));
    return {same: changed.length == 0,
            changed: changed};
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

  keyForCollection: function(collection) {
    if (collection && this._collections[collection])
      return this._collections[collection];

    return this._default;
  },

  /**
   * If `collections` (an array of strings) is provided, iterate
   * over it and generate random keys for each collection.
   * Create a WBO for the given data.
   */
  _makeWBO: function(collections, defaultBundle) {
    let wbo = new CryptoWrapper(CRYPTO_COLLECTION, KEYS_WBO);
    let c = {};
    for (let k in collections) {
      c[k] = collections[k].keyPairB64;
    }
    wbo.cleartext = {
      "default":     defaultBundle ? defaultBundle.keyPairB64 : null,
      "collections": c,
      "collection":  CRYPTO_COLLECTION,
      "id":          KEYS_WBO
    };
    return wbo;
  },

  /**
   * Create a WBO for the current keys.
   */
  asWBO: function(collection, id) {
    return this._makeWBO(this._collections, this._default);
  },

  /**
   * Compute a new default key, and new keys for any specified collections.
   */
  newKeys: function(collections) {
    let newDefaultKey = new BulkKeyBundle(DEFAULT_KEYBUNDLE_NAME);
    newDefaultKey.generateRandom();

    let newColls = {};
    if (collections) {
      collections.forEach(function (c) {
        let b = new BulkKeyBundle(c);
        b.generateRandom();
        newColls[c] = b;
      });
    }
    return [newDefaultKey, newColls];
  },

  /**
   * Generates new keys, but does not replace our local copy. Use this to
   * verify an upload before storing.
   */
  generateNewKeysWBO: function(collections) {
    let newDefaultKey, newColls;
    [newDefaultKey, newColls] = this.newKeys(collections);

    return this._makeWBO(newColls, newDefaultKey);
  },

  // Take the fetched info/collections WBO, checking the change
  // time of the crypto collection.
  updateNeeded: function(info_collections) {

    this._log.info("Testing for updateNeeded. Last modified: " + this.lastModified);

    // No local record of modification time? Need an update.
    if (!this.lastModified)
      return true;

    // No keys on the server? We need an update, though our
    // update handling will be a little more drastic...
    if (!(CRYPTO_COLLECTION in info_collections))
      return true;

    // Otherwise, we need an update if our modification time is stale.
    return (info_collections[CRYPTO_COLLECTION] > this.lastModified);
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

    if (!modified)
      throw "No modified time provided to setContents.";

    let self = this;

    this._log.info("Setting collection keys contents. Our last modified: " +
                   this.lastModified + ", input modified: " + modified + ".");

    if (!payload)
      throw "No payload in CollectionKeyManager.setContents().";

    if (!payload.default) {
      this._log.warn("No downloaded default key: this should not occur.");
      this._log.warn("Not clearing local keys.");
      throw "No default key in CollectionKeyManager.setContents(). Cannot proceed.";
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
          if (keyObj) {
            newCollections[k] = keyObj;
          }
        }
      }
    }

    // Check to see if these are already our keys.
    let sameDefault = (this._default && this._default.equals(newDefault));
    let collComparison = this._compareKeyBundleCollections(newCollections, this._collections);
    let sameColls = collComparison.same;

    if (sameDefault && sameColls) {
      self._log.info("New keys are the same as our old keys! Bumped local modified time.");
      self.lastModified = modified;
      return false;
    }

    // Make sure things are nice and tidy before we set.
    this.clear();

    this._log.info("Saving downloaded keys.");
    this._default     = newDefault;
    this._collections = newCollections;

    // Always trust the server.
    self._log.info("Bumping last modified to " + modified);
    self.lastModified = modified;

    return sameDefault ? collComparison.changed : true;
  },

  updateContents: function updateContents(syncKeyBundle, storage_keys) {
    let log = this._log;
    log.info("Updating collection keys...");

    // storage_keys is a WBO, fetched from storage/crypto/keys.
    // Its payload is the default key, and a map of collections to keys.
    // We lazily compute the key objects from the strings we're given.

    let payload;
    try {
      payload = storage_keys.decrypt(syncKeyBundle);
    } catch (ex) {
      log.warn("Got exception \"" + ex + "\" decrypting storage keys with sync key.");
      log.info("Aborting updateContents. Rethrowing.");
      throw ex;
    }

    let r = this.setContents(payload, storage_keys.modified);
    log.info("Collection keys updated.");
    return r;
  }
}

this.Collection = function Collection(uri, recordObj, service) {
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
    if (this.older)
      args.push('older=' + this.older);
    else if (this.newer) {
      args.push('newer=' + this.newer);
    }
    if (this.full)
      args.push('full=1');
    if (this.sort)
      args.push('sort=' + this.sort);
    if (this.ids != null)
      args.push("ids=" + this.ids);
    if (this.limit > 0 && this.limit != Infinity)
      args.push("limit=" + this.limit);
    if (this._batch)
      args.push("batch=" + encodeURIComponent(this._batch));
    if (this._commit)
      args.push("commit=true");
    if (this._offset)
      args.push("offset=" + encodeURIComponent(this._offset));

    this.uri.query = (args.length > 0)? '?' + args.join('&') : '';
  },

  // get full items
  get full() { return this._full; },
  set full(value) {
    this._full = value;
    this._rebuildURL();
  },

  // Apply the action to a certain set of ids
  get ids() { return this._ids; },
  set ids(value) {
    this._ids = value;
    this._rebuildURL();
  },

  // Limit how many records to get
  get limit() { return this._limit; },
  set limit(value) {
    this._limit = value;
    this._rebuildURL();
  },

  // get only items modified before some date
  get older() { return this._older; },
  set older(value) {
    this._older = value;
    this._rebuildURL();
  },

  // get only items modified since some date
  get newer() { return this._newer; },
  set newer(value) {
    this._newer = value;
    this._rebuildURL();
  },

  // get items sorted by some criteria. valid values:
  // oldest (oldest first)
  // newest (newest first)
  // index
  get sort() { return this._sort; },
  set sort(value) {
    this._sort = value;
    this._rebuildURL();
  },

  get offset() { return this._offset; },
  set offset(value) {
    this._offset = value;
    this._rebuildURL();
  },

  // Set information about the batch for this request.
  get batch() { return this._batch; },
  set batch(value) {
    this._batch = value;
    this._rebuildURL();
  },

  get commit() { return this._commit; },
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
  getBatched(batchSize = DEFAULT_DOWNLOAD_BATCH_SIZE) {
    let totalLimit = Number(this.limit) || Infinity;
    if (batchSize <= 0 || batchSize >= totalLimit) {
      // Invalid batch sizes should arguably be an error, but they're easy to handle
      return this.get();
    }

    if (!this.full) {
      throw new Error("getBatched is unimplemented for guid-only GETs");
    }

    // _onComplete and _onProgress are reset after each `get` by AsyncResource.
    // We overwrite _onRecord to something that stores the data in an array
    // until the end.
    let { _onComplete, _onProgress, _onRecord } = this;
    let recordBuffer = [];
    let resp;
    try {
      this._onRecord = r => recordBuffer.push(r);
      let lastModifiedTime;
      this.limit = batchSize;

      do {
        this._onProgress = _onProgress;
        this._onComplete = _onComplete;
        if (batchSize + recordBuffer.length > totalLimit) {
          this.limit = totalLimit - recordBuffer.length;
        }
        this._log.trace("Performing batched GET", { limit: this.limit, offset: this.offset });
        // Actually perform the request
        resp = this.get();
        if (!resp.success) {
          break;
        }

        // Initialize last modified, or check that something broken isn't happening.
        let lastModified = resp.headers["x-last-modified"];
        if (!lastModifiedTime) {
          lastModifiedTime = lastModified;
          this.setHeader("X-If-Unmodified-Since", lastModified);
        } else if (lastModified != lastModifiedTime) {
          // Should be impossible -- We'd get a 412 in this case.
          throw new Error("X-Last-Modified changed in the middle of a download batch! " +
                          `${lastModified} => ${lastModifiedTime}`)
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
      this._onRecord = _onRecord;
      this._limit = totalLimit;
      this._offset = null;
      delete this._headers["x-if-unmodified-since"];
      this._rebuildURL();
    }
    if (resp.success && Async.checkAppReady()) {
      // call the original _onRecord (e.g. the user supplied record handler)
      // for each record we've stored
      for (let record of recordBuffer) {
        this._onRecord(record);
      }
    }
    return resp;
  },

  set recordHandler(onRecord) {
    // Save this because onProgress is called with this as the ChannelListener
    let coll = this;

    // Switch to newline separated records for incremental parsing
    coll.setHeader("Accept", "application/newlines");

    this._onRecord = onRecord;

    this._onProgress = function() {
      let newline;
      while ((newline = this._data.indexOf("\n")) > 0) {
        // Split the json record from the rest of the data
        let json = this._data.slice(0, newline);
        this._data = this._data.slice(newline + 1);

        // Deserialize a record from json and give it to the callback
        let record = new coll._recordObj();
        record.deserialize(json);
        coll._onRecord(record);
      }
    };
  },

  // This object only supports posting via the postQueue object.
  post() {
    throw new Error("Don't directly post to a collection - use newPostQueue instead");
  },

  newPostQueue(log, timestamp, postCallback) {
    let poster = (data, headers, batch, commit) => {
      this.batch = batch;
      this.commit = commit;
      for (let [header, value] of headers) {
        this.setHeader(header, value);
      }
      return Resource.prototype.post.call(this, data);
    }
    let getConfig = (name, defaultVal) => {
      if (this._service.serverConfiguration && this._service.serverConfiguration.hasOwnProperty(name)) {
        return this._service.serverConfiguration[name];
      }
      return defaultVal;
    }

    let config = {
      max_post_bytes: getConfig("max_post_bytes", MAX_UPLOAD_BYTES),
      max_post_records: getConfig("max_post_records", MAX_UPLOAD_RECORDS),

      max_batch_bytes: getConfig("max_total_bytes", Infinity),
      max_batch_records: getConfig("max_total_records", Infinity),
    }

    // Handle config edge cases
    if (config.max_post_records <= 0) { config.max_post_records = MAX_UPLOAD_RECORDS; }
    if (config.max_batch_records <= 0) { config.max_batch_records = Infinity; }
    if (config.max_post_bytes <= 0) { config.max_post_bytes = MAX_UPLOAD_BYTES; }
    if (config.max_batch_bytes <= 0) { config.max_batch_bytes = Infinity; }

    // Max size of BSO payload is 256k. This assumes at most 4k of overhead,
    // which sounds like plenty. If the server says it can't handle this, we
    // might have valid records we can't sync, so we give up on syncing.
    let requiredMax = 260 * 1024;
    if (config.max_post_bytes < requiredMax) {
      this._log.error("Server configuration max_post_bytes is too low", config);
      throw new Error("Server configuration max_post_bytes is too low");
    }

    return new PostQueue(poster, timestamp, config, log, postCallback);
  },
};

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
function PostQueue(poster, timestamp, config, log, postCallback) {
  // The "post" function we should use when it comes time to do the post.
  this.poster = poster;
  this.log = log;

  // The config we use. We expect it to have fields "max_post_records",
  // "max_batch_records", "max_post_bytes", and "max_batch_bytes"
  this.config = config;

  // The callback we make with the response when we do get around to making the
  // post (which could be during any of the enqueue() calls or the final flush())
  // This callback may be called multiple times and must not add new items to
  // the queue.
  // The second argument passed to this callback is a boolean value that is true
  // if we're in the middle of a batch, and false if either the batch is
  // complete, or it's a post to a server that does not understand batching.
  this.postCallback = postCallback;

  // The string where we are capturing the stringified version of the records
  // queued so far. It will always be invalid JSON as it is always missing the
  // closing bracket.
  this.queued = "";

  // The number of records we've queued so far but are yet to POST.
  this.numQueued = 0;

  // The number of records/bytes we've processed in previous POSTs for our
  // current batch. Does *not* include records currently queued for the next POST.
  this.numAlreadyBatched = 0;
  this.bytesAlreadyBatched = 0;

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
  enqueue(record) {
    // We want to ensure the record has a .toJSON() method defined - even
    // though JSON.stringify() would implicitly call it, the stringify might
    // still work even if it isn't defined, which isn't what we want.
    let jsonRepr = record.toJSON();
    if (!jsonRepr) {
      throw new Error("You must only call this with objects that explicitly support JSON");
    }
    let bytes = JSON.stringify(jsonRepr);

    // Do a flush if we can't add this record without exceeding our single-request
    // limits, or without exceeding the total limit for a single batch.
    let newLength = this.queued.length + bytes.length + 2; // extras for leading "[" / "," and trailing "]"

    let maxAllowedBytes = Math.min(256 * 1024, this.config.max_post_bytes);

    let postSizeExceeded = this.numQueued >= this.config.max_post_records ||
                           newLength >= maxAllowedBytes;

    let batchSizeExceeded = (this.numQueued + this.numAlreadyBatched) >= this.config.max_batch_records ||
                            (newLength + this.bytesAlreadyBatched) >= this.config.max_batch_bytes;

    let singleRecordTooBig = bytes.length + 2 > maxAllowedBytes;

    if (postSizeExceeded || batchSizeExceeded) {
      this.log.trace(`PostQueue flushing due to postSizeExceeded=${postSizeExceeded}, batchSizeExceeded=${batchSizeExceeded}` +
                     `, max_batch_bytes: ${this.config.max_batch_bytes}, max_post_bytes: ${this.config.max_post_bytes}`);

      if (singleRecordTooBig) {
        return { enqueued: false, error: new Error("Single record too large to submit to server") };
      }

      // We need to write the queue out before handling this one, but we only
      // commit the batch (and thus start a new one) if the batch is full.
      // Note that if a single record is too big for the batch or post, then
      // the batch may be empty, and so we don't flush in that case.
      if (this.numQueued) {
        this.flush(batchSizeExceeded || singleRecordTooBig);
      }
    }
    // Either a ',' or a '[' depending on whether this is the first record.
    this.queued += this.numQueued ? "," : "[";
    this.queued += bytes;
    this.numQueued++;
    return { enqueued: true };
  },

  flush(finalBatchPost) {
    if (!this.queued) {
      // nothing queued - we can't be in a batch, and something has gone very
      // bad if we think we are.
      if (this.batchID) {
        throw new Error(`Flush called when no queued records but we are in a batch ${this.batchID}`);
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

    this.log.info(`Posting ${this.numQueued} records of ${this.queued.length+1} bytes with batch=${batch}`);
    let queued = this.queued + "]";
    if (finalBatchPost) {
      this.bytesAlreadyBatched = 0;
      this.numAlreadyBatched = 0;
    } else {
      this.bytesAlreadyBatched += queued.length;
      this.numAlreadyBatched += this.numQueued;
    }
    this.queued = "";
    this.numQueued = 0;
    let response = this.poster(queued, headers, batch, !!(finalBatchPost && this.batchID !== null));

    if (!response.success) {
      this.log.trace("Server error response during a batch", response);
      // not clear what we should do here - we expect the consumer of this to
      // abort by throwing in the postCallback below.
      return this.postCallback(response, !finalBatchPost);
    }

    if (finalBatchPost) {
      this.log.trace("Committed batch", this.batchID);
      this.batchID = undefined; // we are now in "first post for the batch" state.
      this.lastModified = response.headers["x-last-modified"];
      return this.postCallback(response, false);
    }

    if (response.status != 202) {
      if (this.batchID) {
        throw new Error("Server responded non-202 success code while a batch was in progress");
      }
      this.batchID = null; // no batch semantics are in place.
      this.lastModified = response.headers["x-last-modified"];
      return this.postCallback(response, false);
    }

    // this response is saying the server has batch semantics - we should
    // always have a batch ID in the response.
    let responseBatchID = response.obj.batch;
    this.log.trace("Server responsed 202 with batch", responseBatchID);
    if (!responseBatchID) {
      this.log.error("Invalid server response: 202 without a batch ID", response);
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
      throw new Error(`Invalid client/server batch state - client has ${this.batchID}, server has ${responseBatchID}`);
    }

    this.postCallback(response, true);
  },
}
