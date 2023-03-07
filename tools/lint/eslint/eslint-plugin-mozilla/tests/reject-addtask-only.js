/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/reject-addtask-only");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidError(output) {
  let message =
    "add_task(...).only() not allowed - add an exception if this is intentional";
  return [
    {
      message,
      type: "CallExpression",
      suggestions: [{ desc: "Remove only() call from task", output }],
    },
  ];
}

ruleTester.run("reject-addtask-only", rule, {
  valid: [
    "add_task(foo())",
    "add_task(foo()).skip()",
    "add_task(function() {})",
    "add_task(function() {}).skip()",
  ],
  invalid: [
    {
      code: "add_task(foo()).only()",
      errors: invalidError("add_task(foo())"),
    },
    {
      code: "add_task(foo()).only(bar())",
      errors: invalidError("add_task(foo())"),
    },
    {
      code: "add_task(function() {}).only()",
      errors: invalidError("add_task(function() {})"),
    },
    {
      code: "add_task(function() {}).only(bar())",
      errors: invalidError("add_task(function() {})"),
    },
  ],
});
