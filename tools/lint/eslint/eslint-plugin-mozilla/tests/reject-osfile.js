/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/reject-osfile");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 8 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidError() {
  let message = "OS.File is deprecated. You should use IOUtils instead.";
  return [{ message, type: "MemberExpression" }];
}

ruleTester.run("reject-osfile", rule, {
  valid: ["IOUtils.write()"],
  invalid: [
    {
      code: "OS.File.write()",
      errors: invalidError(),
    },
  ],
});
