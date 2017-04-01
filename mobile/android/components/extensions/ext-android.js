"use strict";

extensions.registerModules({
  pageAction: {
    url: "chrome://browser/content/ext-pageAction.js",
    schema: "chrome://browser/content/schemas/page_action.json",
    scopes: ["addon_parent"],
    manifest: ["page_action"],
    paths: [
      ["pageAction"],
    ],
  },
  tabs: {
    url: "chrome://browser/content/ext-tabs.js",
    schema: "chrome://browser/content/schemas/tabs.json",
    scopes: ["addon_parent"],
    paths: [
      ["tabs"],
    ],
  },
});

