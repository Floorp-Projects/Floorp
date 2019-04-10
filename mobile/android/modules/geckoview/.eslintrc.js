"use strict";

module.exports = {
  "globals": {
    "debug": false,
    "warn": false,
  },
  "rules": {
    "prefer-const": "error",
    "indent": [
      "error",
      2,
      {
        "FunctionExpression": {
          "parameters": "first"
        },
        "CallExpression": {
          "arguments": "first"
        },
        "ObjectExpression": "first",
        "MemberExpression": "off",
        "ArrayExpression": "first",
        "SwitchCase": 1,
      }
    ],
  },
};
