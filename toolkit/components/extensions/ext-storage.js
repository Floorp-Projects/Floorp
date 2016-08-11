"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionStorage",
                                  "resource://gre/modules/ExtensionStorage.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionStorageSync",
                                  "resource://gre/modules/ExtensionStorageSync.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  EventManager,
} = ExtensionUtils;

function storageApiFactory(context) {
  let {extension} = context;
  return {
    storage: {
      local: {
        get: function(spec) {
          return ExtensionStorage.get(extension.id, spec);
        },
        set: function(items) {
          return ExtensionStorage.set(extension.id, items, context);
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
          return ExtensionStorageSync.get(extension, spec, context);
        },
        set: function(items) {
          return ExtensionStorageSync.set(extension, items, context);
        },
        remove: function(keys) {
          return ExtensionStorageSync.remove(extension, keys, context);
        },
        clear: function() {
          return ExtensionStorageSync.clear(extension, context);
        },
      },

      onChanged: new EventManager(context, "storage.onChanged", fire => {
        let listenerLocal = changes => {
          fire(changes, "local");
        };
        let listenerSync = changes => {
          fire(changes, "sync");
        };

        ExtensionStorage.addOnChangedListener(extension.id, listenerLocal);
        ExtensionStorageSync.addOnChangedListener(extension, listenerSync);
        return () => {
          ExtensionStorage.removeOnChangedListener(extension.id, listenerLocal);
          ExtensionStorageSync.removeOnChangedListener(extension, listenerSync);
        };
      }).api(),
    },
  };
}
extensions.registerSchemaAPI("storage", "addon_parent", storageApiFactory);
extensions.registerSchemaAPI("storage", "content_parent", storageApiFactory);
