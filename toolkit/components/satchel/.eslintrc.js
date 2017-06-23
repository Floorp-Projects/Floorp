"use strict";

module.exports = {
  rules: {
    "mozilla/balanced-listeners": "error",
    "mozilla/no-cpows-in-tests": "error",
    "mozilla/var-only-at-top-level": "error",

    "array-bracket-spacing": ["error", "never"],
    "block-scoped-var": "error",
    "comma-dangle": ["error", "always-multiline"],
    "comma-style": "error",
    complexity: ["error", {
      max: 20,
    }],
    curly: ["error", "all"],
    "dot-location": ["error", "property"],
    "dot-notation": "error",
    "generator-star-spacing": ["error", "after"],
    indent: ["error", 2, {
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
    "no-caller": "error",
    "no-console": "error",
    "no-fallthrough": "error",
    "no-multi-str": "error",
    "no-multiple-empty-lines": ["error", {
      max: 2,
    }],
    "no-new-wrappers": "error",
    "no-proto": "error",
    "no-throw-literal": "error",
    "no-unneeded-ternary": "error",
    "no-unused-expressions": "error",
    "no-unused-vars": ["error", {
      args: "none",
      varsIgnorePattern: "^(Cc|Ci|Cr|Cu|EXPORTED_SYMBOLS)$",
    }],
    "no-use-before-define": ["error", {
      functions: false,
    }],
    "padded-blocks": ["error", "never"],
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
