"use strict";

module.exports = {
  "rules": {
    "brace-style": ["error", "1tbs", {"allowSingleLine": false}],

    // XXX Bug 1358949 - This should be reduced down - probably to 20 or to
    // be removed & synced with the mozilla/recommended value.
    "complexity": ["error", 56],

    "indent": ["error", 2, {
      ignoredNodes: ["ConditionalExpression"],
      ArrayExpression: "first",
      SwitchCase: 1,
      CallExpression: {
        arguments: "first",
      },
      FunctionExpression: {
        parameters: "first",
      },
      FunctionDeclaration: {
        parameters: "first",
      },
      MemberExpression: "off",
      ObjectExpression: "first",
      outerIIFEBody: 0,
    }],

    "curly": ["error", "all"],
    "no-unused-vars": ["error", {"args": "none", "vars": "local", "varsIgnorePattern": "^(ids|ignored|unused)$"}],
    "space-in-parens": ["error"],
  }
};
