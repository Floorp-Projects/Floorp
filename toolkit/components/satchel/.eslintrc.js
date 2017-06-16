"use strict";

module.exports = {
  rules: {
    "mozilla/no-cpows-in-tests": "error",
    "mozilla/var-only-at-top-level": "error",

    curly: ["error", "all"],
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
    semi: ["error", "always"],
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
    "block-scoped-var": "error",
    "no-use-before-define": ["error", {
      functions: false,
    }],
    complexity: ["error", {
      max: 20,
    }],
    "dot-location": ["error", "property"],
    "max-len": ["error", 100],
    "no-fallthrough": "error",
    "no-multiple-empty-lines": ["error", {
      max: 2,
    }],
    "no-throw-literal": "error",
    "padded-blocks": ["error", "never"],
    radix: "error",
    "array-bracket-spacing": ["error", "never"],
    "space-in-parens": ["error", "never"],
    "comma-dangle": ["error", "always-multiline"],
    "dot-notation": "error",
    "semi-spacing": ["error", {"before": false, "after": true}],
    "max-nested-callbacks": ["error", 4],
    "no-multi-str": "error",
    "comma-style": "error",
    "generator-star-spacing": ["error", "after"],
    "new-parens": "error",
    "no-unused-expressions": "error",
    "no-console": "error",
    "no-proto": "error",
    "no-unneeded-ternary": "error",
    yoda: "error",
    "no-new-wrappers": "error",
    "no-unused-vars": ["error", {
      args: "none",
      varsIgnorePattern: "^(Cc|Ci|Cr|Cu|EXPORTED_SYMBOLS)$",
    }],
    "no-caller": "error",
  },
};
