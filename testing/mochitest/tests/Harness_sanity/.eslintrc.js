"use strict";

module.exports = {
  extends: ["plugin:mozilla/xpcshell-test"],

  overrides: [
    {
      files: "*.html",
      env: { browser: true },
    },
  ],
};
