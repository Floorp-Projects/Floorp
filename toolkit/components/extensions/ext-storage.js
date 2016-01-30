"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionStorage",
                                  "resource://gre/modules/ExtensionStorage.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  EventManager,
} = ExtensionUtils;

extensions.registerPrivilegedAPI("storage", (extension, context) => {
  return {
    storage: {
      local: {
        get: function(keys, callback) {
          return context.wrapPromise(
            ExtensionStorage.get(extension.id, keys), callback);
        },
        set: function(items, callback) {
          return context.wrapPromise(
            ExtensionStorage.set(extension.id, items), callback);
        },
        remove: function(items, callback) {
          return context.wrapPromise(
            ExtensionStorage.remove(extension.id, items), callback);
        },
        clear: function(callback) {
          return context.wrapPromise(
            ExtensionStorage.clear(extension.id), callback);
        },
      },

      onChanged: new EventManager(context, "storage.local.onChanged", fire => {
        let listener = changes => {
          fire(changes, "local");
        };

        ExtensionStorage.addOnChangedListener(extension.id, listener);
        return () => {
          ExtensionStorage.removeOnChangedListener(extension.id, listener);
        };
      }).api(),
    },
  };
});
