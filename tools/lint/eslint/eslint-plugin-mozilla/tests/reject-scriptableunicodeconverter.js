/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/reject-scriptableunicodeconverter");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidError() {
  return [
    { messageId: "rejectScriptableUnicodeConverter", type: "MemberExpression" },
  ];
}

ruleTester.run("reject-scriptableunicodeconverter", rule, {
  valid: ["TextEncoder", "TextDecoder"],
  invalid: [
    {
      code: "Ci.nsIScriptableUnicodeConverter",
      errors: invalidError(),
    },
  ],
});
