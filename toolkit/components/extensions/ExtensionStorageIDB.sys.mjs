/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { IndexedDB } from "resource://gre/modules/IndexedDB.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ExtensionStorage: "resource://gre/modules/ExtensionStorage.sys.mjs",
  ExtensionUtils: "resource://gre/modules/ExtensionUtils.sys.mjs",
  getTrimmedString: "resource://gre/modules/ExtensionTelemetry.sys.mjs",
});

// The userContextID reserved for the extension storage (its purpose is ensuring that the IndexedDB
// storage used by the browser.storage.local API is not directly accessible from the extension code,
// it is defined and reserved as "userContextIdInternal.webextStorageLocal" in ContextualIdentityService.sys.mjs).
const WEBEXT_STORAGE_USER_CONTEXT_ID = -1 >>> 0;

const IDB_NAME = "webExtensions-storage-local";
const IDB_DATA_STORENAME = "storage-local-data";
const IDB_VERSION = 1;
const IDB_MIGRATE_RESULT_HISTOGRAM =
  "WEBEXT_STORAGE_LOCAL_IDB_MIGRATE_RESULT_COUNT";

// Whether or not the installed extensions should be migrated to the storage.local IndexedDB backend.
const BACKEND_ENABLED_PREF =
  "extensions.webextensions.ExtensionStorageIDB.enabled";
const IDB_MIGRATED_PREF_BRANCH =
  "extensions.webextensions.ExtensionStorageIDB.migrated";

class DataMigrationAbortedError extends Error {
  get name() {
    return "DataMigrationAbortedError";
  }
}

var ErrorsTelemetry = {
  initialized: false,

  lazyInit() {
    if (this.initialized) {
      return;
    }
    this.initialized = true;

    // Ensure that these telemetry events category is enabled.
    Services.telemetry.setEventRecordingEnabled("extensions.data", true);

    this.resultHistogram = Services.telemetry.getHistogramById(
      IDB_MIGRATE_RESULT_HISTOGRAM
    );
  },

  /**
   * Get the DOMException error name for a given error object.
   *
   * @param {Error | undefined} error
   *        The Error object to convert into a string, or undefined if there was no error.
   *
   * @returns {string | undefined}
   *          The DOMException error name (sliced to a maximum of 80 chars),
   *          "OtherError" if the error object is not a DOMException instance,
   *          or `undefined` if there wasn't an error.
   */
  getErrorName(error) {
    if (!error) {
      return undefined;
    }

    if (
      DOMException.isInstance(error) ||
      error instanceof DataMigrationAbortedError
    ) {
      if (error.name.length > 80) {
        return lazy.getTrimmedString(error.name);
      }

      return error.name;
    }

    return "OtherError";
  },

  /**
   * Record telemetry related to a data migration result.
   *
   * @param {object} telemetryData
   * @param {string} telemetryData.backend
   *        The backend selected ("JSONFile" or "IndexedDB").
   * @param {boolean} [telemetryData.dataMigrated]
   *        Old extension data has been migrated successfully.
   * @param {string} telemetryData.extensionId
   *        The id of the extension migrated.
   * @param {Error | undefined} telemetryData.error
   *        The error raised during the data migration, if any.
   * @param {boolean} [telemetryData.hasJSONFile]
   *        The extension has an existing JSONFile to migrate.
   * @param {boolean} [telemetryData.hasOldData]
   *        The extension's JSONFile wasn't empty.
   * @param {string} telemetryData.histogramCategory
   *        The histogram category for the result ("success" or "failure").
   */
  recordDataMigrationResult(telemetryData) {
    try {
      const {
        backend,
        dataMigrated,
        extensionId,
        error,
        hasJSONFile,
        hasOldData,
        histogramCategory,
      } = telemetryData;

      this.lazyInit();
      this.resultHistogram.add(histogramCategory);

      const extra = { backend };

      if (dataMigrated != null) {
        extra.data_migrated = dataMigrated ? "y" : "n";
      }

      if (hasJSONFile != null) {
        extra.has_jsonfile = hasJSONFile ? "y" : "n";
      }

      if (hasOldData != null) {
        extra.has_olddata = hasOldData ? "y" : "n";
      }

      if (error) {
        extra.error_name = this.getErrorName(error);
      }

      let addon_id = lazy.getTrimmedString(extensionId);
      Services.telemetry.recordEvent(
        "extensions.data",
        "migrateResult",
        "storageLocal",
        addon_id,
        extra
      );
      Glean.extensionsData.migrateResult.record({
        addon_id,
        backend: extra.backend,
        data_migrated: extra.data_migrated,
        has_jsonfile: extra.has_jsonfile,
        has_olddata: extra.has_olddata,
        error_name: extra.error_name,
      });
    } catch (err) {
      // Report any telemetry error on the browser console, but
      // we treat it as a non-fatal error and we don't re-throw
      // it to the caller.
      Cu.reportError(err);
    }
  },

  /**
   * Record telemetry related to the unexpected errors raised while executing
   * a storage.local API call.
   *
   * @param {object} options
   * @param {string} options.extensionId
   *        The id of the extension migrated.
   * @param {string} options.storageMethod
   *        The storage.local API method being run.
   * @param {Error}  options.error
   *        The unexpected error raised during the API call.
   */
  recordStorageLocalError({ extensionId, storageMethod, error }) {
    this.lazyInit();
    let addon_id = lazy.getTrimmedString(extensionId);
    let error_name = this.getErrorName(error);

    Services.telemetry.recordEvent(
      "extensions.data",
      "storageLocalError",
      storageMethod,
      addon_id,
      { error_name }
    );
    Glean.extensionsData.storageLocalError.record({
      addon_id,
      method: storageMethod,
      error_name,
    });
  },
};

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
    const cursor = await this.objectStore(
      IDB_DATA_STORENAME,
      "readonly"
    ).openKeyCursor();
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
   * @param {callback} [options.serialize]
   *        Set to a function which will be used to serialize the values into
   *        a StructuredCloneHolder object (if appropriate) and being sent
   *        across the processes (it is also used to detect data cloning errors
   *        and raise an appropriate error to the caller).
   *
   * @returns {Promise<null|object>}
   *        Return a promise which resolves to the computed "changes" object
   *        or null.
   */
  async set(items, { serialize } = {}) {
    const changes = {};
    let changed = false;

    // Explicitly create a transaction, so that we can explicitly abort it
    // as soon as one of the put requests fails.
    const transaction = this.transaction(IDB_DATA_STORENAME, "readwrite");
    const objectStore = transaction.objectStore(IDB_DATA_STORENAME);
    const transactionCompleted = transaction.promiseComplete();

    if (!serialize) {
      serialize = (name, anonymizedName, value) => value;
    }

    for (let key of Object.keys(items)) {
      try {
        let oldValue = await objectStore.get(key);

        await objectStore.put(items[key], key);

        changes[key] = {
          oldValue:
            oldValue && serialize(`old/${key}`, `old/<anonymized>`, oldValue),
          newValue: serialize(`new/${key}`, `new/<anonymized>`, items[key]),
        };
        changed = true;
      } catch (err) {
        transactionCompleted.catch(err => {
          // We ignore this rejection because we are explicitly aborting the transaction,
          // the transaction.error will be null, and we throw the original error below.
        });
        transaction.abort();

        throw err;
      }
    }

    await transactionCompleted;

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

    if (typeof keysOrItems === "string") {
      keys = [keysOrItems];
    } else if (Array.isArray(keysOrItems)) {
      keys = keysOrItems;
    } else if (keysOrItems && typeof keysOrItems === "object") {
      keys = Object.keys(keysOrItems);
      defaultValues = keysOrItems;
    }

    const result = {};

    // Retrieve all the stored data using a cursor when browser.storage.local.get()
    // has been called with no keys.
    if (keys == null) {
      const cursor = await this.objectStore(
        IDB_DATA_STORENAME,
        "readonly"
      ).openCursor();
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
   * @returns {Promise<object>}
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

    let promises = [];

    for (let key of keys) {
      promises.push(
        objectStore.getKey(key).then(async foundKey => {
          if (foundKey === key) {
            changed = true;
            changes[key] = { oldValue: await objectStore.get(key) };
            return objectStore.delete(key);
          }
        })
      );
    }

    await Promise.all(promises);

    return changed ? changes : null;
  }

  /**
   * Asynchronously clears all storage entries.
   *
   * @returns {Promise<object>}
   *          Returns an object which contains applied changes.
   */
  async clear() {
    const changes = {};
    let changed = false;

    const objectStore = this.objectStore(IDB_DATA_STORENAME, "readwrite");

    const cursor = await objectStore.openCursor();
    while (!cursor.done) {
      changes[cursor.key] = { oldValue: cursor.value };
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
  let nonFatalError;
  let dataMigrateCompleted = false;
  let hasOldData = false;

  function abortIfShuttingDown() {
    if (extension.hasShutdown || Services.startup.shuttingDown) {
      throw new DataMigrationAbortedError("extension or app is shutting down");
    }
  }

  if (ExtensionStorageIDB.isMigratedExtension(extension)) {
    return;
  }

  try {
    abortIfShuttingDown();
    idbConn = await ExtensionStorageIDB.open(
      storagePrincipal,
      extension.hasPermission("unlimitedStorage")
    );
    abortIfShuttingDown();

    hasEmptyIDB = await idbConn.isEmpty();

    if (!hasEmptyIDB) {
      // If the IDB backend is enabled and there is data already stored in the IDB backend,
      // there is no "going back": any data that has not been migrated will be still on disk
      // but it is not going to be migrated anymore, it could be eventually used to allow
      // a user to manually retrieve the old data file).
      ExtensionStorageIDB.setMigratedExtensionPref(extension, true);
      return;
    }
  } catch (err) {
    extension.logWarning(
      `storage.local data migration cancelled, unable to open IDB connection: ${err.message}::${err.stack}`
    );

    ErrorsTelemetry.recordDataMigrationResult({
      backend: "JSONFile",
      extensionId: extension.id,
      error: err,
      histogramCategory: "failure",
    });

    throw err;
  }

  try {
    abortIfShuttingDown();

    oldStoragePath = lazy.ExtensionStorage.getStorageFile(extension.id);
    oldStorageExists = await IOUtils.exists(oldStoragePath).catch(fileErr => {
      // If we can't access the oldStoragePath here, then extension is also going to be unable to
      // access it, and so we log the error but we don't stop the extension from switching to
      // the IndexedDB backend.
      extension.logWarning(
        `Unable to access extension storage.local data file: ${fileErr.message}::${fileErr.stack}`
      );
      return false;
    });

    // Migrate any data stored in the JSONFile backend (if any), and remove the old data file
    // if the migration has been completed successfully.
    if (oldStorageExists) {
      // Do not load the old JSON file content if shutting down is already in progress.
      abortIfShuttingDown();

      Services.console.logStringMessage(
        `Migrating storage.local data for ${extension.policy.debugName}...`
      );

      jsonFile = await lazy.ExtensionStorage.getFile(extension.id);

      abortIfShuttingDown();

      const data = {};
      for (let [key, value] of jsonFile.data.entries()) {
        data[key] = value;
        hasOldData = true;
      }

      await idbConn.set(data);
      Services.console.logStringMessage(
        `storage.local data successfully migrated to IDB Backend for ${extension.policy.debugName}.`
      );
    }

    dataMigrateCompleted = true;
  } catch (err) {
    extension.logWarning(
      `Error on migrating storage.local data file: ${err.message}::${err.stack}`
    );

    if (oldStorageExists && !dataMigrateCompleted) {
      ErrorsTelemetry.recordDataMigrationResult({
        backend: "JSONFile",
        dataMigrated: dataMigrateCompleted,
        extensionId: extension.id,
        error: err,
        hasJSONFile: oldStorageExists,
        hasOldData,
        histogramCategory: "failure",
      });

      // If the data failed to be stored into the IndexedDB backend, then we clear the IndexedDB
      // backend to allow the extension to retry the migration on its next startup, and reject
      // the data migration promise explicitly (which would prevent the new backend
      // from being enabled for this session).
      await new Promise(resolve => {
        let req = Services.qms.clearStoragesForPrincipal(storagePrincipal);
        req.callback = resolve;
      });

      throw err;
    }

    // This error is not preventing the extension from switching to the IndexedDB backend,
    // but we may still want to know that it has been triggered and include it into the
    // telemetry data collected for the extension.
    nonFatalError = err;
  } finally {
    // Clear the jsonFilePromise cached by the ExtensionStorage.
    await lazy.ExtensionStorage.clearCachedFile(extension.id).catch(err => {
      extension.logWarning(err.message);
    });
  }

  // If the IDB backend has been enabled, rename the old storage.local data file, but
  // do not prevent the extension from switching to the IndexedDB backend if it fails.
  if (oldStorageExists && dataMigrateCompleted) {
    try {
      // Only migrate the file when it actually exists (e.g. the file name is not going to exist
      // when it is corrupted, because JSONFile internally rename it to `.corrupt`.
      if (await IOUtils.exists(oldStoragePath)) {
        const uniquePath = await IOUtils.createUniqueFile(
          PathUtils.parent(oldStoragePath),
          `${PathUtils.filename(oldStoragePath)}.migrated`
        );
        await IOUtils.move(oldStoragePath, uniquePath);
      }
    } catch (err) {
      nonFatalError = err;
      extension.logWarning(err.message);
    }
  }

  ExtensionStorageIDB.setMigratedExtensionPref(extension, true);

  ErrorsTelemetry.recordDataMigrationResult({
    backend: "IndexedDB",
    dataMigrated: dataMigrateCompleted,
    extensionId: extension.id,
    error: nonFatalError,
    hasJSONFile: oldStorageExists,
    hasOldData,
    histogramCategory: "success",
  });
}

/**
 * This ExtensionStorage class implements a backend for the storage.local API which
 * uses IndexedDB to store the data.
 */
export var ExtensionStorageIDB = {
  BACKEND_ENABLED_PREF,
  IDB_MIGRATED_PREF_BRANCH,
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
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "isBackendEnabled",
      BACKEND_ENABLED_PREF,
      false
    );
  },

  isMigratedExtension(extension) {
    return Services.prefs.getBoolPref(
      `${IDB_MIGRATED_PREF_BRANCH}.${extension.id}`,
      false
    );
  },

  setMigratedExtensionPref(extension, val) {
    Services.prefs.setBoolPref(
      `${IDB_MIGRATED_PREF_BRANCH}.${extension.id}`,
      !!val
    );
  },

  clearMigratedExtensionPref(extensionId) {
    Services.prefs.clearUserPref(`${IDB_MIGRATED_PREF_BRANCH}.${extensionId}`);
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
   * @param {import("ExtensionPageChild.sys.mjs").ExtensionBaseContextChild} context
   *        The extension context that is selecting the storage backend.
   *
   * @returns {Promise<object>}
   *          Returns a promise which resolves to an object which provides a
   *          `backendEnabled` boolean property, and if it is true the extension should use
   *          the IDB backend and the object also includes a `storagePrincipal` property
   *          of type nsIPrincipal, otherwise `backendEnabled` will be false when the
   *          extension should use the old JSONFile backend (e.g. because the IDB backend has
   *          not been enabled from the preference).
   */
  selectBackend(context) {
    const { extension } = context;

    if (!this.selectedBackendPromises.has(extension)) {
      let promise;

      if (context.childManager) {
        return context.childManager
          .callParentAsyncFunction("storage.local.IDBBackend.selectBackend", [])
          .then(parentResult => {
            let result;

            if (!parentResult.backendEnabled) {
              result = { backendEnabled: false };
            } else {
              result = {
                ...parentResult,
                // In the child process, we need to deserialize the storagePrincipal
                // from the StructuredCloneHolder used to send it across the processes.
                storagePrincipal: parentResult.storagePrincipal.deserialize(
                  this,
                  true
                ),
              };
            }

            // Cache the result once we know that it has been resolved. The promise returned by
            // context.childManager.callParentAsyncFunction will be dead when context.cloneScope
            // is destroyed. To keep a promise alive in the cache, we wrap the result in an
            // independent promise.
            this.selectedBackendPromises.set(
              extension,
              Promise.resolve(result)
            );

            return result;
          });
      }

      // If migrating to the IDB backend is not enabled by the preference, then we
      // don't need to migrate any data and the new backend is not enabled.
      if (!this.isBackendEnabled) {
        promise = Promise.resolve({ backendEnabled: false });
      } else {
        // In the main process, lazily create a storagePrincipal isolated in a
        // reserved user context id (its purpose is ensuring that the IndexedDB storage used
        // by the browser.storage.local API is not directly accessible from the extension code).
        const storagePrincipal = this.getStoragePrincipal(extension);

        // Serialize the nsIPrincipal object into a StructuredCloneHolder related to the privileged
        // js global, ready to be sent to the child processes.
        const serializedPrincipal = new StructuredCloneHolder(
          "ExtensionStorageIDB/selectBackend/serializedPrincipal",
          null,
          storagePrincipal,
          this
        );

        promise = migrateJSONFileData(extension, storagePrincipal)
          .then(() => {
            extension.setSharedData("storageIDBBackend", true);
            extension.setSharedData("storageIDBPrincipal", storagePrincipal);
            Services.ppmm.sharedData.flush();
            return {
              backendEnabled: true,
              storagePrincipal: serializedPrincipal,
            };
          })
          .catch(err => {
            // If the data migration promise is rejected, the old data has been read
            // successfully from the old JSONFile backend but it failed to be saved
            // into the IndexedDB backend (which is likely unrelated to the kind of
            // data stored and more likely a general issue with the IndexedDB backend)
            // In this case we keep the JSONFile backend enabled for this session
            // and we will retry to migrate to the IDB Backend the next time the
            // extension is being started.
            // TODO Bug 1465129: This should be a very unlikely scenario, some telemetry
            // data about it may be useful.
            extension.logWarning(
              "JSONFile backend is being kept enabled by an unexpected " +
                `IDBBackend failure: ${err.message}::${err.stack}`
            );
            extension.setSharedData("storageIDBBackend", false);
            Services.ppmm.sharedData.flush();

            return { backendEnabled: false };
          });
      }

      this.selectedBackendPromises.set(extension, promise);
    }

    return this.selectedBackendPromises.get(extension);
  },

  persist(storagePrincipal) {
    return new Promise((resolve, reject) => {
      const request = Services.qms.persist(storagePrincipal);
      request.callback = () => {
        if (request.resultCode === Cr.NS_OK) {
          resolve();
        } else {
          reject(
            new Error(
              `Failed to persist storage for principal: ${storagePrincipal.originNoSuffix}`
            )
          );
        }
      };
    });
  },

  /**
   * Open a connection to the IDB storage.local db for a given extension.
   * given extension.
   *
   * @param {nsIPrincipal} storagePrincipal
   *        The "internally reserved" extension storagePrincipal to be used to create
   *        the ExtensionStorageLocalIDB instance.
   * @param {boolean} persisted
   *        A boolean which indicates if the storage should be set into persistent mode.
   *
   * @returns {Promise<ExtensionStorageLocalIDB>}
   *          Return a promise which resolves to the opened IDB connection.
   */
  open(storagePrincipal, persisted) {
    if (!storagePrincipal) {
      return Promise.reject(new Error("Unexpected empty principal"));
    }
    let setPersistentMode = persisted
      ? this.persist(storagePrincipal)
      : Promise.resolve();
    return setPersistentMode.then(() =>
      ExtensionStorageLocalIDB.openForPrincipal(storagePrincipal)
    );
  },

  /**
   * Ensure that an error originated from the ExtensionStorageIDB methods is normalized
   * into an ExtensionError (e.g. DataCloneError and QuotaExceededError instances raised
   * from the internal IndexedDB operations have to be converted into an ExtensionError
   * to be accessible to the extension code).
   *
   * @param {object} params
   * @param {Error|ExtensionError|DOMException} params.error
   *        The error object to normalize.
   * @param {string} params.extensionId
   *        The id of the extension that was executing the storage.local method.
   * @param {string} params.storageMethod
   *        The storage method being executed when the error has been thrown
   *        (used to keep track of the unexpected error incidence in telemetry).
   *
   * @returns {ExtensionError}
   *          Return an ExtensionError error instance.
   */
  normalizeStorageError({ error, extensionId, storageMethod }) {
    const { ExtensionError } = lazy.ExtensionUtils;

    if (error instanceof ExtensionError) {
      // @ts-ignore (will go away after `lazy` is properly typed)
      return error;
    }

    let errorMessage;

    if (DOMException.isInstance(error)) {
      switch (error.name) {
        case "DataCloneError":
          errorMessage = String(error);
          break;
        case "QuotaExceededError":
          errorMessage = `${error.name}: storage.local API call exceeded its quota limitations.`;
          break;
      }
    }

    if (!errorMessage) {
      Cu.reportError(error);

      errorMessage = "An unexpected error occurred";

      ErrorsTelemetry.recordStorageLocalError({
        error,
        extensionId,
        storageMethod,
      });
    }

    return new ExtensionError(errorMessage);
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
