"use strict";

module.exports = {
  "extends": "plugin:mozilla/browser-test",

  "env": {
    "webextensions": true,
  },

  "rules": {
    "no-shadow": "off",
  },
};
