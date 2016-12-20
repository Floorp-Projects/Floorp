"use strict";

module.exports = { // eslint-disable-line no-undef
  "rules": {
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

    // Require braces around blocks that start a new line
    "curly": ["error", "multi-line"],

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

    // Disallow modifying variables of class declarations.
    "no-class-assign": "error",

    // Disallow assignment in conditional expressions, except if the assignment
    // is within parentheses.
    "no-cond-assign": ["error", "except-parens"],

    // Disallow use of the console API.
    "no-console": "error",

    // Disallow modifying variables that are declared using const.
    "no-const-assign": "error",

    // Disallow use of debugger
    "no-debugger": "error",

    // Disallow deletion of variables (deleting properties is fine though).
    "no-delete-var": "error",

    // No duplicate arguments in function declarations
    "no-dupe-args": "error",

    // No duplicate keys in object declarations
    "no-dupe-keys": "error",

    // No duplicate cases in switch statements
    "no-duplicate-case": "error",

    // Disallow use of eval().
    "no-eval": "error",

    // No labels
    "no-labels": "error",

    // If an if block ends with a return no need for an else block
    "no-else-return": "error",

    // No empty character classes in regex
    "no-empty-character-class": "error",

    // Disallow empty destructuring
    "no-empty-pattern": "error",

    // No assigning to exception variable
    "no-ex-assign": "error",

    // No using !! where casting to boolean is already happening
    "no-extra-boolean-cast": "error",

    // No double semicolon
    "no-extra-semi": "error",

    // No overwriting defined functions
    "no-func-assign": "error",

    // No reassigning native JS objects or read only globals.
    "no-global-assign": "error",

    // No invalid regular expressions
    "no-invalid-regexp": "error",

    // No odd whitespace characters
    "no-irregular-whitespace": "error",

    // No single if block inside an else block
    "no-lonely-if": "error",

    // No mixing spaces and tabs in indent
    "no-mixed-spaces-and-tabs": ["error", "smart-tabs"],

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

    // No declaring variables that hide things like arguments
    "no-shadow-restricted-names": "error",

    // Disallow sparse arrays, eg. let arr = [,,"error"].
    "no-sparse-arrays": "error",

    // Disallow throwing literals (eg. |throw "error"| instead of
    // |throw new Error("error")|)
    "no-throw-literal": "error",

    // No trailing whitespace
    "no-trailing-spaces": "error",

    // No using undeclared variables
    "no-undef": "error",

    // Error on newline where a semicolon is needed
    "no-unexpected-multiline": "error",

    // No unreachable statements
    "no-unreachable": "error",

    // No expressions where a statement is expected
    "no-unused-expressions": "error",

    // No using with
    "no-with": "error",

    // Always require semicolon at end of statement
    "semi": ["error", "always"],

    // Require space before blocks
    "space-before-blocks": "error",

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

    // Only check typeof against valid results
    "valid-typeof": "error"
  },
  "env": {
    "browser": true
  },
  "globals": {
    "Components": false,
    "dump": false
  }
};
