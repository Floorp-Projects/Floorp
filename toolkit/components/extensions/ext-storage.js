"use strict";

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-toolkit.js */

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManagerPrivate: "resource://gre/modules/AddonManager.jsm",
  ExtensionStorage: "resource://gre/modules/ExtensionStorage.jsm",
  extensionStorageSync: "resource://gre/modules/ExtensionStorageSync.jsm",
  NativeManifests: "resource://gre/modules/NativeManifests.jsm",
});

var {
  ExtensionError,
} = ExtensionUtils;

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
  let info = await NativeManifests.lookupManifest("storage", extensionId, context);
  if (info) {
    return ExtensionStorage._serializableMap(info.manifest.data);
  }
  return null;
};

this.storage = class extends ExtensionAPI {
  getAPI(context) {
    let {extension} = context;
    return {
      storage: {
        local: {
          get: function(spec) {
            return ExtensionStorage.get(extension.id, spec);
          },
          set: function(items) {
            return ExtensionStorage.set(extension.id, items);
          },
          remove: function(keys) {
            return ExtensionStorage.remove(extension.id, keys);
          },
          clear: function() {
            return ExtensionStorage.clear(extension.id);
          },
        },

        sync: {
          get: function(spec) {
            enforceNoTemporaryAddon(extension.id);
            return extensionStorageSync.get(extension, spec, context);
          },
          set: function(items) {
            enforceNoTemporaryAddon(extension.id);
            return extensionStorageSync.set(extension, items, context);
          },
          remove: function(keys) {
            enforceNoTemporaryAddon(extension.id);
            return extensionStorageSync.remove(extension, keys, context);
          },
          clear: function() {
            enforceNoTemporaryAddon(extension.id);
            return extensionStorageSync.clear(extension, context);
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
              return Promise.reject({message: "Managed storage manifest not found"});
            }
            return ExtensionStorage._filterProperties(data, keys);
          },
        },

        onChanged: new EventManager(context, "storage.onChanged", fire => {
          let listenerLocal = changes => {
            fire.raw(changes, "local");
          };
          let listenerSync = changes => {
            fire.async(changes, "sync");
          };

          ExtensionStorage.addOnChangedListener(extension.id, listenerLocal);
          extensionStorageSync.addOnChangedListener(extension, listenerSync, context);
          return () => {
            ExtensionStorage.removeOnChangedListener(extension.id, listenerLocal);
            extensionStorageSync.removeOnChangedListener(extension, listenerSync);
          };
        }).api(),
      },
    };
  }
};
