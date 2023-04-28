/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/reject-osfile");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidError(os, util) {
  let message = `${os} is deprecated. You should use ${util} instead.`;
  return [{ message, type: "MemberExpression" }];
}

ruleTester.run("reject-osfile", rule, {
  valid: ["IOUtils.write()"],
  invalid: [
    {
      code: "OS.File.write()",
      errors: invalidError("OS.File", "IOUtils"),
    },
    {
      code: "lazy.OS.File.write()",
      errors: invalidError("OS.File", "IOUtils"),
    },
  ],
});

ruleTester.run("reject-osfile", rule, {
  valid: ["PathUtils.join()"],
  invalid: [
    {
      code: "OS.Path.join()",
      errors: invalidError("OS.Path", "PathUtils"),
    },
    {
      code: "lazy.OS.Path.join()",
      errors: invalidError("OS.Path", "PathUtils"),
    },
  ],
});
