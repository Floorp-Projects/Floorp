/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/reject-chromeutils-import-params");
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

ruleTester.run("reject-chromeutils-import-params", rule, {
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
    {
      code: 'ChromeUtils.import("resource://some/path/to/My.jsm", this)',
      errors: [
        {
          suggestions: [
            {
              desc: "Use destructuring for imports.",
              output: `const { My } = ChromeUtils.import("resource://some/path/to/My.jsm")`,
            },
          ],
        },
      ],
    },
    {
      code: 'ChromeUtils.import("resource://some/path/to/My.js", this)',
      errors: [
        {
          suggestions: [
            {
              desc: "Use destructuring for imports.",
              output: `const { My } = ChromeUtils.import("resource://some/path/to/My.js")`,
            },
          ],
        },
      ],
    },
    {
      code: `
ChromeUtils.import(
  "resource://some/path/to/My.jsm",
  this
);`,
      errors: [
        {
          suggestions: [
            {
              desc: "Use destructuring for imports.",
              output: `
const { My } = ChromeUtils.import("resource://some/path/to/My.jsm");`,
            },
          ],
        },
      ],
    },
  ],
});
