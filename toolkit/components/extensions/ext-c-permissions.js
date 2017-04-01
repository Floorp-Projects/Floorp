"use strict";

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
const {ExtensionError} = ExtensionUtils;

extensions.registerSchemaAPI("permissions", "addon_child", context => {
  return {
    permissions: {
      async request(perms) {
        let winUtils = context.contentWindow.getInterface(Ci.nsIDOMWindowUtils);
        if (!winUtils.isHandlingUserInput) {
          throw new ExtensionError("May only request permissions from a user input handler");
        }

        return context.childManager.callParentAsyncFunction("permissions.request_parent", [perms]);
      },
    },
  };
});
