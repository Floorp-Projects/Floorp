/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/no-import-into-var-and-global");
var RuleTester = require("eslint/lib/testers/rule-tester");

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

const ExpectedError = {
  message: "Cu.import imports into variables and into global scope.",
  type: "CallExpression"
};

ruleTester.run("no-import-into-var-and-global", rule, {
  valid: [
    "var foo = Cu.import('fake', {});",
    "var foo = Components.utils.import('fake', {});"
  ],
  invalid: [{
    code: "var foo = Cu.import('fake', this);",
    errors: [ExpectedError]
  }, {
    code: "var foo = Cu.import('fake');",
    errors: [ExpectedError]
  }, {
    code: "var foo = Components.utils.import('fake');",
    errors: [ExpectedError]
  }, {
    code: "var foo = ChromeUtils.import('fake');",
    errors: [ExpectedError]
  }]
});
