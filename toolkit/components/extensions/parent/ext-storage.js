/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManagerPrivate: "resource://gre/modules/AddonManager.jsm",
  ExtensionStorage: "resource://gre/modules/ExtensionStorage.jsm",
  ExtensionStorageIDB: "resource://gre/modules/ExtensionStorageIDB.jsm",
  NativeManifests: "resource://gre/modules/NativeManifests.jsm",
});

var { ExtensionError } = ExtensionUtils;

XPCOMUtils.defineLazyGetter(this, "extensionStorageSync", () => {
  let url = Services.prefs.getBoolPref("webextensions.storage.sync.kinto")
    ? "resource://gre/modules/ExtensionStorageSyncKinto.jsm"
    : "resource://gre/modules/ExtensionStorageSync.jsm";

  const { extensionStorageSync } = ChromeUtils.import(url, {});
  return extensionStorageSync;
});

const enforceNoTemporaryAddon = extensionId => {
  const EXCEPTION_MESSAGE =
    "The storage API will not work with a temporary addon ID. " +
    "Please add an explicit addon ID to your manifest. " +
    "For more information see https://bugzil.la/1323228.";
  if (AddonManagerPrivate.isTemporaryInstallID(extensionId)) {
    throw new ExtensionError(EXCEPTION_MESSAGE);
  }
};

// WeakMap[extension -> Promise<SerializableMap?>]
const managedStorage = new WeakMap();

const lookupManagedStorage = async (extensionId, context) => {
  if (Services.policies) {
    let extensionPolicy = Services.policies.getExtensionPolicy(extensionId);
    if (extensionPolicy) {
      return ExtensionStorage._serializableMap(extensionPolicy);
    }
  }
  let info = await NativeManifests.lookupManifest(
    "storage",
    extensionId,
    context
  );
  if (info) {
    return ExtensionStorage._serializableMap(info.manifest.data);
  }
  return null;
};

this.storage = class extends ExtensionAPI {
  constructor(extension) {
    super(extension);

    const messageName = `Extension:StorageLocalOnChanged:${extension.uuid}`;
    Services.ppmm.addMessageListener(messageName, this);
    this.clearStorageChangedListener = () => {
      Services.ppmm.removeMessageListener(messageName, this);
    };
  }

  onShutdown() {
    const { clearStorageChangedListener } = this;
    this.clearStorageChangedListener = null;

    if (clearStorageChangedListener) {
      clearStorageChangedListener();
    }
  }

  receiveMessage({ name, data }) {
    if (name !== `Extension:StorageLocalOnChanged:${this.extension.uuid}`) {
      return;
    }

    ExtensionStorageIDB.notifyListeners(this.extension.id, data);
  }

  getAPI(context) {
    let { extension } = context;

    return {
      storage: {
        local: {
          async callMethodInParentProcess(method, args) {
            const res = await ExtensionStorageIDB.selectBackend({ extension });
            if (!res.backendEnabled) {
              return ExtensionStorage[method](extension.id, ...args);
            }

            const persisted = extension.hasPermission("unlimitedStorage");
            const db = await ExtensionStorageIDB.open(
              res.storagePrincipal.deserialize(this, true),
              persisted
            );
            try {
              const changes = await db[method](...args);
              if (changes) {
                ExtensionStorageIDB.notifyListeners(extension.id, changes);
              }
              return changes;
            } catch (err) {
              const normalizedError = ExtensionStorageIDB.normalizeStorageError(
                {
                  error: err,
                  extensionId: extension.id,
                  storageMethod: method,
                }
              ).message;
              return Promise.reject({
                message: String(normalizedError),
              });
            }
          },
          // Private storage.local JSONFile backend methods (used internally by the child
          // ext-storage.js module).
          JSONFileBackend: {
            get(spec) {
              return ExtensionStorage.get(extension.id, spec);
            },
            set(items) {
              return ExtensionStorage.set(extension.id, items);
            },
            remove(keys) {
              return ExtensionStorage.remove(extension.id, keys);
            },
            clear() {
              return ExtensionStorage.clear(extension.id);
            },
          },
          // Private storage.local IDB backend methods (used internally by the child ext-storage.js
          // module).
          IDBBackend: {
            selectBackend() {
              return ExtensionStorageIDB.selectBackend(context);
            },
          },
        },

        sync: {
          get(spec) {
            enforceNoTemporaryAddon(extension.id);
            return extensionStorageSync.get(extension, spec, context);
          },
          set(items) {
            enforceNoTemporaryAddon(extension.id);
            return extensionStorageSync.set(extension, items, context);
          },
          remove(keys) {
            enforceNoTemporaryAddon(extension.id);
            return extensionStorageSync.remove(extension, keys, context);
          },
          clear() {
            enforceNoTemporaryAddon(extension.id);
            return extensionStorageSync.clear(extension, context);
          },
          getBytesInUse(keys) {
            enforceNoTemporaryAddon(extension.id);
            return extensionStorageSync.getBytesInUse(extension, keys, context);
          },
        },

        managed: {
          async get(keys) {
            enforceNoTemporaryAddon(extension.id);
            let lookup = managedStorage.get(extension);

            if (!lookup) {
              lookup = lookupManagedStorage(extension.id, context);
              managedStorage.set(extension, lookup);
            }

            let data = await lookup;
            if (!data) {
              return Promise.reject({
                message: "Managed storage manifest not found",
              });
            }
            return ExtensionStorage._filterProperties(data, keys);
          },
        },

        onChanged: new EventManager({
          context,
          name: "storage.onChanged",
          register: fire => {
            let listenerLocal = changes => {
              fire.raw(changes, "local");
            };
            let listenerSync = changes => {
              fire.async(changes, "sync");
            };

            ExtensionStorage.addOnChangedListener(extension.id, listenerLocal);
            ExtensionStorageIDB.addOnChangedListener(
              extension.id,
              listenerLocal
            );
            extensionStorageSync.addOnChangedListener(
              extension,
              listenerSync,
              context
            );
            return () => {
              ExtensionStorage.removeOnChangedListener(
                extension.id,
                listenerLocal
              );
              ExtensionStorageIDB.removeOnChangedListener(
                extension.id,
                listenerLocal
              );
              extensionStorageSync.removeOnChangedListener(
                extension,
                listenerSync
              );
            };
          },
        }).api(),
      },
    };
  }
};
