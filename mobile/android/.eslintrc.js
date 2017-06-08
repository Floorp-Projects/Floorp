"use strict";

module.exports = {
  rules: {
    // XXX Bug 1358949 - This should be reduced down - probably to 20 or to
    // be removed & synced with the mozilla/recommended value.
    "complexity": ["error", 31],

    // Rules enabled in mozilla/recommended, and disabled for now, we should
    // re-enable these over time.
    "block-spacing": "off",
    "brace-style": "off",
    "comma-spacing": "off",
    "consistent-return": "off",
    "eol-last": "off",
    "key-spacing": "off",
    "keyword-spacing": "off",
    "no-else-return": "off",
    "no-empty": "off",
    "no-extra-bind": "off",
    "no-extra-semi": "off",
    "no-lonely-if": "off",
    "no-multi-spaces": "off",
    "no-native-reassign": "off",
    "no-nested-ternary": "off",
    "no-new-object": "off",
    "no-octal": "off",
    "no-redeclare": "off",
    "no-trailing-spaces": "off",
    "no-useless-call": "off",
    "no-useless-concat": "off",
    "no-useless-return": "off",
    "no-undef": "off",
    "no-unused-vars": "off",
    "object-shorthand": "off",
    "quotes": "off", // [2, "double"]
    "space-before-blocks": "off",
    "space-before-function-paren": "off",
    "space-infix-ops": "off",
    "spaced-comment": "off",
  }
};
