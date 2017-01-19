"use strict";

module.exports = { // eslint-disable-line no-undef
  "rules": {
    // Enforce return statements in callbacks of array methods.
    "array-callback-return": "error",

    // Braces only needed for multi-line arrow function blocks
    "arrow-body-style": ["error", "as-needed"],

    // Require spacing around =>
    "arrow-spacing": "error",

    // Always require spacing around a single line block
    "block-spacing": "error",

    // No space before always a space after a comma
    "comma-spacing": ["error", {"before": false, "after": true}],

    // Commas at the end of the line not the start
    "comma-style": "error",

    // Don't require spaces around computed properties
    "computed-property-spacing": ["error", "never"],

    // Functions must always return something or nothing
    "consistent-return": "error",

    // Verify calls of super() in constructors.
    "constructor-super": "error",

    // Require braces around blocks that start a new line
    "curly": ["error", "multi-line"],

    // Require default case in switch statements.
    "default-case": "error",

    // Require `foo.bar` dot notation instead of `foo["bar"]` notation.
    "dot-notation": "error",

    // Always require a trailing EOL
    "eol-last": "error",

    // No spaces between function name and parentheses.
    "func-call-spacing": ["error", "never"],

    // Require function* name()
    "generator-star-spacing": ["error", {"before": false, "after": true}],

    // Space after colon not before in property declarations
    "key-spacing": ["error", { "beforeColon": false, "afterColon": true, "mode": "minimum" }],

    // Require spaces around keywords
    "keyword-spacing": "error",

    // Unix linebreaks
    "linebreak-style": ["error", "unix"],

    // Always require parenthesis for new calls
    "new-parens": "error",

    // Disallow use of alert(), confirm(), and prompt().
    "no-alert": "error",

    // Use [] instead of Array()
    "no-array-constructor": "error",

    // Disallow use of arguments.caller or arguments.callee.
    "no-caller": "error",

    // Disallow likely erroneous `switch` scoped lexical declarations in
    // case/default clauses.
    "no-case-declarations": "error",

    // Disallow modifying variables of class declarations.
    "no-class-assign": "error",

    // Disallow assignment in conditional expressions, except if the assignment
    // is within parentheses.
    "no-cond-assign": ["error", "except-parens"],

    // Disallow use of the console API.
    "no-console": "error",

    // Disallow modifying variables that are declared using const.
    "no-const-assign": "error",

    // Disallow constant expressions in conditions (except for loops).
    "no-constant-condition": ["error", { "checkLoops": false }],

    // Disallow control characters in regular expressions.
    "no-control-regex": "error",

    // Disallow use of debugger
    "no-debugger": "error",

    // Disallow deletion of variables (deleting properties is fine though).
    "no-delete-var": "error",

    // No duplicate arguments in function declarations
    "no-dupe-args": "error",

    // Disallow duplicate class members.
    "no-dupe-class-members": "error",

    // No duplicate keys in object declarations
    "no-dupe-keys": "error",

    // No duplicate cases in switch statements
    "no-duplicate-case": "error",

    // If an if block ends with a return no need for an else block
    "no-else-return": "error",

    // No empty statements (except for catch blocks).
    "no-empty": ["error", { "allowEmptyCatch": true }],

    // No empty character classes in regex
    "no-empty-character-class": "error",

    // Disallow empty destructuring
    "no-empty-pattern": "error",

    // Disallow use of eval().
    "no-eval": "error",

    // No assigning to exception variable
    "no-ex-assign": "error",

    // Disallow extending of native objects.
    "no-extend-native": "error",

    // Disallow unnecessary function binding.
    "no-extra-bind": "error",

    // No using !! where casting to boolean is already happening
    "no-extra-boolean-cast": "error",

    // No double semicolon
    "no-extra-semi": "error",

    // Disallow case statement fallthrough without explicit `// falls through`
    // annotation.
    "no-fallthrough": "error",

    // No overwriting defined functions
    "no-func-assign": "error",

    // No reassigning native JS objects or read only globals.
    "no-global-assign": "error",

    // Disallow implied eval().
    "no-implied-eval": "error",

    // No invalid regular expressions
    "no-invalid-regexp": "error",

    // No odd whitespace characters
    "no-irregular-whitespace": "error",

    // No labels.
    "no-labels": "error",

    // No single if block inside an else block
    "no-lonely-if": "error",

    // No unnecessary spacing
    "no-multi-spaces": ["error", { "exceptions": {
      "AssignmentExpression": true,
      "VariableDeclarator": true,
      "ArrayExpression": true,
      "ObjectExpression": true
    }}],

    // Nested ternary statements are confusing
    "no-nested-ternary": "error",

    // Use {} instead of new Object()
    "no-new-object": "error",

    // Disallow primitive wrapper instances like `new Boolean(false)`, which
    // seem like they should act like primitives but don't.
    "no-new-wrappers": "error",

    // No Math() or JSON()
    "no-obj-calls": "error",

    // No octal literals
    "no-octal": "error",

    // No redeclaring variables
    "no-redeclare": "error",

    // Disallow use of assignment in return statement.
    "no-return-assign": ["error", "always"],

    // Disallow self assignment such as |foo = foo|.
    "no-self-assign": "error",

    // No unnecessary comparisons
    "no-self-compare": "error",

    // Disallow use of the comma operator.
    "no-sequences": "error",

    // No declaring variables that hide things like arguments
    "no-shadow-restricted-names": "error",

    // Disallow sparse arrays, eg. let arr = [,,"error"].
    "no-sparse-arrays": "error",

    // Disallow tabs.
    "no-tabs": "error",

    // Disallow template literal placeholder syntax in regular strings.
    "no-template-curly-in-string": "error",

    // Disallow use of this/super before calling super() in constructors.
    "no-this-before-super": "error",

    // Disallow throwing literals (eg. |throw "error"| instead of
    // |throw new Error("error")|)
    "no-throw-literal": "error",

    // No trailing whitespace
    "no-trailing-spaces": "error",

    // No using undeclared variables
    "no-undef": "error",

    // Error on newline where a semicolon is needed
    "no-unexpected-multiline": "error",

    // Disallow unmodified loop conditions.
    "no-unmodified-loop-condition": "error",

    // Disallow ternary operators when simpler alternatives exist.
    "no-unneeded-ternary": "error",

    // No unreachable statements
    "no-unreachable": "error",

    // Disallow unsafe control flow statements in finally blocks.
    "no-unsafe-finally": "error",

    // No expressions where a statement is expected
    "no-unused-expressions": "error",

    // Disallow unnecessary escape usage in strings and regular expressions.
    "no-useless-escape": "error",

    // Disallow whitespace before properties.
    "no-whitespace-before-property": "error",

    // No using with
    "no-with": "error",

    // Disallow blank line padding within blocks.
    "padded-blocks": ["error", "never"],

    // Require double quote strings to be used, except cases where another quote
    // type is used to avoid escaping.
    "quotes": ["error", "double", { "avoidEscape": true }],

    // Always require semicolon at end of statement
    "semi": ["error", "always"],

    // Enforce spacing after semicolons.
    "semi-spacing": ["error", { "before": false, "after": true }],

    // Require space before blocks
    "space-before-blocks": "error",

    // Never use spaces before named function parentheses, but always for async
    // arrow functions.
    "space-before-function-paren": ["error", {
      "anonymous": "ignore",
      "asyncArrow": "always",
      "named": "never",
    }],

    // No space padding in parentheses
    "space-in-parens": ["error", "never"],

    // Require spaces around operators
    "space-infix-ops": "error",

    // ++ and -- should not need spacing
    "space-unary-ops": ["error", { "words": true, "nonwords": false }],

    // Require "use strict" to be defined globally in the script.
    "strict": ["error", "global"],

    // No comparisons to NaN
    "use-isnan": "error",

    // Enforce valid JSDoc comments.
    "valid-jsdoc": ["error", {
      "requireParamDescription": false,
      "requireReturn": false,
      "requireReturnDescription": false,
    }],

    // Only check typeof against valid results
    "valid-typeof": "error",

    // Disallow Yoda conditions.
    "yoda": ["error", "never"],
  },
  "env": {
    "browser": true
  },
  "globals": {
    "Components": false,
    "dump": false
  }
};
