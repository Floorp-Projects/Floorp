/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  globals: {
    // These are defined in the WebExtension script scopes by ExtensionCommon.jsm
    Cc: true,
    Ci: true,
    Cr: true,
    Cu: true,
    AppConstants: true,
    ExtensionAPI: true,
    ExtensionAPIPersistent: true,
    ExtensionCommon: true,
    ExtensionUtils: true,
    extensions: true,
    global: true,
    require: false,
    Services: true,
    XPCOMUtils: true,
  },

  rules: {
    // Rules from the mozilla plugin
    "mozilla/balanced-listeners": "error",
    "mozilla/no-aArgs": "error",
    "mozilla/var-only-at-top-level": "error",
    // Disable reject-importGlobalProperties because we don't want to include
    // these in the sandbox directly as that would potentially mean the
    // imported properties would be instatiated up-front rather than lazily.
    "mozilla/reject-importGlobalProperties": "off",

    // Functions are not required to consistently return something or nothing
    "consistent-return": "off",

    // Disallow empty statements. This will report an error for:
    // try { something(); } catch (e) {}
    // but will not report it for:
    // try { something(); } catch (e) { /* Silencing the error because ...*/ }
    // which is a valid use case.
    "no-empty": "error",

    // No expressions where a statement is expected
    "no-unused-expressions": "error",

    // No declaring variables that are never used
    "no-unused-vars": [
      "error",
      {
        args: "none",
        vars: "all",
        varsIgnorePattern: "^console$",
      },
    ],

    // No using things before they're defined.
    "no-use-before-define": [
      "error",
      {
        allowNamedExports: true,
        classes: true,
        // The next two being false allows idiomatic patterns which are more
        // type-inference friendly.  Functions are hoisted, so this is safe.
        functions: false,
        // This flag is only meaningful for `var` declarations.
        // When false, it still disallows use-before-define in the same scope.
        // Since we only allow `var` at the global scope, this is no worse than
        // how we currently declare an uninitialized `let` at the top of file.
        variables: false,
      },
    ],

    // Disallow using variables outside the blocks they are defined (especially
    // since only let and const are used, see "no-var").
    "block-scoped-var": "error",

    // Warn about cyclomatic complexity in functions.
    complexity: "error",

    // Don't warn for inconsistent naming when capturing this (not so important
    // with auto-binding fat arrow functions).
    // "consistent-this": ["error", "self"],

    // Don't require a default case in switch statements. Avoid being forced to
    // add a bogus default when you know all possible cases are handled.
    "default-case": "off",

    // Allow using == instead of ===, in the interest of landing something since
    // the devtools codebase is split on convention here.
    eqeqeq: "off",

    // Don't require function expressions to have a name.
    // This makes the code more verbose and hard to read. Our engine already
    // does a fantastic job assigning a name to the function, which includes
    // the enclosing function name, and worst case you have a line number that
    // you can just look up.
    "func-names": "off",

    // Allow use of function declarations and expressions.
    "func-style": "off",

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
    "new-cap": ["off", { capIsNew: false }],

    // Allow use of bitwise operators.
    "no-bitwise": "off",

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

    // Disallow fallthrough of case statements, except if there is a comment.
    "no-fallthrough": "error",

    // Allow comments inline after code.
    "no-inline-comments": "off",

    // Disallow use of labels for anything other then loops and switches.
    "no-labels": ["error", { allowLoop: true }],

    // Disallow use of multiline strings (use template strings instead).
    "no-multi-str": "error",

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

    // Require use of the second argument for parseInt().
    radix: "error",

    // Don't require to sort variables within the same declaration block.
    // Anyway, one-var is disabled.
    "sort-vars": "off",

    // Require "use strict" to be defined globally in the script.
    strict: ["error", "global"],

    // Allow vars to be declared anywhere in the scope.
    "vars-on-top": "off",

    // Disallow Yoda conditions (where literal value comes first).
    yoda: "error",

    // Disallow function or variable declarations in nested blocks
    "no-inner-declarations": "error",

    // Disallow labels that share a name with a variable
    "no-label-var": "error",
  },

  overrides: [
    {
      files: "test/xpcshell/head*.js",
      rules: {
        "no-unused-vars": [
          "error",
          {
            args: "none",
            vars: "local",
          },
        ],
      },
    },
  ],
};
