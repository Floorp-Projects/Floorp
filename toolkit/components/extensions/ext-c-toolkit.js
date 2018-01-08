"use strict";

Cu.import("resource://gre/modules/ExtensionCommon.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

// These are defined on "global" which is used for the same scopes as the other
// ext-c-*.js files.
/* exported EventManager */
/* global EventManager: false */

global.EventManager = ExtensionCommon.EventManager;

extensions.registerModules({
  backgroundPage: {
    url: "chrome://extensions/content/ext-c-backgroundPage.js",
    scopes: ["addon_child"],
    manifest: ["background"],
    paths: [
      ["extension", "getBackgroundPage"],
      ["runtime", "getBackgroundPage"],
    ],
  },
  contentScripts: {
    url: "chrome://extensions/content/ext-c-contentScripts.js",
    scopes: ["addon_child"],
    paths: [
      ["contentScripts"],
    ],
  },
  extension: {
    url: "chrome://extensions/content/ext-c-extension.js",
    scopes: ["addon_child", "content_child", "devtools_child", "proxy_script"],
    paths: [
      ["extension"],
    ],
  },
  i18n: {
    url: "chrome://extensions/content/ext-i18n.js",
    scopes: ["addon_child", "content_child", "devtools_child", "proxy_script"],
    paths: [
      ["i18n"],
    ],
  },
  runtime: {
    url: "chrome://extensions/content/ext-c-runtime.js",
    scopes: ["addon_child", "content_child", "devtools_child", "proxy_script"],
    paths: [
      ["runtime"],
    ],
  },
  storage: {
    url: "chrome://extensions/content/ext-c-storage.js",
    scopes: ["addon_child", "content_child", "devtools_child", "proxy_script"],
    paths: [
      ["storage"],
    ],
  },
  test: {
    url: "chrome://extensions/content/ext-c-test.js",
    scopes: ["addon_child", "content_child", "devtools_child", "proxy_script"],
    paths: [
      ["test"],
    ],
  },
  webRequest: {
    url: "chrome://extensions/content/ext-c-webRequest.js",
    scopes: ["addon_child"],
    paths: [
      ["webRequest"],
    ],
  },
});

if (AppConstants.MOZ_BUILD_APP === "browser") {
  extensions.registerModules({
    identity: {
      url: "chrome://extensions/content/ext-c-identity.js",
      scopes: ["addon_child"],
      paths: [
        ["identity"],
      ],
    },
  });
}
