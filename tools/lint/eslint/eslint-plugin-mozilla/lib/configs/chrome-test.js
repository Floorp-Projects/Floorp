// Parent config file for all mochitest files.
"use strict";

module.exports = {
  env: {
    browser: true,
    "mozilla/browser-window": true,
  },

  // All globals made available in the test environment.
  globals: {
    // SpecialPowers is injected into the window object via SimpleTest.js
    SpecialPowers: false,
    extractJarToTmp: false,
    getChromeDir: false,
    getJar: false,
    getResolvedURI: false,
    getRootDirectory: false,
  },

  overrides: [
    {
      env: {
        // Ideally we wouldn't be using the simpletest env here, but our uses of
        // js files mean we pick up everything from the global scope, which could
        // be any one of a number of html files. So we just allow the basics...
        "mozilla/simpletest": true,
      },
      files: ["*.js"],
    },
  ],

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
  },
};
