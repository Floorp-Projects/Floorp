"use strict";

Cu.import("resource://gre/modules/ExtensionCommon.jsm");

// These are defined on "global" which is used for the same scopes as the other
// ext-c-*.js files.
/* exported EventManager */
/* global EventManager: false */

global.EventManager = ExtensionCommon.EventManager;

global.initializeBackgroundPage = (contentWindow) => {
  // Override the `alert()` method inside background windows;
  // we alias it to console.log().
  // See: https://bugzilla.mozilla.org/show_bug.cgi?id=1203394
  let alertDisplayedWarning = false;
  let alertOverwrite = text => {
    if (!alertDisplayedWarning) {
      require("devtools/client/framework/devtools-browser");

      let hudservice = require("devtools/client/webconsole/hudservice");
      hudservice.openBrowserConsoleOrFocus();

      contentWindow.console.warn("alert() is not supported in background windows; please use console.log instead.");

      alertDisplayedWarning = true;
    }

    contentWindow.console.log(text);
  };
  Cu.exportFunction(alertOverwrite, contentWindow, {defineAs: "alert"});
};

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
  downloads: {
    url: "chrome://extensions/content/ext-c-downloads.js",
    scopes: ["addon_child"],
    paths: [
      ["downloads"],
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
  permissions: {
    url: "chrome://extensions/content/ext-c-permissions.js",
    scopes: ["addon_child", "content_child", "devtools_child", "proxy_script"],
    paths: [
      ["permissions"],
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
