"use strict";

module.exports = {
  "extends": ["plugin:mozilla/xpcshell-test"],
  "rules": {
    "camelcase": "off",
    "max-len": ["error", 100, {
      "ignoreStrings": true,
      "ignoreTemplateLiterals": true,
      "ignoreUrls": true,
    }],
  },
};
