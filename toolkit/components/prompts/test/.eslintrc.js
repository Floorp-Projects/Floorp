"use strict";

module.exports = {
  "extends": [
    "plugin:mozilla/mochitest-test"
  ],
  "rules": {
    // ownerGlobal doesn't exist in content privileged windows.
    "mozilla/use-ownerGlobal": "off",
  }
};
