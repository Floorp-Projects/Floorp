// Parent config file for all mochitest files.
module.exports = {
  rules: {
    "mozilla/import-headjs-globals": "warn",
    "mozilla/import-browserjs-globals": "warn",
    "mozilla/import-test-globals": "warn",
    "mozilla/mark-test-function-used": "warn",
  },

  "env": {
    "browser": true,
  },

  // All globals made available in the test environment.
  "globals": {
    // `$` is defined in SimpleTest.js
    "$": false,
    "add_task": false,
    "addLoadEvent": false,
    "Assert": false,
    "BrowserTestUtils": false,
    "EventUtils": false,
    "executeSoon": false,
    "export_assertions": false,
    "finish": false,
    "getRootDirectory": false,
    "getTestFilePath": false,
    "gTestPath": false,
    "info": false,
    "is": false,
    "isnot": false,
    "ok": false,
    "privateNoteIntentionalCrash": false,
    "promise": false,
    "registerCleanupFunction": false,
    "requestLongerTimeout": false,
    "SpecialPowers": false,
    "todo": false,
    "todo_is": false,
    "todo_isnot": false,
    "waitForClipboard": false,
    "waitForExplicitFinish": false,
    "waitForFocus": false,
  }
};
