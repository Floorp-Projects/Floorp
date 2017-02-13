"use strict";

module.exports = {
  "extends": [
    "../../../../testing/mochitest/mochitest.eslintrc.js"
  ],
  "rules": {
    // ownerGlobal doesn't exist in content privileged windows.
    "mozilla/use-ownerGlobal": "off",
  }
};
