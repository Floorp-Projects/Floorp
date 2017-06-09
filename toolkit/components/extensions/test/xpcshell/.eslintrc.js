"use strict";

module.exports = {
  "extends": "plugin:mozilla/xpcshell-test",

  "env": {
    // The tests in this folder are testing based on WebExtensions, so lets
    // just define the webextensions environment here.
    "webextensions": true
  }
};
