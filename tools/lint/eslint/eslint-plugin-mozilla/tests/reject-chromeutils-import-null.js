/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/reject-chromeutils-import-null");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 8 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidError() {
  let message =
    "ChromeUtils.import should not be called with (..., null) to " +
    "retrieve the JSM global object. Rely on explicit exports instead.";
  return [{ message, type: "CallExpression" }];
}

ruleTester.run("reject-chromeutils-import-null", rule, {
  valid: ['ChromeUtils.import("resource://some/path/to/My.jsm")'],
  invalid: [
    {
      code: 'ChromeUtils.import("resource://some/path/to/My.jsm", null)',
      errors: invalidError(),
    },
    {
      code: `
ChromeUtils.import(
  "resource://some/path/to/My.jsm",
  null
);`,
      errors: invalidError(),
    },
  ],
});
