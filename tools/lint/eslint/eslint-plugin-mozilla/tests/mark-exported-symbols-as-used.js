/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/mark-exported-symbols-as-used");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, type, message) {
  return { code, errors: [{ message, type }] };
}

ruleTester.run("mark-exported-symbols-as-used", rule, {
  valid: [
    "var EXPORTED_SYMBOLS = ['foo'];",
    "this.EXPORTED_SYMBOLS = ['foo'];",
  ],
  invalid: [
    invalidCode(
      "let EXPORTED_SYMBOLS = ['foo'];",
      "VariableDeclaration",
      "EXPORTED_SYMBOLS cannot be declared via `let`. Use `var` or `this.EXPORTED_SYMBOLS =`"
    ),
    invalidCode(
      "var EXPORTED_SYMBOLS = 'foo';",
      "VariableDeclaration",
      "Unexpected assignment of non-Array to EXPORTED_SYMBOLS"
    ),
    invalidCode(
      "this.EXPORTED_SYMBOLS = 'foo';",
      "AssignmentExpression",
      "Unexpected assignment of non-Array to EXPORTED_SYMBOLS"
    ),
  ],
});
