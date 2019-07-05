"use strict";

extensions.registerModules({
  tabs: {
    url: "chrome://geckoview/content/ext-c-tabs.js",
    scopes: ["addon_child"],
    paths: [["tabs"]],
  },
});
