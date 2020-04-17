/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/use-finally");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

let gError = [
  {
    message: `Use .finally instead of passing 2 identical arguments to .then`,
    type: "CallExpression",
  },
];

ruleTester.run("use-finally", rule, {
  valid: ["foo.finally(a)", "foo.then(a)", "foo.then(() => {})"],
  invalid: [
    {
      code: "foo.then(a, a);",
      output: "foo.finally(a);",
      errors: gError,
    },
    // ignore whitespace:
    {
      code: "foo.then(() =>   {}, () => {});",
      output: "foo.finally(() => {});",
      errors: gError,
    },
    // keep comments:
    {
      code: "foo.then(a, /* make sure we always do this */ a);",
      output: "foo.finally(/* make sure we always do this */ a);",
      errors: gError,
    },
  ],
});
