"use strict";

module.exports = { // eslint-disable-line no-undef
  "extends": "../../../../../testing/mochitest/mochitest.eslintrc.js",

  "env": {
    "webextensions": true,
  },

  "globals": {
    "ChromeWorker": false,
    "onmessage": true,
    "sendAsyncMessage": false,

    "waitForLoad": true,
    "promiseConsoleOutput": true,

    "ExtensionTestUtils": false,
    "NetUtil": true,
    "webrequest_test": false,
    "XPCOMUtils": true,
  },
};
