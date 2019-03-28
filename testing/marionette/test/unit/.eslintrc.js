"use strict";

module.exports = {
  "rules": {
    "camelcase": "off",
    "max-len": ["error", 100, {
      "ignoreStrings": true,
      "ignoreTemplateLiterals": true,
      "ignoreUrls": true,
    }],
  },
};
