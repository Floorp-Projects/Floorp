"use strict";

module.exports = {
  "rules": {
    "mozilla/no-aArgs": "error",
    "mozilla/reject-importGlobalProperties": ["error", "everything"],
    "mozilla/var-only-at-top-level": "error",
    "block-scoped-var": "error",
    "camelcase": ["error", { "properties": "never" }],
    "complexity": ["error", {"max": 20}],
    "max-nested-callbacks": ["error", 3],
    "new-cap": ["error", {"capIsNew": false}],
    "no-extend-native": "error",
    "no-fallthrough": "error",
    "no-inline-comments": "error",
    "no-multi-str": "error",
    "no-return-assign": "error",
    "no-shadow": "error",
    "strict": ["error", "global"],
    "yoda": "error"
  }
};
