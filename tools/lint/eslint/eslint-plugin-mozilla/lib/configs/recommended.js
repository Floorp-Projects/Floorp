/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * The configuration is based on eslint:recommended config. The details for all
 * the ESLint rules, and which ones are in the recommended configuration can
 * be found here:
 *
 * https://eslint.org/docs/rules/
 */
module.exports = {
  env: {
    browser: true,
    es2021: true,
    "mozilla/privileged": true,
    "mozilla/specific": true,
  },

  // The prettier configuration here comes from eslint-config-prettier and
  // turns off all of ESLint's rules related to formatting.
  extends: [
    "eslint:recommended",
    "prettier",
    "plugin:json/recommended-with-comments",
  ],

  overrides: [
    {
      // System mjs files and jsm files are not loaded in the browser scope,
      // so we turn that off for those. Though we do have our own special
      // environment for them.
      env: {
        browser: false,
        "mozilla/jsm": true,
      },
      files: ["**/*.sys.mjs", "**/*.jsm", "**/*.jsm.js"],
      rules: {
        "mozilla/lazy-getter-object-name": "error",
        "mozilla/reject-eager-module-in-lazy-getter": "error",
        "mozilla/reject-global-this": "error",
        "mozilla/reject-globalThis-modification": "error",
        // For all system modules, we expect no properties to need importing,
        // hence reject everything.
        "mozilla/reject-importGlobalProperties": ["error", "everything"],
        "mozilla/reject-mixing-eager-and-lazy": "error",
        "mozilla/reject-top-level-await": "error",
        // TODO: Bug 1575506 turn `builtinGlobals` on here.
        // We can enable builtinGlobals for jsms due to their scopes.
        "no-redeclare": ["error", { builtinGlobals: false }],
      },
    },
    {
      files: ["**/*.mjs", "**/*.jsm", "**/?(*.)worker.?(m)js"],
      rules: {
        // Modules and workers are far easier to check for no-unused-vars on a
        // global scope, than our content files. Hence we turn that on here.
        "no-unused-vars": [
          "error",
          {
            args: "none",
            vars: "all",
          },
        ],
      },
    },
    {
      files: ["**/*.sys.mjs"],
      rules: {
        "mozilla/use-static-import": "error",
      },
    },
    {
      excludedFiles: ["**/*.sys.mjs"],
      files: ["**/*.mjs"],
      rules: {
        "mozilla/reject-import-system-module-from-non-system": "error",
        "mozilla/reject-lazy-imports-into-globals": "error",
        "no-shadow": ["error", { allow: ["event"], builtinGlobals: true }],
      },
    },
    {
      files: ["**/*.mjs"],
      rules: {
        "mozilla/use-static-import": "error",
        // This rule defaults to not allowing "use strict" in module files since
        // they are always loaded in strict mode.
        strict: "error",
      },
    },
    {
      files: ["**/*.jsm", "**/*.jsm.js"],
      rules: {
        "mozilla/mark-exported-symbols-as-used": "error",
      },
    },
    {
      env: {
        browser: false,
        "mozilla/privileged": false,
        "mozilla/sjs": true,
        "mozilla/specific": false,
      },
      files: ["**/*.sjs"],
      rules: {
        // For sjs files, reject everything as we should update the sandbox
        // to include the globals we need, as these are test-only files.
        "mozilla/reject-importGlobalProperties": ["error", "everything"],
      },
    },
    {
      env: {
        browser: false,
        worker: true,
      },
      files: [
        // Most files should use the `.worker.` format to be consistent with
        // other items like `.sys.mjs`, but we allow simply calling the file
        // "worker" as well.
        "**/?(*.)worker.?(m)js",
      ],
    },
  ],

  parserOptions: {
    ecmaVersion: "latest",
  },

  // When adding items to this file please check for effects on sub-directories.
  plugins: ["fetch-options", "html", "json", "no-unsanitized"],

  // When adding items to this file please check for effects on all of toolkit
  // and browser
  rules: {
    // This may conflict with prettier, so we turn it off.
    "arrow-body-style": "off",

    // Warn about cyclomatic complexity in functions.
    // XXX Get this down to 20?
    complexity: ["error", 34],

    // Functions must always return something or nothing
    "consistent-return": "error",

    // XXX This rule line should be removed to enable it. See bug 1487642.
    // Require super() calls in constructors
    "constructor-super": "off",

    // Require braces around blocks that start a new line
    curly: ["error", "all"],

    // Encourage the use of dot notation whenever possible.
    "dot-notation": "error",

    // XXX This rule should be enabled, see Bug 1557040
    // No credentials submitted with fetch calls
    "fetch-options/no-fetch-credentials": "off",

    // XXX This rule line should be removed to enable it. See bug 1487642.
    // Enforce return statements in getters
    "getter-return": "off",

    // Don't enforce the maximum depth that blocks can be nested. The complexity
    // rule is a better rule to check this.
    "max-depth": "off",

    // Maximum depth callbacks can be nested.
    "max-nested-callbacks": ["error", 10],

    "mozilla/avoid-removeChild": "error",
    "mozilla/consistent-if-bracing": "error",
    "mozilla/import-browser-window-globals": "error",
    "mozilla/import-globals": "error",
    "mozilla/no-compare-against-boolean-literals": "error",
    "mozilla/no-cu-reportError": "error",
    "mozilla/no-define-cc-etc": "error",
    "mozilla/no-throw-cr-literal": "error",
    "mozilla/no-useless-parameters": "error",
    "mozilla/no-useless-removeEventListener": "error",
    "mozilla/prefer-boolean-length-check": "error",
    "mozilla/prefer-formatValues": "error",
    "mozilla/reject-addtask-only": "error",
    "mozilla/reject-chromeutils-import-params": "error",
    "mozilla/reject-importGlobalProperties": ["error", "allownonwebidl"],
    "mozilla/reject-multiple-getters-calls": "error",
    "mozilla/reject-scriptableunicodeconverter": "warn",
    "mozilla/rejects-requires-await": "error",
    "mozilla/use-cc-etc": "error",
    "mozilla/use-chromeutils-definelazygetter": "error",
    "mozilla/use-chromeutils-generateqi": "error",
    "mozilla/use-chromeutils-import": "error",
    "mozilla/use-console-createInstance": "error",
    "mozilla/use-default-preference-values": "error",
    "mozilla/use-includes-instead-of-indexOf": "error",
    "mozilla/use-isInstance": "error",
    "mozilla/use-ownerGlobal": "error",
    "mozilla/use-returnValue": "error",
    "mozilla/use-services": "error",
    "mozilla/valid-lazy": "error",
    "mozilla/valid-services": "error",

    // Use [] instead of Array()
    "no-array-constructor": "error",

    // Disallow use of arguments.caller or arguments.callee.
    "no-caller": "error",

    // XXX Bug 1487642 - decide if we want to enable this or not.
    // Disallow lexical declarations in case clauses
    "no-case-declarations": "off",

    // XXX Bug 1487642 - decide if we want to enable this or not.
    // Disallow the use of console
    "no-console": "off",

    // Disallows expressions where the operation doesn't affect the value.
    "no-constant-binary-expression": "error",

    // XXX Bug 1487642 - decide if we want to enable this or not.
    // Disallow constant expressions in conditions
    "no-constant-condition": "off",

    // No duplicate keys in object declarations
    "no-dupe-keys": "error",

    // If an if block ends with a return no need for an else block
    "no-else-return": "error",

    // No empty statements
    "no-empty": ["error", { allowEmptyCatch: true }],

    // Disallow eval and setInteral/setTimeout with strings
    "no-eval": "error",

    // Disallow unnecessary calls to .bind()
    "no-extra-bind": "error",

    // Disallow fallthrough of case statements
    "no-fallthrough": [
      "error",
      {
        // The eslint rule doesn't allow for case-insensitive regex option.
        // The following pattern allows for a dash between "fall through" as
        // well as alternate spelling of "fall thru". The pattern also allows
        // for an optional "s" at the end of "fall" ("falls through").
        commentPattern:
          "[Ff][Aa][Ll][Ll][Ss]?[\\s-]?([Tt][Hh][Rr][Oo][Uu][Gg][Hh]|[Tt][Hh][Rr][Uu])",
      },
    ],

    // Disallow assignments to native objects or read-only global variables
    "no-global-assign": "error",

    // Disallow eval and setInteral/setTimeout with strings
    "no-implied-eval": "error",

    // This has been superseded since we're using ES6.
    // Disallow variable or function declarations in nested blocks
    "no-inner-declarations": "off",

    // Disallow the use of the __iterator__ property
    "no-iterator": "error",

    // No labels
    "no-labels": "error",

    // Disallow unnecessary nested blocks
    "no-lone-blocks": "error",

    // No single if block inside an else block
    "no-lonely-if": "error",

    // Disallow the use of number literals that immediately lose precision at runtime when converted to JS Number
    "no-loss-of-precision": "error",

    // Nested ternary statements are confusing
    "no-nested-ternary": "error",

    // Use {} instead of new Object()
    "no-new-object": "error",

    // Disallow use of new wrappers
    "no-new-wrappers": "error",

    // We don't want this, see bug 1551829
    "no-prototype-builtins": "off",

    // Disable builtinGlobals for no-redeclare as this conflicts with our
    // globals declarations especially for browser window.
    "no-redeclare": ["error", { builtinGlobals: false }],

    // Disallow use of event global.
    "no-restricted-globals": ["error", "event"],

    // No unnecessary comparisons
    "no-self-compare": "error",

    // No comma sequenced statements
    "no-sequences": "error",

    // No declaring variables from an outer scope
    // "no-shadow": "error",

    // No declaring variables that hide things like arguments
    "no-shadow-restricted-names": "error",

    // Disallow throwing literals (eg. throw "error" instead of
    // throw new Error("error")).
    "no-throw-literal": "error",

    // Disallow the use of Boolean literals in conditional expressions.
    "no-unneeded-ternary": "error",

    // No unsanitized use of innerHTML=, document.write() etc.
    // cf. https://github.com/mozilla/eslint-plugin-no-unsanitized#rule-details
    "no-unsanitized/method": "error",
    "no-unsanitized/property": "error",

    // No declaring variables that are never used
    "no-unused-vars": [
      "error",
      {
        args: "none",
        vars: "local",
      },
    ],

    // No using variables before defined
    // "no-use-before-define": ["error", "nofunc"],

    // Disallow unnecessary .call() and .apply()
    "no-useless-call": "error",

    // Don't concatenate string literals together (unless they span multiple
    // lines)
    "no-useless-concat": "error",

    // XXX Bug 1487642 - decide if we want to enable this or not.
    // Disallow unnecessary escape characters
    "no-useless-escape": "off",

    // Disallow redundant return statements
    "no-useless-return": "error",

    // No using with
    "no-with": "error",

    // Require object-literal shorthand with ES6 method syntax
    "object-shorthand": ["error", "always", { avoidQuotes: true }],

    // This may conflict with prettier, so turn it off.
    "prefer-arrow-callback": "off",

    // This generates too many false positives that are not easy to work around,
    // and false positives seem to be inherent in the rule.
    "require-atomic-updates": "off",

    // XXX Bug 1487642 - decide if we want to enable this or not.
    // Require generator functions to contain yield
    "require-yield": "off",
  },

  // To avoid bad interactions of the html plugin with the xml preprocessor in
  // eslint-plugin-mozilla, we turn off processing of the html plugin for .xml
  // files.
  settings: {
    "html/xml-extensions": [".xhtml"],
  },
};
