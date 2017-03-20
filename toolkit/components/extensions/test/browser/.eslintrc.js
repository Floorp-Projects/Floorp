"use strict";

module.exports = {
  "extends": "plugin:mozilla/mochitest-test",

  "env": {
    "webextensions": true,
  },

  "globals": {
    "BrowserTestUtils": true,
    "ExtensionTestUtils": false,
    "XPCOMUtils": true,
  },

  "rules": {
    "no-shadow": "off",
  },
};
