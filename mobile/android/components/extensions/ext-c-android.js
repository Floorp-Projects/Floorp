"use strict";

extensions.registerModules({
  tabs: {
    url: "chrome://browser/content/ext-c-tabs.js",
    scopes: ["addon_child"],
    paths: [
      ["tabs"],
    ],
  },
});
