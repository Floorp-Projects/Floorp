"use strict";

module.exports = {
  env: {
    browser: true,
  },
  rules: {
    "no-unused-vars": [
      "error",
      { args: "none", varsIgnorePattern: "^EXPORTED_SYMBOLS|^Sync" },
    ],
  },
};
