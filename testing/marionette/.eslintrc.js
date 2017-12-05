"use strict";

module.exports = {
  "rules": {
    "camelcase": "error",
    "comma-dangle": ["error", "always-multiline"],
    "indent-legacy": ["error", 2, {
      "CallExpression": {"arguments": 2},
      "FunctionExpression": {"body": 1, "parameters": 2},
      "MemberExpression": 2,
      "SwitchCase": 1,
    }],
    "max-len": ["error", 78, {
      "ignoreStrings": true,
      "ignoreTemplateLiterals": true,
      "ignoreUrls": true,
    }],
    "no-fallthrough": "error",
    "no-new-object": "error",
    "no-undef-init": "error",
    "no-unused-vars": ["error", {}],
    "no-var": "error",
    "object-curly-spacing": ["error", "never"],
    "semi": "error",
  }
};
