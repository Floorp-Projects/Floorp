// Parent config file for all mochitest files.
"use strict";

module.exports = {
  "env": {
    "browser": true,
    "mozilla/browser-window": true,
    "mozilla/simpletest": true
  },

  // All globals made available in the test environment.
  "globals": {
    // `$` is defined in SimpleTest.js
    "$": false,
    "Assert": false,
    "BrowserTestUtils": false,
    "ContentTask": false,
    "EventUtils": false,
    "SpecialPowers": false,
    "addLoadEvent": false,
    "add_task": false,
    "executeSoon": false,
    "export_assertions": false,
    "extractJarToTmp": false,
    "finish": false,
    "gTestPath": false,
    "getJar": false,
    "getRootDirectory": false,
    "getTestFilePath": false,
    "info": false,
    "is": false,
    "isnot": false,
    "ok": false,
    "privateNoteIntentionalCrash": false,
    "promise": false,
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

  rules: {
    "mozilla/import-content-task-globals": "error",
    "mozilla/import-headjs-globals": "warn",
    "mozilla/mark-test-function-used": "warn"
  }
};
