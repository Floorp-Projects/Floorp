"use strict";

module.exports = {
  rules: {
    "mozilla/balanced-listeners": "error",
    "mozilla/var-only-at-top-level": "error",

    "array-bracket-spacing": ["error", "never"],
    "block-scoped-var": "error",
    complexity: ["error", {
      max: 23,
    }],
    curly: ["error", "all"],
    "dot-location": ["error", "property"],
    "indent-legacy": ["error", 2, {
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
      // XXX: following line is used in eslint v4 to not throw an error when chaining methods
      //MemberExpression: "off",
      outerIIFEBody: 0,
    }],
    "max-len": ["error", 100],
    "max-nested-callbacks": ["error", 4],
    "new-parens": "error",
    "no-console": "error",
    "no-fallthrough": "error",
    "no-multi-str": "error",
    "no-multiple-empty-lines": ["error", {
      max: 2,
    }],
    "no-proto": "error",
    "no-unused-expressions": "error",
    "no-unused-vars": ["error", {
      args: "none",
      vars: "all",
    }],
    "no-use-before-define": ["error", {
      functions: false,
    }],
    radix: "error",
    semi: ["error", "always"],
    "semi-spacing": ["error", {"before": false, "after": true}],
    "space-in-parens": ["error", "never"],
    "valid-jsdoc": ["error", {
      prefer: {
        return: "returns",
      },
      preferType: {
        Boolean: "boolean",
        Number: "number",
        String: "string",
        bool: "boolean",
      },
      requireParamDescription: false,
      requireReturn: false,
      requireReturnDescription: false,
    }],
    yoda: "error",
  },
};
