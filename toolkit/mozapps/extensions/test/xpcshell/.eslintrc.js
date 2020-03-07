"use strict";

module.exports = {
  rules: {
    "no-unused-vars": [
      "error",
      { args: "none", varsIgnorePattern: "^end_test$" },
    ],
  },
  overrides: [
    {
      files: "head*.js",
      rules: {
        "no-unused-vars": [
          "error",
          {
            args: "none",
            vars: "local",
          },
        ],
      },
    },
  ],
};
