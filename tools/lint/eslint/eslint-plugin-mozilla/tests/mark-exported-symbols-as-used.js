/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/mark-exported-symbols-as-used");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, type, messageId) {
  return { code, errors: [{ messageId, type }] };
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
      "useLetForExported"
    ),
    invalidCode(
      "var EXPORTED_SYMBOLS = 'foo';",
      "VariableDeclaration",
      "nonArrayAssignedToImported"
    ),
    invalidCode(
      "this.EXPORTED_SYMBOLS = 'foo';",
      "AssignmentExpression",
      "nonArrayAssignedToImported"
    ),
  ],
});
