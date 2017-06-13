"use strict";

var {
  ExtensionError,
} = ExtensionUtils;

this.downloads = class extends ExtensionAPI {
  getAPI(context) {
    return {
      downloads: {
        open(downloadId) {
          let winUtils = context.contentWindow.getInterface(Ci.nsIDOMWindowUtils);
          if (!winUtils.isHandlingUserInput) {
            throw new ExtensionError("May only open downloads from a user input handler");
          }

          return context.childManager.callParentAsyncFunction("downloads.open_parent", [downloadId]);
        },
      },
    };
  }
};
