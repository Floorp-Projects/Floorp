"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
const {
  ignoreEvent,
} = ExtensionUtils;

extensions.registerSchemaAPI("downloads", "downloads", (extension, context) => {
  return {
    downloads: {
      // When we do open(), check for additional downloads.open permission.
      // i.e.:
      // open(downloadId) {
      //   if (!extension.hasPermission("downloads.open")) {
      //     throw new context.cloneScope.Error("Permission denied because 'downloads.open' permission is missing.");
      //   }
      //   ...
      // }
      // likewise for setShelfEnabled() and the "download.shelf" permission

      onCreated: ignoreEvent(context, "downloads.onCreated"),
      onErased: ignoreEvent(context, "downloads.onErased"),
      onChanged: ignoreEvent(context, "downloads.onChanged"),
      onDeterminingFilename: ignoreEvent(context, "downloads.onDeterminingFilename"),
    },
  };
});
