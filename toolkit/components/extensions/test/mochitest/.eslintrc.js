"use strict";

module.exports = { // eslint-disable-line no-undef
  "extends": "../../../../../testing/mochitest/mochitest.eslintrc.js",

  "env": {
    "webextensions": true,
  },

  "globals": {
    "onmessage": true,
    "sendAsyncMessage": false,

    "waitForLoad": true,
    "promiseConsoleOutput": true,

    "ExtensionTestUtils": false,
    "NetUtil": true,
    "webrequest_test": false,
    "XPCOMUtils": true,

    // head_webrequest.js symbols
    "addStylesheet": true,
    "addLink": true,
    "addImage": true,
    "addScript": true,
    "addFrame": true,
    "makeExtension": false,
  },

  "rules": {
    "no-shadow": 0,
  },
};
