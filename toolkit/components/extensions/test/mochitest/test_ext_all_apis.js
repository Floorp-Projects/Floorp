/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// Tests whether not too many APIs are visible by default.
// This file is used by test_ext_all_apis.html in browser/ and mobile/android/,
// which may modify the following variables to add or remove expected APIs.
/* globals expectedContentApisTargetSpecific */
/* globals expectedBackgroundApisTargetSpecific */

// Generates a list of expectations.
function generateExpectations(list) {
  return list.reduce((allApis, path) => {
    return allApis.concat(`browser.${path}`, `chrome.${path}`);
  }, []).sort();
}

let expectedCommonApis = [
  "extension.getURL",
  "extension.inIncognitoContext",
  "extension.lastError",
  "i18n.detectLanguage",
  "i18n.getAcceptLanguages",
  "i18n.getMessage",
  "i18n.getUILanguage",
  "runtime.OnInstalledReason",
  "runtime.OnRestartRequiredReason",
  "runtime.PlatformArch",
  "runtime.PlatformOs",
  "runtime.RequestUpdateCheckStatus",
  "runtime.getManifest",
  "runtime.connect",
  "runtime.getURL",
  "runtime.id",
  "runtime.lastError",
  "runtime.onConnect",
  "runtime.onMessage",
  "runtime.sendMessage",
  // If you want to add a new powerful test API, please see bug 1287233.
  "test.assertEq",
  "test.assertFalse",
  "test.assertRejects",
  "test.assertThrows",
  "test.assertTrue",
  "test.fail",
  "test.log",
  "test.notifyFail",
  "test.notifyPass",
  "test.onMessage",
  "test.sendMessage",
  "test.succeed",
];

let expectedContentApis = [
  ...expectedCommonApis,
  ...expectedContentApisTargetSpecific,
];

let expectedBackgroundApis = [
  ...expectedCommonApis,
  ...expectedBackgroundApisTargetSpecific,
  "extension.ViewType",
  "extension.getBackgroundPage",
  "extension.getViews",
  "extension.isAllowedFileSchemeAccess",
  "extension.isAllowedIncognitoAccess",
  // Note: extensionTypes is not visible in Chrome.
  "extensionTypes.ImageFormat",
  "extensionTypes.RunAt",
  "management.ExtensionDisabledReason",
  "management.ExtensionInstallType",
  "management.ExtensionType",
  "management.getSelf",
  "management.uninstallSelf",
  "runtime.getBackgroundPage",
  "runtime.getBrowserInfo",
  "runtime.getPlatformInfo",
  "runtime.onInstalled",
  "runtime.onStartup",
  "runtime.onUpdateAvailable",
  "runtime.openOptionsPage",
  "runtime.reload",
  "runtime.setUninstallURL",
];

function sendAllApis() {
  function isEvent(key, val) {
    if (!/^on[A-Z]/.test(key)) {
      return false;
    }
    let eventKeys = [];
    for (let prop in val) {
      eventKeys.push(prop);
    }
    eventKeys = eventKeys.sort().join();
    return eventKeys === "addListener,hasListener,removeListener";
  }
  function mayRecurse(key, val) {
    if (Object.keys(val).filter(k => !/^[A-Z\-0-9_]+$/.test(k)).length === 0) {
      // Don't recurse on constants and empty objects.
      return false;
    }
    return !isEvent(key, val);
  }

  let results = [];
  function diveDeeper(path, obj) {
    for (let key in obj) {
      let val = obj[key];
      if (typeof val == "object" && val !== null && mayRecurse(key, val)) {
        diveDeeper(`${path}.${key}`, val);
      } else if (val !== undefined) {
        results.push(`${path}.${key}`);
      }
    }
  }
  diveDeeper("browser", browser);
  diveDeeper("chrome", chrome);
  browser.test.sendMessage("allApis", results.sort());
}

add_task(function* test_enumerate_content_script_apis() {
  let extensionData = {
    manifest: {
      content_scripts: [{
        matches: ["http://mochi.test/*/file_sample.html"],
        js: ["contentscript.js"],
        run_at: "document_start",
      }],
    },
    files: {
      "contentscript.js": sendAllApis,
    },
  };
  let extension = ExtensionTestUtils.loadExtension(extensionData);
  yield extension.startup();

  let win = window.open("file_sample.html");
  let actualApis = yield extension.awaitMessage("allApis");
  win.close();
  let expectedApis = generateExpectations(expectedContentApis);
  isDeeply(actualApis, expectedApis, "content script APIs");

  yield extension.unload();
});

add_task(function* test_enumerate_background_script_apis() {
  let extensionData = {
    background: sendAllApis,
  };
  let extension = ExtensionTestUtils.loadExtension(extensionData);
  yield extension.startup();
  let actualApis = yield extension.awaitMessage("allApis");
  let expectedApis = generateExpectations(expectedBackgroundApis);
  isDeeply(actualApis, expectedApis, "background script APIs");

  yield extension.unload();
});
