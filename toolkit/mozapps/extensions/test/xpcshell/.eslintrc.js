"use strict";

module.exports = {
  rules: {
    "no-unused-vars": [
      "error",
      { argsIgnorePattern: "^_", varsIgnorePattern: "^end_test$" },
    ],
  },
  overrides: [
    {
      files: "head*.js",
      rules: {
        "no-unused-vars": [
          "error",
          {
            argsIgnorePattern: "^_",
            vars: "local",
          },
        ],
      },
    },
  ],
};
