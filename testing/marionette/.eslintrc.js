"use strict";

module.exports = {
  "rules": {
    "comma-dangle": ["error", "always-multiline"],
    "indent": ["error", 2, {
      "SwitchCase": 1,
      "FunctionExpression": {"body": 1, "parameters": 2},
      "MemberExpression": 2,
    }],
    "max-len": ["error", 78, {
      "ignoreStrings": true,
      "ignoreUrls": true,
    }],
    "no-new-object": "error",
    "object-curly-spacing": ["error", "never"],
  }
};
