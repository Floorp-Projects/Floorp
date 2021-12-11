/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/reject-importGlobalProperties");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 8 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

ruleTester.run("reject-importGlobalProperties", rule, {
  valid: [
    {
      code: "Cu.something();",
    },
    {
      options: ["allownonwebidl"],
      code: "Cu.importGlobalProperties(['fetch'])",
    },
  ],
  invalid: [
    {
      code: "Cu.importGlobalProperties(['fetch'])",
      options: ["everything"],
      errors: [{ messageId: "unexpectedCall" }],
    },
    {
      code: "Cu.importGlobalProperties(['TextEncoder'])",
      options: ["everything"],
      errors: [{ messageId: "unexpectedCall" }],
    },
    {
      code: "Cu.importGlobalProperties(['TextEncoder'])",
      options: ["allownonwebidl"],
      errors: [{ messageId: "unexpectedCallWebIdl" }],
    },
  ],
});
