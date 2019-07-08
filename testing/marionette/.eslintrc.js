"use strict";

// inherits from ../../tools/lint/eslint/eslint-plugin-mozilla/lib/configs/recommended.js

module.exports = {
  "rules": {
    "camelcase": ["error", { "properties": "never" }],
    "no-fallthrough": "error",
    "no-undef-init": "error",
    "no-var": "error",
  }
};
