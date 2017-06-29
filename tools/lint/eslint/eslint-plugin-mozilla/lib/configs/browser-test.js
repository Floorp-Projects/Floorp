// Parent config file for all browser-chrome files.
"use strict";

module.exports = {
  "env": {
    "browser": true,
    "mozilla/browser-window": true,
    "mozilla/simpletest": true
    // "node": true
  },

  // All globals made available in the test environment.
  "globals": {
    // `$` is defined in SimpleTest.js
    "$": false,
    "Assert": false,
    "BrowserTestUtils": false,
    "ContentTask": false,
    "ContentTaskUtils": false,
    "EventUtils": false,
    "PromiseDebugging": false,
    "SpecialPowers": false,
    "TestUtils": false,
    "XPCNativeWrapper": false,
    "XULDocument": false,
    "addLoadEvent": false,
    "add_task": false,
    "content": false,
    "executeSoon": false,
    "expectUncaughtException": false,
    "export_assertions": false,
    "extractJarToTmp": false,
    "finish": false,
    "gTestPath": false,
    "getChromeDir": false,
    "getJar": false,
    "getResolvedURI": false,
    "getRootDirectory": false,
    "getTestFilePath": false,
    "ignoreAllUncaughtExceptions": false,
    "info": false,
    "is": false,
    "isnot": false,
    "ok": false,
    "privateNoteIntentionalCrash": false,
    "registerCleanupFunction": false,
    "requestLongerTimeout": false,
    "todo": false,
    "todo_is": false,
    "todo_isnot": false,
    "waitForClipboard": false,
    "waitForExplicitFinish": false,
    "waitForFocus": false
  },

  "plugins": [
    "mozilla"
  ],

  "rules": {
    "mozilla/import-content-task-globals": "error",
    "mozilla/import-headjs-globals": "warn",
    "mozilla/mark-test-function-used": "warn"
  }
};
