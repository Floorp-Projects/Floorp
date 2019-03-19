// Parent config file for all mochitest files.
"use strict";

module.exports = {
  "env": {
    "browser": true,
  },

  // All globals made available in the test environment.
  "globals": {
    // SpecialPowers is injected into the window object via SimpleTest.js
    "SpecialPowers": false,
    "XPCNativeWrapper": false,
  },

  "overrides": [{
    "env": {
      // Ideally we wouldn't be using the simpletest env here, but our uses of
      // js files mean we pick up everything from the global scope, which could
      // be any one of a number of html files. So we just allow the basics...
      "mozilla/simpletest": true,
    },
    "files": ["*.js"],
  }],

  "plugins": [
    "mozilla",
  ],

  "rules": {
    "mozilla/import-content-task-globals": "error",
    "mozilla/import-headjs-globals": "error",
    "mozilla/mark-test-function-used": "error",
    // Turn off no-define-cc-etc for mochitests as these don't have Cc etc defined in the
    // global scope.
    "mozilla/no-define-cc-etc": "off",
    "no-shadow": "error",
  },
};
