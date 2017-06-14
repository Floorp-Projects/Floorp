"use strict";

module.exports = {
  rules: {
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
  },
};
