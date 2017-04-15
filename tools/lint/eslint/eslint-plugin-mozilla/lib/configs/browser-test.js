// Parent config file for all browser-chrome files.
"use strict";

module.exports = {
  "rules": {
    "mozilla/import-content-task-globals": "error",
    "mozilla/import-headjs-globals": "warn",
    "mozilla/mark-test-function-used": "warn"
  },

  "env": {
    "browser": true,
    "mozilla/browser-window": true,
    "mozilla/simpletest": true
    // "node": true
  },

  "plugins": [
    "mozilla"
  ],

  // All globals made available in the test environment.
  "globals": {
    // `$` is defined in SimpleTest.js
    "$": false,
    "add_task": false,
    "addLoadEvent": false,
    "Assert": false,
    "BrowserTestUtils": false,
    "content": false,
    "ContentTask": false,
    "ContentTaskUtils": false,
    "EventUtils": false,
    "executeSoon": false,
    "expectUncaughtException": false,
    "export_assertions": false,
    "extractJarToTmp": false,
    "finish": false,
    "getChromeDir": false,
    "getJar": false,
    "getResolvedURI": false,
    "getRootDirectory": false,
    "getTestFilePath": false,
    "gTestPath": false,
    "info": false,
    "ignoreAllUncaughtExceptions": false,
    "is": false,
    "isnot": false,
    "ok": false,
    "PromiseDebugging": false,
    "privateNoteIntentionalCrash": false,
    "registerCleanupFunction": false,
    "requestLongerTimeout": false,
    "SpecialPowers": false,
    "TestUtils": false,
    "thisTestLeaksUncaughtRejectionsAndShouldBeFixed": false,
    "todo": false,
    "todo_is": false,
    "todo_isnot": false,
    "waitForClipboard": false,
    "waitForExplicitFinish": false,
    "waitForFocus": false,
    "XPCNativeWrapper": false,
    "XULDocument": false
  }
};
