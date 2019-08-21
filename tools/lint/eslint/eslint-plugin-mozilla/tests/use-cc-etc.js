/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/use-cc-etc");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, originalName, newName) {
  return {
    code,
    errors: [
      {
        message: `Use ${newName} rather than ${originalName}`,
        type: "MemberExpression",
      },
    ],
  };
}

ruleTester.run("use-cc-etc", rule, {
  valid: ["Components.Constructor();", "let x = Components.foo;"],
  invalid: [
    invalidCode(
      "let foo = Components.classes['bar'];",
      "Components.classes",
      "Cc"
    ),
    invalidCode(
      "let bar = Components.interfaces.bar;",
      "Components.interfaces",
      "Ci"
    ),
    invalidCode(
      "Components.results.NS_ERROR_ILLEGAL_INPUT;",
      "Components.results",
      "Cr"
    ),
    invalidCode(
      "Components.utils.reportError('fake');",
      "Components.utils",
      "Cu"
    ),
  ],
});
