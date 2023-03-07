/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/no-cu-reportError");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

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
      code: "Cu.reportError(bar.stack)",
      output: "console.error(bar.stack)",
      errors: callError(),
    },
    {
      code: "foo().catch(Cu.reportError)",
      output: "foo().catch(console.error)",
      errors: callError(),
    },
    {
      code: "foo().then(bar, Cu.reportError)",
      output: "foo().then(bar, console.error)",
      errors: callError(),
    },
    // When referencing identifiers/members, try to reference them rather
    // than stringifying:
    {
      code: "Cu.reportError('foo' + e)",
      output: "console.error('foo', e)",
      errors: callError(),
    },
    {
      code: "Cu.reportError('foo' + msg.data)",
      output: "console.error('foo', msg.data)",
      errors: callError(),
    },
    // Don't touch existing concatenation of literals (usually done for
    // wrapping reasons)
    {
      code: "Cu.reportError('foo' + 'bar' + 'baz')",
      output: "console.error('foo' + 'bar' + 'baz')",
      errors: callError(),
    },
    // Ensure things work when nested:
    {
      code: "Cu.reportError('foo' + e + 'baz')",
      output: "console.error('foo', e, 'baz')",
      errors: callError(),
    },
    // Ensure things work when nested in different places:
    {
      code: "Cu.reportError('foo' + e + 'quux' + 'baz')",
      output: "console.error('foo', e, 'quux' + 'baz')",
      errors: callError(),
    },
    {
      code: "Cu.reportError('foo' + 'quux' + e + 'baz')",
      output: "console.error('foo' + 'quux', e, 'baz')",
      errors: callError(),
    },
    // Ensure things work with parens changing order of operations:
    {
      code: "Cu.reportError('foo' + 'quux' + (e + 'baz'))",
      output: "console.error('foo' + 'quux' + (e + 'baz'))",
      errors: callError(),
    },
  ],
});
