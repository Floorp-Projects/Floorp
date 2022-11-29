/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/no-cu-reportError");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 9 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function callError() {
  return [{ messageId: "useConsoleError", type: "CallExpression" }];
}

ruleTester.run("no-cu-reportError", rule, {
  valid: [
    "console.error('foo')",
    "Cu.cloneInto({}, {})",
    "foo().catch(console.error)",
    // TODO: Bug 1802347 - this should be treated as an error as well.
    "Cu.reportError('foo', stack)",
  ],
  invalid: [
    {
      code: "Cu.reportError('foo')",
      output: "console.error('foo')",
      errors: callError(),
    },
    {
      code: "Cu.reportError(bar)",
      output: "console.error(bar)",
      errors: callError(),
    },
    {
      code: "foo().catch(Cu.reportError)",
      output: "foo().catch(console.error)",
      errors: callError(),
    },
  ],
});
