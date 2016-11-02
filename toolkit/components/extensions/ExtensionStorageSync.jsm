/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionStorageSync"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;
const global = this;

const STORAGE_SYNC_ENABLED_PREF = "webextensions.storage.sync.enabled";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
const {
  runSafeSyncWithoutClone,
} = Cu.import("resource://gre/modules/ExtensionUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AppsUtils",
                                  "resource://gre/modules/AppsUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionStorage",
                                  "resource://gre/modules/ExtensionStorage.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "loadKinto",
                                  "resource://services-common/kinto-offline-client.js");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyPreferenceGetter(this, "prefPermitsStorageSync",
                                      STORAGE_SYNC_ENABLED_PREF, false);

/* globals prefPermitsStorageSync */

// Map of Extensions to Promise<Collections>.
const collectionPromises = new Map();
// Map of Extensions to Set<Contexts> to track contexts that are still
// "live" and could still use this collection.
const extensionContexts = new WeakMap();

// Kinto record IDs have two condtions:
//
// - They must contain only ASCII alphanumerics plus - and _. To fix
// this, we encode all non-letters using _C_, where C is the
// percent-encoded character, so space becomes _20_
// and underscore becomes _5F_.
//
// - They must start with an ASCII letter. To ensure this, we prefix
// all keys with "key-".
function keyToId(key) {
  function escapeChar(match) {
    return "_" + match.codePointAt(0).toString(16).toUpperCase() + "_";
  }
  return "key-" + key.replace(/[^a-zA-Z0-9]/g, escapeChar);
}

// Convert a Kinto ID back into a chrome.storage key.
// Returns null if a key couldn't be parsed.
function idToKey(id) {
  function unescapeNumber(match, group1) {
    return String.fromCodePoint(parseInt(group1, 16));
  }
  // An escaped ID should match this regex.
  // An escaped ID should consist of only letters and numbers, plus
  // code points escaped as _[0-9a-f]+_.
  const ESCAPED_ID_FORMAT = /^(?:[a-zA-Z0-9]|_[0-9A-F]+_)*$/;

  if (!id.startsWith("key-")) {
    return null;
  }
  const unprefixed = id.slice(4);
  // Verify that the ID is the correct format.
  if (!ESCAPED_ID_FORMAT.test(unprefixed)) {
    return null;
  }
  return unprefixed.replace(/_([0-9A-F]+)_/g, unescapeNumber);
}

// An "id schema" used to validate Kinto IDs and generate new ones.
const storageSyncIdSchema = {
  // We should never generate IDs; chrome.storage only acts as a
  // key-value store, so we should always have a key.
  generate() {
    throw new Error("cannot generate IDs");
  },

  // See keyToId and idToKey for more details.
  validate(id) {
    return idToKey(id) !== null;
  },
};

/**
 * Return a KintoBase object, suitable for using in Firefox.
 *
 * This centralizes the logic used to create Kinto instances, which
 * we will need to do in several places.
 *
 * @returns {Kinto}
 */
function makeKinto() {
  const Kinto = loadKinto();
  return new Kinto({
    adapter: Kinto.adapters.FirefoxAdapter,
    adapterOptions: {path: "storage-sync.sqlite"},
  });
}

/**
 * Actually for-real close the collection associated with a
 * collection.
 *
 * @param {Extension} extension
 *                    The extension whose uses are all over.
 * @returns {Promise<()>} Promise that resolves when everything is clean.
 */
function closeExtensionCollection(extension) {
  const collectionPromise = collectionPromises.get(extension);
  if (!collectionPromise) {
    Cu.reportError(new Error(`Internal error: trying to close extension ${extension.id}` +
                             "that doesn't have a collection"));
    return;
  }
  collectionPromises.delete(extension);
  return collectionPromise.then(coll => {
    return coll.db.close();
  });
}

/**
 * Clean up now that one context is no longer using this extension's collection.
 *
 * @param {Extension} extension
 *                    The extension whose context just ended.
 * @param {Context} context
 *                  The context that just ended.
 * @returns {Promise<()>} Promise that resolves when everything is clean.
 */
function cleanUpForContext(extension, context) {
  const contexts = extensionContexts.get(extension);
  if (!contexts) {
    Cu.reportError(new Error(`Internal error: cannot find any contexts for extension ${extension.id}`));
    // Try to shut down cleanly anyhow?
    return closeExtensionCollection(extension);
  }
  contexts.delete(context);
  if (contexts.size === 0) {
    // Nobody else is using this collection. Clean up.
    extensionContexts.delete(extension);
    return closeExtensionCollection(extension);
  }
}

/**
 * Generate a promise that produces the Collection for an extension.
 *
 * @param {Extension} extension
 *                    The extension whose collection needs to
 *                    be opened.
 * @param {Context} context
 *                  The context for this extension. The Collection
 *                  will shut down automatically when all contexts
 *                  close.
 * @returns {Promise<Collection>}
 */
const openCollection = Task.async(function* (extension, context) {
  // FIXME: This leaks metadata about what extensions a user has
  // installed.  We should calculate collection ID using a hash of
  // user ID, extension ID, and some secret.
  let collectionId = extension.id;
  // TODO: implement sync process
  const db = makeKinto();
  const coll = db.collection(collectionId, {
    idSchema: storageSyncIdSchema,
  });
  yield coll.db.open();
  return coll;
});

this.ExtensionStorageSync = {
  listeners: new WeakMap(),

  /**
   * Get the collection for an extension, consulting a cache to
   * save time.
   *
   * @param {Extension} extension
   *                    The extension for which we are seeking
   *                    a collection.
   * @param {Context} context
   *                  The context of the extension, so that we can
   *                  clean up the collection when the extension ends.
   * @returns {Promise<Collection>}
   */
  getCollection(extension, context) {
    if (prefPermitsStorageSync !== true) {
      return Promise.reject({message: `Please set ${STORAGE_SYNC_ENABLED_PREF} to true in about:config`});
    }
    if (!collectionPromises.has(extension)) {
      const collectionPromise = openCollection(extension, context);
      collectionPromises.set(extension, collectionPromise);
      collectionPromise.catch(Cu.reportError);
    }

    // Register that the extension and context are in use.
    if (!extensionContexts.has(extension)) {
      extensionContexts.set(extension, new Set());
    }
    const contexts = extensionContexts.get(extension);
    if (!contexts.has(context)) {
      // New context. Register it and make sure it cleans itself up
      // when it closes.
      contexts.add(context);
      context.callOnClose({
        close: () => cleanUpForContext(extension, context),
      });
    }

    return collectionPromises.get(extension);
  },

  set: Task.async(function* (extension, items, context) {
    const coll = yield this.getCollection(extension, context);
    const keys = Object.keys(items);
    const ids = keys.map(keyToId);
    const changes = yield coll.execute(txn => {
      let changes = {};
      for (let [i, key] of keys.entries()) {
        const id = ids[i];
        let item = items[key];
        let {oldRecord} = txn.upsert({
          id,
          key,
          data: item,
        });
        changes[key] = {
          newValue: item,
        };
        if (oldRecord && oldRecord.data) {
          // Extract the "data" field from the old record, which
          // represents the value part of the key-value store
          changes[key].oldValue = oldRecord.data;
        }
      }
      return changes;
    }, {preloadIds: ids});
    this.notifyListeners(extension, changes);
  }),

  remove: Task.async(function* (extension, keys, context) {
    const coll = yield this.getCollection(extension, context);
    keys = [].concat(keys);
    const ids = keys.map(keyToId);
    let changes = {};
    yield coll.execute(txn => {
      for (let [i, key] of keys.entries()) {
        const id = ids[i];
        const res = txn.deleteAny(id);
        if (res.deleted) {
          changes[key] = {
            oldValue: res.data.data,
          };
        }
      }
      return changes;
    }, {preloadIds: ids});
    if (Object.keys(changes).length > 0) {
      this.notifyListeners(extension, changes);
    }
  }),

  clear: Task.async(function* (extension, context) {
    // We can't call Collection#clear here, because that just clears
    // the local database. We have to explicitly delete everything so
    // that the deletions can be synced as well.
    const coll = yield this.getCollection(extension, context);
    const res = yield coll.list();
    const records = res.data;
    const keys = records.map(record => record.key);
    yield this.remove(extension, keys, context);
  }),

  get: Task.async(function* (extension, spec, context) {
    const coll = yield this.getCollection(extension, context);
    let keys, records;
    if (spec === null) {
      records = {};
      const res = yield coll.list();
      for (let record of res.data) {
        records[record.key] = record.data;
      }
      return records;
    }
    if (typeof spec === "string") {
      keys = [spec];
      records = {};
    } else if (Array.isArray(spec)) {
      keys = spec;
      records = {};
    } else {
      keys = Object.keys(spec);
      records = Cu.cloneInto(spec, global);
    }

    for (let key of keys) {
      const res = yield coll.getAny(keyToId(key));
      if (res.data && res.data._status != "deleted") {
        records[res.data.key] = res.data.data;
      }
    }

    return records;
  }),

  addOnChangedListener(extension, listener) {
    let listeners = this.listeners.get(extension) || new Set();
    listeners.add(listener);
    this.listeners.set(extension, listeners);
  },

  removeOnChangedListener(extension, listener) {
    let listeners = this.listeners.get(extension);
    listeners.delete(listener);
    if (listeners.size == 0) {
      this.listeners.delete(extension);
    }
  },

  notifyListeners(extension, changes) {
    let listeners = this.listeners.get(extension) || new Set();
    if (listeners) {
      for (let listener of listeners) {
        runSafeSyncWithoutClone(listener, changes);
      }
    }
  },
};
