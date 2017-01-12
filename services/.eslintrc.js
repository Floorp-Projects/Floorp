"use strict";

module.exports = {
  "extends": [
    "../toolkit/.eslintrc.js"
  ],
  rules: {
    /* These rules are only set to warn temporarily
       until they get fixed, at which point their
       respective line in this file should be removed. */
    "consistent-return": "warn",
    "no-func-assign": "warn",
    "no-nested-ternary": "warn",
  }
};
