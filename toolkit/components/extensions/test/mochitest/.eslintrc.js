"use strict";

module.exports = { // eslint-disable-line no-undef
  "extends": "../../../../../testing/mochitest/mochitest.eslintrc.js",

  "env": {
    "webextensions": true,
  },

  "globals": {
    "sendAsyncMessage": false,

    "waitForLoad": true,
    "promiseConsoleOutput": true,

    "ExtensionTestUtils": false,
    "NetUtil": true,
    "XPCOMUtils": true,
  },
};
