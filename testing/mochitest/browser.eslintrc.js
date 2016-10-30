// Parent config file for all browser-chrome files.
module.exports = {
  "rules": {
    "mozilla/import-headjs-globals": 1,
    "mozilla/import-browserjs-globals": 1,
    "mozilla/mark-test-function-used": 1,
  },

  "env": {
    "browser": true,
    //"node": true
  },

  // All globals made available in the test environment.
  "globals": {
    "add_task": false,
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
    "getJar": false,
    "getRootDirectory": false,
    "getTestFilePath": false,
    "gTestPath": false,
    "info": false,
    "ignoreAllUncaughtExceptions": false,
    "is": false,
    "isnot": false,
    "ok": false,
    "registerCleanupFunction": false,
    "requestLongerTimeout": false,
    "SimpleTest": false,
    "SpecialPowers": false,
    "TestUtils": false,
    "thisTestLeaksUncaughtRejectionsAndShouldBeFixed": false,
    "todo": false,
    "todo_is": false,
    "todo_isnot": false,
    "waitForClipboard": false,
    "waitForExplicitFinish": false,
    "waitForFocus": false,
  }
};
