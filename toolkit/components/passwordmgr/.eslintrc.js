"use strict";

module.exports = {
  "rules": {
    "block-scoped-var": "error",

    // XXX Bug 1358949 - This should be reduced down - probably to 20 or to
    // be removed & synced with the mozilla/recommended value.
    "complexity": ["error", 56],

    "no-unused-vars": ["error", {"args": "none", "vars": "local", "varsIgnorePattern": "^(ids|ignored|unused)$"}],
    "no-var": "error",
  }
};
