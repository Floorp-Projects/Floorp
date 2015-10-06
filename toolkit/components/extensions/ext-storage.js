var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionStorage",
                                  "resource://gre/modules/ExtensionStorage.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  EventManager,
  ignoreEvent,
  runSafe,
} = ExtensionUtils;

extensions.registerPrivilegedAPI("storage", (extension, context) => {
  return {
    storage: {
      local: {
        get: function(keys, callback) {
          ExtensionStorage.get(extension.id, keys).then(result => {
            runSafe(context, callback, result);
          });
        },
        set: function(items, callback) {
          ExtensionStorage.set(extension.id, items).then(() => {
            if (callback) {
              runSafe(context, callback);
            }
          });
        },
        remove: function(items, callback) {
          ExtensionStorage.remove(extension.id, items).then(() => {
            if (callback) {
              runSafe(context, callback);
            }
          });
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
