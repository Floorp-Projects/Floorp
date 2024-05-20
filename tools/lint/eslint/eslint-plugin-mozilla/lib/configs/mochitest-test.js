// Parent config file for all mochitest files.
"use strict";

module.exports = {
  env: {
    browser: true,
  },

  // All globals made available in the test environment.
  globals: {
    // SpecialPowers is injected into the window object via SimpleTest.js
    SpecialPowers: false,
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
    "mozilla/import-content-task-globals": "error",
    "mozilla/import-headjs-globals": "error",
    "mozilla/mark-test-function-used": "error",
    // Turn off no-define-cc-etc for mochitests as these don't have Cc etc defined in the
    // global scope.
    "mozilla/no-define-cc-etc": "off",
    // We mis-predict globals for HTML test files in directories shared
    // with browser tests, so don't try to "fix" imports that are needed.
    "mozilla/no-redeclare-with-import-autofix": "off",
    // Turn off use-chromeutils-generateqi as these tests don't have ChromeUtils
    // available.
    "mozilla/use-chromeutils-generateqi": "off",
    "no-shadow": "error",
    // Turn off no-unsanitized for tests, as we do want to be able to use
    // these for testing.
    "no-unsanitized/method": "off",
    "no-unsanitized/property": "off",
  },
};
