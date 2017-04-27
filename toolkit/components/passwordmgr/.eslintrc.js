"use strict";

module.exports = {

  "rules": {
    "comma-style": "error",
    // XXX Bug 1358949 - This should be reduced down - probably to 20 or to
    // be removed & synced with the mozilla/recommended value.
    "complexity": ["error", 43],

    "no-array-constructor": "error",
    "no-unused-vars": ["error", {"args": "none", "vars": "local", "varsIgnorePattern": "^(ids|ignored|unused)$"}],
    "semi": ["error", "always"],
  }
};
