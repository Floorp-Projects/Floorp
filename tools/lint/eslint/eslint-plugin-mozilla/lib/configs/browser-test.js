// Parent config file for all browser-chrome files.
"use strict";

module.exports = {
  env: {
    browser: true,
    "mozilla/browser-window": true,
    "mozilla/simpletest": true,
    // "node": true
  },

  // All globals made available in the test environment.
  globals: {
    // `$` is defined in SimpleTest.js
    $: false,
    Assert: false,
    BrowserTestUtils: false,
    ContentTask: false,
    ContentTaskUtils: false,
    EventUtils: false,
    IOUtils: false,
    PathUtils: false,
    PromiseDebugging: false,
    SpecialPowers: false,
    TestUtils: false,
    XPCNativeWrapper: false,
    addLoadEvent: false,
    add_setup: false,
    add_task: false,
    content: false,
    executeSoon: false,
    expectUncaughtException: false,
    export_assertions: false,
    extractJarToTmp: false,
    finish: false,
    gTestPath: false,
    getChromeDir: false,
    getJar: false,
    getResolvedURI: false,
    getRootDirectory: false,
    getTestFilePath: false,
    ignoreAllUncaughtExceptions: false,
    info: false,
    is: false,
    isnot: false,
    ok: false,
    record: false,
    registerCleanupFunction: false,
    requestLongerTimeout: false,
    setExpectedFailuresForSelfTest: false,
    stringContains: false,
    stringMatches: false,
    todo: false,
    todo_is: false,
    todo_isnot: false,
    waitForClipboard: false,
    waitForExplicitFinish: false,
    waitForFocus: false,
  },

  plugins: ["mozilla", "@microsoft/sdl"],

  rules: {
    // No using of insecure url, so no http urls
    "@microsoft/sdl/no-insecure-url": [
      "error",
      {
        exceptions: [
          "^http:\\/\\/mochi\\.test?.*",
          "^http:\\/\\/localhost?.*",
          "^http:\\/\\/127\\.0\\.0\\.1?.*",
          // Exempt xmlns urls
          "^http:\\/\\/www\\.w3\\.org?.*",
          "^http:\\/\\/www\\.mozilla\\.org\\/keymaster\\/gatekeeper?.*",
          // Exempt urls that start with ftp or ws.
          "^ws:?.*",
          "^ftp:?.*",
        ],
        varExceptions: ["insecure?.*"],
      },
    ],
    "mozilla/import-content-task-globals": "error",
    "mozilla/import-headjs-globals": "error",
    "mozilla/mark-test-function-used": "error",
    "mozilla/no-addtask-setup": "error",
    "mozilla/no-arbitrary-setTimeout": "error",
  },
};
