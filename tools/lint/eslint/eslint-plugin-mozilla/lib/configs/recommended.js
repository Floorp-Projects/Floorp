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
    es6: true,
    "mozilla/privileged": true,
  },

  extends: ["eslint:recommended", "plugin:prettier/recommended"],

  globals: {
    Cc: false,
    // Specific to Firefox (Chrome code only).
    ChromeUtils: false,
    Ci: false,
    Components: false,
    Cr: false,
    Cu: false,
    Debugger: false,
    InstallTrigger: false,
    // Specific to Firefox
    // https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/InternalError
    InternalError: true,
    Intl: false,
    SharedArrayBuffer: false,
    StopIteration: false,
    dump: true,
    // Override the "browser" env definition of "location" to allow writing as it
    // is a writeable property.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1509270#c1 for more information.
    location: true,
    openDialog: false,
    saveStack: false,
    sizeToContent: false,
    // Specific to Firefox
    // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/uneval
    uneval: false,
  },

  overrides: [
    {
      // Turn off use-services for xml files. XBL bindings are going away, and
      // working out the valid globals for those is difficult.
      files: "**/*.xml",
      rules: {
        "mozilla/use-services": "off",
      },
    },
    {
      // We don't have the general browser environment for jsm files, but we do
      // have our own special environments for them.
      env: {
        browser: false,
        "mozilla/jsm": true,
      },
      files: "**/*.jsm",
      rules: {
        "mozilla/mark-exported-symbols-as-used": "error",
        // JSM modules are far easier to check for no-unused-vars on a global scope,
        // than our content files. Hence we turn that on here.
        "no-unused-vars": [
          "error",
          {
            args: "none",
            vars: "all",
          },
        ],
      },
    },
  ],

  parserOptions: {
    ecmaVersion: 9,
  },

  // When adding items to this file please check for effects on sub-directories.
  plugins: ["html", "fetch-options", "no-unsanitized"],

  // When adding items to this file please check for effects on all of toolkit
  // and browser
  rules: {
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
    "mozilla/no-define-cc-etc": "error",
    "mozilla/no-useless-parameters": "error",
    "mozilla/no-useless-removeEventListener": "error",
    "mozilla/reject-importGlobalProperties": ["error", "allownonwebidl"],
    "mozilla/rejects-requires-await": "error",
    "mozilla/use-cc-etc": "error",
    "mozilla/use-chromeutils-generateqi": "error",
    "mozilla/use-chromeutils-import": "error",
    "mozilla/use-default-preference-values": "error",
    "mozilla/use-includes-instead-of-indexOf": "error",
    "mozilla/use-ownerGlobal": "error",
    "mozilla/use-returnValue": "error",
    "mozilla/use-services": "error",

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

    // XXX Bug 1487642 - decide if we want to enable this or not.
    // Disallow fallthrough of case statements
    "no-fallthrough": "off",

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

    // Nested ternary statements are confusing
    "no-nested-ternary": "error",

    // Use {} instead of new Object()
    "no-new-object": "error",

    // Disallow use of new wrappers
    "no-new-wrappers": "error",

    // Disallow use of event global.
    "no-restricted-globals": ["error", "event"],

    // Disallows unnecessary `return await ...`.
    "no-return-await": "error",

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
