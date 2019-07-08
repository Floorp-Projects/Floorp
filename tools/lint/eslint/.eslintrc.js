"use strict";

module.exports = {
  "env": {
    "node": true
  },

  "rules": {
    "camelcase": ["error", { "properties": "never" }],
    "handle-callback-err": ["error", "er"],
    "no-shadow": "error",
    "no-undef-init": "error",
    "one-var": ["error", "never"],
    "strict": ["error", "global"],
  },
};
