"use strict";

module.exports = {
  env: {
    webextensions: true,
  },

  rules: {
    "no-unused-vars": [
      "error",
      { argsIgnorePattern: "^_", varsIgnorePattern: "^end_test$" },
    ],
  },
};
