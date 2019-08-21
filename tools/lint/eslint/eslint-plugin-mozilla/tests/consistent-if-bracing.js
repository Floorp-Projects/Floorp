/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/consistent-if-bracing");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 8 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code) {
  return { code, errors: [{ messageId: "consistentIfBracing" }] };
}

ruleTester.run("consistent-if-bracing", rule, {
  valid: [
    "if (true) {1} else {0}",
    "if (false) 1; else 0",
    "if (true) {1} else if (true) {2} else {0}",
    "if (true) {1} else if (true) {2} else if (true) {3} else {0}",
  ],
  invalid: [
    invalidCode(`if (true) {1} else 0`),
    invalidCode("if (true) 1; else {0}"),
    invalidCode("if (true) {1} else if (true) 2; else {0}"),
    invalidCode("if (true) 1; else if (true) {2} else {0}"),
    invalidCode("if (true) {1} else if (true) 2; else {0}"),
    invalidCode("if (true) {1} else if (true) {2} else 0"),
    invalidCode("if (true) {1} else if (true) {2} else if (true) 3; else {0}"),
    invalidCode("if (true) {if (true) 1; else {0}} else {0}"),
  ],
});
