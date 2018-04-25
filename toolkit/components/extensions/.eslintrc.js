"use strict";

module.exports = {

  "globals": {
    // These are defined in the WebExtension script scopes by ExtensionCommon.jsm
    "Cc": true,
    "Ci": true,
    "Cr": true,
    "Cu": true,
    "AppConstants": true,
    "ExtensionAPI": true,
    "ExtensionCommon": true,
    "ExtensionUtils": true,
    "extensions": true,
    "global": true,
    "require": false,
    "Services": true,
    "XPCOMUtils": true,
  },

  "rules": {
    // Rules from the mozilla plugin
    "mozilla/balanced-listeners": "error",
    "mozilla/no-aArgs": "error",
    "mozilla/var-only-at-top-level": "error",

    "valid-jsdoc": ["error", {
      "prefer": {
        "return": "returns",
      },
      "preferType": {
        "Boolean": "boolean",
        "Number": "number",
        "String": "string",
        "bool": "boolean",
      },
      "requireParamDescription": false,
      "requireReturn": false,
      "requireReturnDescription": false,
    }],

    // Forbid spaces inside the square brackets of array literals.
    "array-bracket-spacing": ["error", "never"],

    // Forbid spaces inside the curly brackets of object literals.
    "object-curly-spacing": ["error", "never"],

    // No space padding in parentheses
    "space-in-parens": ["error", "never"],

    // Functions are not required to consistently return something or nothing
    "consistent-return": "off",

    // Require braces around blocks that start a new line
    "curly": ["error", "all"],

    // Two space indent
    "indent": [
      "error", 2,
      {
        "ArrayExpression": "first",
        "CallExpression": {"arguments": "first"},
        "FunctionDeclaration": {"parameters": "first"},
        "FunctionExpression": {"parameters": "first"},
        "MemberExpression": "off",
        "ObjectExpression": "first",
        "SwitchCase": 1,
        "ignoredNodes": ["ConditionalExpression"],
      },
    ],

    // Always require parenthesis for new calls
    "new-parens": "error",

    // Disallow empty statements. This will report an error for:
    // try { something(); } catch (e) {}
    // but will not report it for:
    // try { something(); } catch (e) { /* Silencing the error because ...*/ }
    // which is a valid use case.
    "no-empty": "error",

    // No mixing different operators without parens
    "no-mixed-operators": ["error", {"groups": [["&&", "||"], ["==", "!=", "===", "!==", ">", ">=", "<", "<="], ["in", "instanceof"]]}],

    // Disallow use of multiple spaces (sometimes used to align const values,
    // array or object items, etc.). It's hard to maintain and doesn't add that
    // much benefit.
    "no-multi-spaces": "error",

    // No expressions where a statement is expected
    "no-unused-expressions": "error",

    // No declaring variables that are never used
    "no-unused-vars": ["error", {
      "args": "none", "vars": "all", "varsIgnorePattern": "^console$"
    }],

    // No using variables before defined
    "no-use-before-define": "error",

    // Never use spaces before function parentheses
    "space-before-function-paren": ["error", {"anonymous": "never", "named": "never"}],

    // ++ and -- should not need spacing
    "space-unary-ops": ["error", {"nonwords": false, "words": true, "overrides": {"typeof": false}}],

    // Disallow using variables outside the blocks they are defined (especially
    // since only let and const are used, see "no-var").
    "block-scoped-var": "error",

    // Allow trailing commas for easy list extension.  Having them does not
    // impair readability, but also not required either.
    "comma-dangle": ["error", "always-multiline"],

    // Warn about cyclomatic complexity in functions.
    "complexity": "error",

    // Don't warn for inconsistent naming when capturing this (not so important
    // with auto-binding fat arrow functions).
    // "consistent-this": ["error", "self"],

    // Don't require a default case in switch statements. Avoid being forced to
    // add a bogus default when you know all possible cases are handled.
    "default-case": "off",

    // Enforce dots on the next line with property name.
    "dot-location": ["error", "property"],

    // Allow using == instead of ===, in the interest of landing something since
    // the devtools codebase is split on convention here.
    "eqeqeq": "off",

    // Don't require function expressions to have a name.
    // This makes the code more verbose and hard to read. Our engine already
    // does a fantastic job assigning a name to the function, which includes
    // the enclosing function name, and worst case you have a line number that
    // you can just look up.
    "func-names": "off",

    // Allow use of function declarations and expressions.
    "func-style": "off",

    // Maximum length of a line.
    // Disabled because we exceed this in too many places.
    "max-len": [0, 80],

    // Maximum depth callbacks can be nested.
    "max-nested-callbacks": ["error", 4],

    // Don't limit the number of parameters that can be used in a function.
    "max-params": "off",

    // Don't limit the maximum number of statement allowed in a function. We
    // already have the complexity rule that's a better measurement.
    "max-statements": "off",

    // Don't require a capital letter for constructors, only check if all new
    // operators are followed by a capital letter. Don't warn when capitalized
    // functions are used without the new operator.
    "new-cap": ["off", {"capIsNew": false}],

    // Allow use of bitwise operators.
    "no-bitwise": "off",

    // Disallow the catch clause parameter name being the same as a variable in
    // the outer scope, to avoid confusion.
    "no-catch-shadow": "off",

    // Disallow using the console API.
    "no-console": "error",

    // Allow using constant expressions in conditions like while (true)
    "no-constant-condition": "off",

    // Allow use of the continue statement.
    "no-continue": "off",

    // Allow division operators explicitly at beginning of regular expression.
    "no-div-regex": "off",

    // Disallow adding to native types
    "no-extend-native": "error",

    // Allow unnecessary parentheses, as they may make the code more readable.
    "no-extra-parens": "off",

    // Disallow fallthrough of case statements, except if there is a comment.
    "no-fallthrough": "error",

    // Allow the use of leading or trailing decimal points in numeric literals.
    "no-floating-decimal": "off",

    // Allow comments inline after code.
    "no-inline-comments": "off",

    // Disallow use of labels for anything other then loops and switches.
    "no-labels": ["error", {"allowLoop": true}],

    // Disallow use of multiline strings (use template strings instead).
    "no-multi-str": "error",

    // Disallow multiple empty lines.
    "no-multiple-empty-lines": [1, {"max": 2}],

    // Allow reassignment of function parameters.
    "no-param-reassign": "off",

    // Allow string concatenation with __dirname and __filename (not a node env).
    "no-path-concat": "off",

    // Allow use of unary operators, ++ and --.
    "no-plusplus": "off",

    // Allow using process.env (not a node environment).
    "no-process-env": "off",

    // Allow using process.exit (not a node environment).
    "no-process-exit": "off",

    // Disallow usage of __proto__ property.
    "no-proto": "error",

    // Allow reserved words being used as object literal keys.
    "no-reserved-keys": "off",

    // Don't restrict usage of specified node modules (not a node environment).
    "no-restricted-modules": "off",

    // Disallow use of assignment in return statement. It is preferable for a
    // single line of code to have only one easily predictable effect.
    "no-return-assign": "error",

    // Don't warn about declaration of variables already declared in the outer scope.
    "no-shadow": "off",

    // Allow use of synchronous methods (not a node environment).
    "no-sync": "off",

    // Allow the use of ternary operators.
    "no-ternary": "off",

    // Disallow throwing literals (eg. throw "error" instead of
    // throw new Error("error")).
    "no-throw-literal": "error",

    // Allow dangling underscores in identifiers (for privates).
    "no-underscore-dangle": "off",

    // Allow use of undefined variable.
    "no-undefined": "off",

    // We use var-only-at-top-level instead of no-var as we allow top level
    // vars.
    "no-var": "off",

    // Allow using TODO/FIXME comments.
    "no-warning-comments": "off",

    // Don't require method and property shorthand syntax for object literals.
    // We use this in the code a lot, but not consistently, and this seems more
    // like something to check at code review time.
    "object-shorthand": "off",

    // Allow more than one variable declaration per function.
    "one-var": "off",

    // Disallow padding within blocks.
    "padded-blocks": ["error", "never"],

    // Don't require quotes around object literal property names.
    "quote-props": "off",

    // Require use of the second argument for parseInt().
    "radix": "error",

    // Enforce spacing after semicolons.
    "semi-spacing": ["error", {"before": false, "after": true}],

    // Don't require to sort variables within the same declaration block.
    // Anyway, one-var is disabled.
    "sort-vars": "off",

    // Require a space immediately following the // in a line comment.
    "spaced-comment": ["error", "always"],

    // Require "use strict" to be defined globally in the script.
    "strict": ["error", "global"],

    // Allow vars to be declared anywhere in the scope.
    "vars-on-top": "off",

    // Don't require immediate function invocation to be wrapped in parentheses.
    "wrap-iife": "off",

    // Don't require regex literals to be wrapped in parentheses (which
    // supposedly prevent them from being mistaken for division operators).
    "wrap-regex": "off",

    // Disallow Yoda conditions (where literal value comes first).
    "yoda": "error",

    // Disallow function or variable declarations in nested blocks
    "no-inner-declarations": "error",

    // Disallow labels that share a name with a variable
    "no-label-var": "error",
  },
};
