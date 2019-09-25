/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { ExtensionCommon } = ChromeUtils.import(
  "resource://gre/modules/ExtensionCommon.jsm"
);

// eslint-disable-next-line no-unused-vars
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

// These are defined on "global" which is used for the same scopes as the other
// ext-c-*.js files.
/* exported EventManager */
/* global EventManager: false */

global.EventManager = ExtensionCommon.EventManager;

extensions.registerModules({
  backgroundPage: {
    url: "chrome://extensions/content/child/ext-backgroundPage.js",
    scopes: ["addon_child"],
    manifest: ["background"],
    paths: [
      ["extension", "getBackgroundPage"],
      ["runtime", "getBackgroundPage"],
    ],
  },
  contentScripts: {
    url: "chrome://extensions/content/child/ext-contentScripts.js",
    scopes: ["addon_child"],
    paths: [["contentScripts"]],
  },
  extension: {
    url: "chrome://extensions/content/child/ext-extension.js",
    scopes: ["addon_child", "content_child", "devtools_child"],
    paths: [["extension"]],
  },
  i18n: {
    url: "chrome://extensions/content/parent/ext-i18n.js",
    scopes: ["addon_child", "content_child", "devtools_child"],
    paths: [["i18n"]],
  },
  runtime: {
    url: "chrome://extensions/content/child/ext-runtime.js",
    scopes: ["addon_child", "content_child", "devtools_child"],
    paths: [["runtime"]],
  },
  storage: {
    url: "chrome://extensions/content/child/ext-storage.js",
    scopes: ["addon_child", "content_child", "devtools_child"],
    paths: [["storage"]],
  },
  test: {
    url: "chrome://extensions/content/child/ext-test.js",
    scopes: ["addon_child", "content_child", "devtools_child"],
    paths: [["test"]],
  },
  userScripts: {
    url: "chrome://extensions/content/child/ext-userScripts.js",
    scopes: ["addon_child"],
    paths: [["userScripts"]],
  },
  userScriptsContent: {
    url: "chrome://extensions/content/child/ext-userScripts-content.js",
    scopes: ["content_child"],
    paths: [["userScripts", "onBeforeScript"]],
  },
  webRequest: {
    url: "chrome://extensions/content/child/ext-webRequest.js",
    scopes: ["addon_child"],
    paths: [["webRequest"]],
  },
});

if (AppConstants.MOZ_BUILD_APP === "browser") {
  extensions.registerModules({
    identity: {
      url: "chrome://extensions/content/child/ext-identity.js",
      scopes: ["addon_child"],
      paths: [["identity"]],
    },
  });
}
