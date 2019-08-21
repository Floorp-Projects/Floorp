/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/use-includes-instead-of-indexOf");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code) {
  let message = "use .includes instead of .indexOf";
  return { code, errors: [{ message, type: "BinaryExpression" }] };
}

ruleTester.run("use-includes-instead-of-indexOf", rule, {
  valid: [
    "let a = foo.includes(bar);",
    "let a = foo.indexOf(bar) > 0;",
    "let a = foo.indexOf(bar) != 0;",
  ],
  invalid: [
    invalidCode("let a = foo.indexOf(bar) >= 0;"),
    invalidCode("let a = foo.indexOf(bar) != -1;"),
    invalidCode("let a = foo.indexOf(bar) !== -1;"),
    invalidCode("let a = foo.indexOf(bar) == -1;"),
    invalidCode("let a = foo.indexOf(bar) === -1;"),
    invalidCode("let a = foo.indexOf(bar) < 0;"),
  ],
});
