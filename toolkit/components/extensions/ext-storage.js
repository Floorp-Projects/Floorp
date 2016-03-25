"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionStorage",
                                  "resource://gre/modules/ExtensionStorage.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  EventManager,
} = ExtensionUtils;

extensions.registerSchemaAPI("storage", "storage", (extension, context) => {
  return {
    storage: {
      local: {
        get: function(keys) {
          return ExtensionStorage.get(extension.id, keys);
        },
        set: function(items) {
          return ExtensionStorage.set(extension.id, items, context.cloneScope);
        },
        remove: function(items) {
          return ExtensionStorage.remove(extension.id, items);
        },
        clear: function() {
          return ExtensionStorage.clear(extension.id);
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
