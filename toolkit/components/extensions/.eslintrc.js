"use strict";

module.exports = { // eslint-disable-line no-undef
  "extends": "../../.eslintrc.js",

  "globals": {
    "Cc": true,
    "Ci": true,
    "Components": true,
    "Cr": true,
    "Cu": true,
    "dump": true,
    "TextDecoder": false,
    "TextEncoder": false,
    // Specific to WebExtensions:
    "Extension": true,
    "ExtensionManagement": true,
    "extensions": true,
    "getContainerForCookieStoreId": true,
    "getCookieStoreIdForContainer": true,
    "global": true,
    "isContainerCookieStoreId": true,
    "isDefaultCookieStoreId": true,
    "isPrivateCookieStoreId": true,
    "isValidCookieStoreId": true,
    "NetUtil": true,
    "openOptionsPage": true,
    "require": false,
    "runSafe": true,
    "runSafeSync": true,
    "runSafeSyncWithoutClone": true,
    "Services": true,
    "TabManager": true,
    "WindowListManager": true,
    "XPCOMUtils": true,
  },

  "rules": {
    // Rules from the mozilla plugin
    "mozilla/balanced-listeners": "error",
    "mozilla/no-aArgs": "error",
    "mozilla/no-cpows-in-tests": "warn",
    "mozilla/var-only-at-top-level": "warn",

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

    // Braces only needed for multi-line arrow function blocks
    // "arrow-body-style": ["error", "as-needed"],

    // Require spacing around =>
    "arrow-spacing": "error",

    // Always require spacing around a single line block
    "block-spacing": "warn",

    // Forbid spaces inside the square brackets of array literals.
    "array-bracket-spacing": ["error", "never"],

    // Forbid spaces inside the curly brackets of object literals.
    "object-curly-spacing": ["error", "never"],

    // No space padding in parentheses
    "space-in-parens": ["error", "never"],

    // Enforce one true brace style (opening brace on the same line) and avoid
    // start and end braces on the same line.
    "brace-style": ["error", "1tbs", {"allowSingleLine": true}],

    // No space before always a space after a comma
    "comma-spacing": ["error", {"before": false, "after": true}],

    // Commas at the end of the line not the start
    "comma-style": "error",

    // Don't require spaces around computed properties
    "computed-property-spacing": ["error", "never"],

    // Functions are not required to consistently return something or nothing
    "consistent-return": "off",

    // Require braces around blocks that start a new line
    "curly": ["error", "all"],

    // Always require a trailing EOL
    "eol-last": "error",

    // Require function* name()
    "generator-star-spacing": ["error", {"before": false, "after": true}],

    // Two space indent
    "indent": ["error", 2, {"SwitchCase": 1, "ArrayExpression": "first", "ObjectExpression": "first"}],

    // Space after colon not before in property declarations
    "key-spacing": ["error", {"beforeColon": false, "afterColon": true, "mode": "minimum"}],

    // Require spaces before and after finally, catch, etc.
    "keyword-spacing": "error",

    // Unix linebreaks
    "linebreak-style": ["error", "unix"],

    // Always require parenthesis for new calls
    "new-parens": "error",

    // Use [] instead of Array()
    "no-array-constructor": "error",

    // No duplicate arguments in function declarations
    "no-dupe-args": "error",

    // No duplicate keys in object declarations
    "no-dupe-keys": "error",

    // No duplicate cases in switch statements
    "no-duplicate-case": "error",

    // If an if block ends with a return no need for an else block
    // "no-else-return": "error",

    // Disallow empty statements. This will report an error for:
    // try { something(); } catch (e) {}
    // but will not report it for:
    // try { something(); } catch (e) { /* Silencing the error because ...*/ }
    // which is a valid use case.
    "no-empty": "error",

    // No empty character classes in regex
    "no-empty-character-class": "error",

    // Disallow empty destructuring
    "no-empty-pattern": "error",

    // No assiging to exception variable
    "no-ex-assign": "error",

    // No using !! where casting to boolean is already happening
    "no-extra-boolean-cast": "warn",

    // No double semicolon
    "no-extra-semi": "error",

    // No overwriting defined functions
    "no-func-assign": "error",

    // No invalid regular expresions
    "no-invalid-regexp": "error",

    // No odd whitespace characters
    "no-irregular-whitespace": "error",

    // No single if block inside an else block
    "no-lonely-if": "warn",

    // No mixing spaces and tabs in indent
    "no-mixed-spaces-and-tabs": ["error", "smart-tabs"],

    // Disallow use of multiple spaces (sometimes used to align const values,
    // array or object items, etc.). It's hard to maintain and doesn't add that
    // much benefit.
    "no-multi-spaces": "warn",

    // No reassigning native JS objects
    "no-native-reassign": "error",

    // Nested ternary statements are confusing
    "no-nested-ternary": "error",

    // Use {} instead of new Object()
    "no-new-object": "error",

    // No Math() or JSON()
    "no-obj-calls": "error",

    // No octal literals
    "no-octal": "error",

    // No redeclaring variables
    "no-redeclare": "error",

    // No unnecessary comparisons
    "no-self-compare": "error",

    // No spaces between function name and parentheses
    "no-spaced-func": "warn",

    // No trailing whitespace
    "no-trailing-spaces": "error",

    // Error on newline where a semicolon is needed
    "no-unexpected-multiline": "error",

    // No unreachable statements
    "no-unreachable": "error",

    // No expressions where a statement is expected
    "no-unused-expressions": "error",

    // No declaring variables that are never used
    "no-unused-vars": ["error", {"args": "none", "varsIgnorePattern": "^(Cc|Ci|Cr|Cu|EXPORTED_SYMBOLS)$"}],

    // No using variables before defined
    "no-use-before-define": "error",

    // No using with
    "no-with": "error",

    // Always require semicolon at end of statement
    "semi": ["error", "always"],

    // Require space before blocks
    "space-before-blocks": "error",

    // Never use spaces before function parentheses
    "space-before-function-paren": ["error", {"anonymous": "never", "named": "never"}],

    // Require spaces around operators, except for a|0.
    "space-infix-ops": ["error", {"int32Hint": true}],

    // ++ and -- should not need spacing
    "space-unary-ops": ["warn", {"nonwords": false, "words": true, "overrides": {"typeof": false}}],

    // No comparisons to NaN
    "use-isnan": "error",

    // Only check typeof against valid results
    "valid-typeof": "error",

    // Disallow using variables outside the blocks they are defined (especially
    // since only let and const are used, see "no-var").
    "block-scoped-var": "error",

    // Allow trailing commas for easy list extension.  Having them does not
    // impair readability, but also not required either.
    "comma-dangle": ["error", "always-multiline"],

    // Warn about cyclomatic complexity in functions.
    "complexity": "warn",

    // Don't warn for inconsistent naming when capturing this (not so important
    // with auto-binding fat arrow functions).
    // "consistent-this": ["error", "self"],

    // Don't require a default case in switch statements. Avoid being forced to
    // add a bogus default when you know all possible cases are handled.
    "default-case": "off",

    // Enforce dots on the next line with property name.
    "dot-location": ["error", "property"],

    // Encourage the use of dot notation whenever possible.
    "dot-notation": "error",

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

    // Don't enforce the maximum depth that blocks can be nested. The complexity
    // rule is a better rule to check this.
    "max-depth": "off",

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

    // Disallow use of arguments.caller or arguments.callee.
    "no-caller": "error",

    // Disallow the catch clause parameter name being the same as a variable in
    // the outer scope, to avoid confusion.
    "no-catch-shadow": "off",

    // Disallow assignment in conditional expressions.
    "no-cond-assign": "error",

    // Disallow using the console API.
    "no-console": "error",

    // Allow using constant expressions in conditions like while (true)
    "no-constant-condition": "off",

    // Allow use of the continue statement.
    "no-continue": "off",

    // Disallow control characters in regular expressions.
    "no-control-regex": "error",

    // Disallow use of debugger.
    "no-debugger": "error",

    // Disallow deletion of variables (deleting properties is fine).
    "no-delete-var": "error",

    // Allow division operators explicitly at beginning of regular expression.
    "no-div-regex": "off",

    // Disallow use of eval(). We have other APIs to evaluate code in content.
    "no-eval": "error",

    // Disallow adding to native types
    "no-extend-native": "error",

    // Disallow unnecessary function binding.
    "no-extra-bind": "error",

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
    "no-multi-str": "warn",

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

    // Disallow multiple spaces in a regular expression literal.
    "no-regex-spaces": "error",

    // Allow reserved words being used as object literal keys.
    "no-reserved-keys": "off",

    // Don't restrict usage of specified node modules (not a node environment).
    "no-restricted-modules": "off",

    // Disallow use of assignment in return statement. It is preferable for a
    // single line of code to have only one easily predictable effect.
    "no-return-assign": "error",

    // Don't warn about declaration of variables already declared in the outer scope.
    "no-shadow": "off",

    // Disallow shadowing of names such as arguments.
    "no-shadow-restricted-names": "error",

    // Allow use of synchronous methods (not a node environment).
    "no-sync": "off",

    // Allow the use of ternary operators.
    "no-ternary": "off",

    // Disallow throwing literals (eg. throw "error" instead of
    // throw new Error("error")).
    "no-throw-literal": "error",

    // Disallow use of undeclared variables unless mentioned in a /* global */
    // block. Note that globals from head.js are automatically imported in tests
    // by the import-headjs-globals rule form the mozilla eslint plugin.
    "no-undef": "error",

    // Allow dangling underscores in identifiers (for privates).
    "no-underscore-dangle": "off",

    // Allow use of undefined variable.
    "no-undefined": "off",

    // Disallow the use of Boolean literals in conditional expressions.
    "no-unneeded-ternary": "error",

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
    "padded-blocks": ["warn", "never"],

    // Don't require quotes around object literal property names.
    "quote-props": "off",

    // Double quotes should be used.
    "quotes": ["warn", "double", {"avoidEscape": true, "allowTemplateLiterals": true}],

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

    // disallow use of eval()-like methods
    "no-implied-eval": "error",

    // Disallow function or variable declarations in nested blocks
    "no-inner-declarations": "error",

    // Disallow usage of __iterator__ property
    "no-iterator": "error",

    // Disallow labels that share a name with a variable
    "no-label-var": "error",

    // Disallow creating new instances of String, Number, and Boolean
    "no-new-wrappers": "error",
  },
};
