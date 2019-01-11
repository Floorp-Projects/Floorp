"use strict";

module.exports = {
  "extends": [
    "plugin:mozilla/mochitest-test",
    "plugin:mozilla/chrome-test"
  ],
  "rules": {
    "brace-style": ["error", "1tbs", {"allowSingleLine": false}],
    "no-undef": "off",
    "no-unused-vars": "off",
  },
};
