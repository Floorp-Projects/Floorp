/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/avoid-Date-timing");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, type, message) {
  return { code, errors: [{ messageId: "usePerfNow", type }] };
}

ruleTester.run("avoid-Date-timing", rule, {
  valid: [
    "new Date('2017-07-11');",
    "new Date(1499790192440);",
    "new Date(2017, 7, 11);",
    "Date.UTC(2017, 7);",
  ],
  invalid: [
    invalidCode("Date.now();", "CallExpression"),
    invalidCode("new Date();", "NewExpression"),
  ],
});
