"use strict";

module.exports = {
  "rules": {
    "indent-legacy": ["error", 2, { "SwitchCase": 1 }],
    "new-parens": "error",
    "no-inner-declarations": "error",
    "no-shadow": "error",
    "no-unused-vars": ["error", {"vars": "all", "varsIgnorePattern": "^EXPORTED_SYMBOLS$", "args": "none"}],
  },
}
