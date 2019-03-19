"use strict";

module.exports = {
  "rules": {
    "mozilla/no-aArgs": "error",
    "mozilla/reject-importGlobalProperties": ["error", "everything"],
    "mozilla/var-only-at-top-level": "error",
    "block-scoped-var": "error",
    "camelcase": "error",
    "complexity": ["error", {"max": 20}],
    "curly": "error",
    "dot-location": ["error", "property"],
    "indent-legacy": ["error", 2, {"SwitchCase": 1}],
    "max-len": ["error", 80, 2, {"ignoreUrls": true}],
    "max-nested-callbacks": ["error", 3],
    "new-cap": ["error", {"capIsNew": false}],
    "new-parens": "error",
    "no-extend-native": "error",
    "no-fallthrough": "error",
    "no-inline-comments": "error",
    "no-mixed-spaces-and-tabs": "error",
    "no-multi-spaces": "error",
    "no-multi-str": "error",
    "no-multiple-empty-lines": ["error", {"max": 1}],
    "no-return-assign": "error",
    "no-shadow": "error",
    "quotes": ["error", "double", "avoid-escape"],
    "semi-spacing": ["error", {"before": false, "after": true}],
    "space-in-parens": ["error", "never"],
    "strict": ["error", "global"],
    "yoda": "error"
  }
};
