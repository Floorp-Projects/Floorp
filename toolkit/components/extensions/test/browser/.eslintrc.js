"use strict";

module.exports = { // eslint-disable-line no-undef
  "extends": "../../../../../testing/mochitest/mochitest.eslintrc.js",

  "env": {
    "webextensions": true,
  },

  "globals": {
    "BrowserTestUtils": true,
    "ExtensionTestUtils": false,
    "XPCOMUtils": true,
  },

  "rules": {
    "no-shadow": 0,
  },
};
