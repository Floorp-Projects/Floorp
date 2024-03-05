// Parent config file for all xpcshell files.
"use strict";

module.exports = {
  rules: {
    "mozilla/import-headjs-globals": "error",
    "mozilla/mark-test-function-used": "error",
    "mozilla/no-arbitrary-setTimeout": "error",

    // Bug 1883707 - Turn off no-console in tests at the moment.
    "no-console": "off",
    // Turn off no-unsanitized for tests, as we do want to be able to use
    // these for testing.
    "no-unsanitized/method": "off",
    "no-unsanitized/property": "off",
  },
};
