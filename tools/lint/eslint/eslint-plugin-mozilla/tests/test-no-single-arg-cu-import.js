/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/no-single-arg-cu-import");
var RuleTester = require("eslint/lib/testers/rule-tester");

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

const ExpectedError = {
  message: "Single argument Cu.import exposes new globals to all modules",
  type: "CallExpression"
};

ruleTester.run("no-single-arg-cu-import", rule, {
  valid: [
    "Cu.import('fake', {});"
  ],
  invalid: [{
    code: "Cu.import('fake');",
    errors: [ExpectedError]
  }, {
    code: "ChromeUtils.import('fake');",
    errors: [ExpectedError]
  }]
});
