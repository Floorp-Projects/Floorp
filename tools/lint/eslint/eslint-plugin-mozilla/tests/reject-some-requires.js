/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/reject-some-requires");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function requirePathError(path) {
  return [
    { messageId: "rejectRequire", data: { path }, type: "CallExpression" },
  ];
}

const DEVTOOLS_FORBIDDEN_PATH = "^(resource://)?devtools/forbidden";

ruleTester.run("reject-some-requires", rule, {
  valid: [
    {
      code: 'require("devtools/not-forbidden/path")',
      options: [DEVTOOLS_FORBIDDEN_PATH],
    },
    {
      code: 'require("resource://devtools/not-forbidden/path")',
      options: [DEVTOOLS_FORBIDDEN_PATH],
    },
  ],
  invalid: [
    {
      code: 'require("devtools/forbidden/path")',
      errors: requirePathError("devtools/forbidden/path"),
      options: [DEVTOOLS_FORBIDDEN_PATH],
    },
    {
      code: 'require("resource://devtools/forbidden/path")',
      errors: requirePathError("resource://devtools/forbidden/path"),
      options: [DEVTOOLS_FORBIDDEN_PATH],
    },
  ],
});
