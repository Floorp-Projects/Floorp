/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionStorageIDB"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/IndexedDB.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  ContextualIdentityService: "resource://gre/modules/ContextualIdentityService.jsm",
  ExtensionStorage: "resource://gre/modules/ExtensionStorage.jsm",
  ExtensionUtils: "resource://gre/modules/ExtensionUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  OS: "resource://gre/modules/osfile.jsm",
});

// The userContextID reserved for the extension storage (its purpose is ensuring that the IndexedDB
// storage used by the browser.storage.local API is not directly accessible from the extension code).
XPCOMUtils.defineLazyGetter(this, "WEBEXT_STORAGE_USER_CONTEXT_ID", () => {
  return ContextualIdentityService.getDefaultPrivateIdentity(
    "userContextIdInternal.webextStorageLocal").userContextId;
});

const IDB_NAME = "webExtensions-storage-local";
const IDB_DATA_STORENAME = "storage-local-data";
const IDB_VERSION = 1;
const IDB_MIGRATE_RESULT_HISTOGRAM = "WEBEXT_STORAGE_LOCAL_IDB_MIGRATE_RESULT_COUNT";

// Whether or not the installed extensions should be migrated to the storage.local IndexedDB backend.
const BACKEND_ENABLED_PREF = "extensions.webextensions.ExtensionStorageIDB.enabled";

class ExtensionStorageLocalIDB extends IndexedDB {
  onupgradeneeded(event) {
    if (event.oldVersion < 1) {
      this.createObjectStore(IDB_DATA_STORENAME);
    }
  }

  static openForPrincipal(storagePrincipal) {
    // The db is opened using an extension principal isolated in a reserved user context id.
    return super.openForPrincipal(storagePrincipal, IDB_NAME, IDB_VERSION);
  }

  async isEmpty() {
    const cursor = await this.objectStore(IDB_DATA_STORENAME, "readonly").openKeyCursor();
    return cursor.done;
  }

  /**
   * Asynchronously sets the values of the given storage items.
   *
   * @param {object} items
   *        The storage items to set. For each property in the object,
   *        the storage value for that property is set to its value in
   *        said object. Any values which are StructuredCloneHolder
   *        instances are deserialized before being stored.
   * @param {object}  options
   * @param {function} options.serialize
   *        Set to a function which will be used to serialize the values into
   *        a StructuredCloneHolder object (if appropriate) and being sent
   *        across the processes (it is also used to detect data cloning errors
   *        and raise an appropriate error to the caller).
   *
   * @returns {Promise<null|object>}
   *        Return a promise which resolves to the computed "changes" object
   *        or null.
   */
  async set(items, {serialize} = {}) {
    const changes = {};
    let changed = false;

    // Explicitly create a transaction, so that we can explicitly abort it
    // as soon as one of the put requests fails.
    const transaction = this.transaction(IDB_DATA_STORENAME, "readwrite");
    const objectStore = transaction.objectStore(IDB_DATA_STORENAME, "readwrite");

    for (let key of Object.keys(items)) {
      try {
        let oldValue = await objectStore.get(key);

        await objectStore.put(items[key], key);

        changes[key] = {
          oldValue: oldValue && serialize ? serialize(oldValue) : oldValue,
          newValue: serialize ? serialize(items[key]) : items[key],
        };
        changed = true;
      } catch (err) {
        transaction.abort();

        // Ensure that the error we throw is converted into an ExtensionError
        // (e.g. DataCloneError instances raised from the internal IndexedDB
        // operation have to be converted to be accessible to the extension code).
        throw new ExtensionUtils.ExtensionError(String(err));
      }
    }

    return changed ? changes : null;
  }

  /**
   * Asynchronously retrieves the values for the given storage items.
   *
   * @param {Array<string>|object|null} [keysOrItems]
   *        The storage items to get. If an array, the value of each key
   *        in the array is returned. If null, the values of all items
   *        are returned. If an object, the value for each key in the
   *        object is returned, or that key's value if the item is not
   *        set.
   * @returns {Promise<object>}
   *        An object which has a property for each requested key,
   *        containing that key's value as stored in the IndexedDB
   *        storage.
   */
  async get(keysOrItems) {
    let keys;
    let defaultValues;

    if (Array.isArray(keysOrItems)) {
      keys = keysOrItems;
    } else if (keysOrItems && typeof(keysOrItems) === "object") {
      keys = Object.keys(keysOrItems);
      defaultValues = keysOrItems;
    }

    const result = {};

    // Retrieve all the stored data using a cursor when browser.storage.local.get()
    // has been called with no keys.
    if (keys == null) {
      const cursor = await this.objectStore(IDB_DATA_STORENAME, "readonly").openCursor();
      while (!cursor.done) {
        result[cursor.key] = cursor.value;
        await cursor.continue();
      }
    } else {
      const objectStore = this.objectStore(IDB_DATA_STORENAME);
      for (let key of keys) {
        const storedValue = await objectStore.get(key);
        if (storedValue === undefined) {
          if (defaultValues && defaultValues[key] !== undefined) {
            result[key] = defaultValues[key];
          }
        } else {
          result[key] = storedValue;
        }
      }
    }

    return result;
  }

  /**
   * Asynchronously removes the given storage items.
   *
   * @param {string|Array<string>} keys
   *        A string key of a list of storage items keys to remove.
   * @returns {Promise<Object>}
   *          Returns an object which contains applied changes.
   */
  async remove(keys) {
    // Ensure that keys is an array of strings.
    keys = [].concat(keys);

    if (keys.length === 0) {
      // Early exit if there is nothing to remove.
      return null;
    }

    const changes = {};
    let changed = false;

    const objectStore = this.objectStore(IDB_DATA_STORENAME, "readwrite");

    for (let key of keys) {
      let oldValue = await objectStore.get(key);
      changes[key] = {oldValue};

      if (oldValue) {
        changed = true;
      }

      await objectStore.delete(key);
    }

    return changed ? changes : null;
  }

  /**
   * Asynchronously clears all storage entries.
   *
   * @returns {Promise<Object>}
   *          Returns an object which contains applied changes.
   */
  async clear() {
    const changes = {};
    let changed = false;

    const objectStore = this.objectStore(IDB_DATA_STORENAME, "readwrite");

    const cursor = await objectStore.openCursor();
    while (!cursor.done) {
      changes[cursor.key] = {oldValue: cursor.value};
      changed = true;
      await cursor.continue();
    }

    await objectStore.clear();

    return changed ? changes : null;
  }
}

/**
 * Migrate the data stored in the JSONFile backend to the IDB Backend.
 *
 * Returns a promise which is resolved once the data migration has been
 * completed and the new IDB backend can be enabled.
 * Rejects if the data has been read successfully from the JSONFile backend
 * but it failed to be saved in the new IDB backend.
 *
 * This method is called only from the main process (where the file
 * can be opened).
 *
 * @param {Extension} extension
 *        The extension to migrate to the new IDB backend.
 * @param {nsIPrincipal} storagePrincipal
 *        The "internally reserved" extension storagePrincipal to be used to create
 *        the ExtensionStorageLocalIDB instance.
 */
async function migrateJSONFileData(extension, storagePrincipal) {
  let oldStoragePath;
  let oldStorageExists;
  let idbConn;
  let jsonFile;
  let hasEmptyIDB;
  let oldDataRead = false;
  let migrated = false;
  let histogram = Services.telemetry.getHistogramById(IDB_MIGRATE_RESULT_HISTOGRAM);

  try {
    idbConn = await ExtensionStorageLocalIDB.openForPrincipal(storagePrincipal);
    hasEmptyIDB = await idbConn.isEmpty();

    if (!hasEmptyIDB) {
      // If the IDB backend is enabled and there is data already stored in the IDB backend,
      // there is no "going back": any data that has not been migrated will be still on disk
      // but it is not going to be migrated anymore, it could be eventually used to allow
      // a user to manually retrieve the old data file).
      return;
    }

    // Migrate any data stored in the JSONFile backend (if any), and remove the old data file
    // if the migration has been completed successfully.
    oldStoragePath = ExtensionStorage.getStorageFile(extension.id);
    oldStorageExists = await OS.File.exists(oldStoragePath);

    if (oldStorageExists) {
      Services.console.logStringMessage(
        `Migrating storage.local data for ${extension.policy.debugName}...`);

      jsonFile = await ExtensionStorage.getFile(extension.id);
      const data = {};
      for (let [key, value] of jsonFile.data.entries()) {
        data[key] = value;
      }
      oldDataRead = true;
      await idbConn.set(data);
      migrated = true;
      Services.console.logStringMessage(
        `storage.local data successfully migrated to IDB Backend for ${extension.policy.debugName}.`);
    }
  } catch (err) {
    extension.logWarning(`Error on migrating storage.local data: ${err.message}::${err.stack}`);
    if (oldDataRead) {
      // If the data has been read successfully and it has been failed to be stored
      // into the IndexedDB backend, then clear any partially stored data and reject
      // the data migration promise explicitly (which would prevent the new backend
      // from being enabled for this session).
      Services.qms.clearStoragesForPrincipal(storagePrincipal);

      histogram.add("failure");

      throw err;
    }
  } finally {
    // Finalize the jsonFile and clear the jsonFilePromise cached by the ExtensionStorage
    // (so that the file can be immediatelly removed when we call OS.File.remove).
    ExtensionStorage.clearCachedFile(extension.id);
    if (jsonFile) {
      jsonFile.finalize();
    }
  }

  histogram.add("success");

  // If the IDB backend has been enabled, try to remove the old storage.local data file,
  // but keep using the selected backend even if it fails to be removed.
  if (oldStorageExists && migrated) {
    try {
      await OS.File.remove(oldStoragePath);
    } catch (err) {
      extension.logWarning(err.message);
    }
  }
}

/**
 * This ExtensionStorage class implements a backend for the storage.local API which
 * uses IndexedDB to store the data.
 */
this.ExtensionStorageIDB = {
  BACKEND_ENABLED_PREF,
  IDB_MIGRATE_RESULT_HISTOGRAM,

  // Map<extension-id, Set<Function>>
  listeners: new Map(),

  // Keep track if the IDB backend has been selected or not for a running extension
  // (the selected backend should never change while the extension is running, even if the
  // related preference has been changed in the meantime):
  //
  //   WeakMap<extension -> Promise<boolean>
  selectedBackendPromises: new WeakMap(),

  init() {
    XPCOMUtils.defineLazyPreferenceGetter(this, "isBackendEnabled", BACKEND_ENABLED_PREF, false);
  },

  getStoragePrincipal(extension) {
    return extension.createPrincipal(extension.baseURI, {
      userContextId: WEBEXT_STORAGE_USER_CONTEXT_ID,
    });
  },

  /**
   * Select the preferred backend and return a promise which is resolved once the
   * selected backend is ready to be used (e.g. if the extension is switching from
   * the old JSONFile storage to the new IDB backend, any previously stored data will
   * be migrated to the backend before the promise is resolved).
   *
   * This method is called from both the main and child (content or extension) processes:
   * - an extension child context will call this method lazily, when the browser.storage.local
   *   is being used for the first time, and it will result into asking the main process
   *   to call the same method in the main process
   * - on the main process side, it will check if the new IDB backend can be used (and if it can,
   *   it will migrate any existing data into the new backend, which needs to happen in the
   *   main process where the file can directly be accessed)
   *
   * The result will be cached while the extension is still running, and so an extension
   * child context is going to ask the main process only once per child process, and on the
   * main process side the backend selection and data migration will happen only once.
   *
   * @param {BaseContext} context
   *        The extension context that is selecting the storage backend.
   *
   * @returns {Promise<Object>}
   *          Returns a promise which resolves to an object which provides a
   *          `backendEnabled` boolean property, and if it is true the extension should use
   *          the IDB backend and the object also includes a `storagePrincipal` property
   *          of type nsIPrincipal, otherwise `backendEnabled` will be false when the
   *          extension should use the old JSONFile backend (e.g. because the IDB backend has
   *          not been enabled from the preference).
   */
  selectBackend(context) {
    const {extension} = context;

    if (!this.selectedBackendPromises.has(extension)) {
      let promise;

      if (context.childManager) {
        return context.childManager.callParentAsyncFunction(
          "storage.local.IDBBackend.selectBackend", []
        ).then(parentResult => {
          let result;

          if (!parentResult.backendEnabled) {
            result = {backendEnabled: false};
          } else {
            result = {
              ...parentResult,
              // In the child process, we need to deserialize the storagePrincipal
              // from the StructuredCloneHolder used to send it across the processes.
              storagePrincipal: parentResult.storagePrincipal.deserialize(this),
            };
          }

          // Cache the result once we know that it has been resolved. The promise returned by
          // context.childManager.callParentAsyncFunction will be dead when context.cloneScope
          // is destroyed. To keep a promise alive in the cache, we wrap the result in an
          // independent promise.
          this.selectedBackendPromises.set(extension, Promise.resolve(result));

          return result;
        });
      }

      // If migrating to the IDB backend is not enabled by the preference, then we
      // don't need to migrate any data and the new backend is not enabled.
      if (!this.isBackendEnabled) {
        promise = Promise.resolve({backendEnabled: false});
      } else {
        // In the main process, lazily create a storagePrincipal isolated in a
        // reserved user context id (its purpose is ensuring that the IndexedDB storage used
        // by the browser.storage.local API is not directly accessible from the extension code).
        const storagePrincipal = this.getStoragePrincipal(extension);

        // Serialize the nsIPrincipal object into a StructuredCloneHolder related to the privileged
        // js global, ready to be sent to the child processes.
        const serializedPrincipal = new StructuredCloneHolder(storagePrincipal, this);

        promise = migrateJSONFileData(extension, storagePrincipal).then(() => {
          return {backendEnabled: true, storagePrincipal: serializedPrincipal};
        }).catch(err => {
          // If the data migration promise is rejected, the old data has been read
          // successfully from the old JSONFile backend but it failed to be saved
          // into the IndexedDB backend (which is likely unrelated to the kind of
          // data stored and more likely a general issue with the IndexedDB backend)
          // In this case we keep the JSONFile backend enabled for this session
          // and we will retry to migrate to the IDB Backend the next time the
          // extension is being started.
          // TODO Bug 1465129: This should be a very unlikely scenario, some telemetry
          // data about it may be useful.
          extension.logWarning("JSONFile backend is being kept enabled by an unexpected " +
                               `IDBBackend failure: ${err.message}::${err.stack}`);
          return {backendEnabled: false};
        });
      }

      this.selectedBackendPromises.set(extension, promise);
    }

    return this.selectedBackendPromises.get(extension);
  },

  /**
   * Open a connection to the IDB storage.local db for a given extension.
   * given extension.
   *
   * @param {nsIPrincipal} storagePrincipal
   *        The "internally reserved" extension storagePrincipal to be used to create
   *        the ExtensionStorageLocalIDB instance.
   *
   * @returns {Promise<ExtensionStorageLocalIDB>}
   *          Return a promise which resolves to the opened IDB connection.
   */
  open(storagePrincipal) {
    return ExtensionStorageLocalIDB.openForPrincipal(storagePrincipal);
  },

  addOnChangedListener(extensionId, listener) {
    let listeners = this.listeners.get(extensionId) || new Set();
    listeners.add(listener);
    this.listeners.set(extensionId, listeners);
  },

  removeOnChangedListener(extensionId, listener) {
    let listeners = this.listeners.get(extensionId);
    listeners.delete(listener);
  },

  notifyListeners(extensionId, changes) {
    let listeners = this.listeners.get(extensionId);
    if (listeners) {
      for (let listener of listeners) {
        listener(changes);
      }
    }
  },

  hasListeners(extensionId) {
    let listeners = this.listeners.get(extensionId);
    return listeners && listeners.size > 0;
  },
};

ExtensionStorageIDB.init();
