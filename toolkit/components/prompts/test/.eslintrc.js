"use strict";

module.exports = {
  extends: ["plugin:mozilla/mochitest-test"],
  globals: {
    Assert: true,
  },
  rules: {
    // ownerGlobal doesn't exist in content privileged windows.
    "mozilla/use-ownerGlobal": "off",
  },
};
