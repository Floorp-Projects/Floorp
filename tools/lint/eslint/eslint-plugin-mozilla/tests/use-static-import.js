/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/use-static-import");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({
  parserOptions: { ecmaVersion: "latest", sourceType: "module" },
});

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function callError() {
  return [{ messageId: "useStaticImport", type: "VariableDeclaration" }];
}

ruleTester.run("use-static-import", rule, {
  valid: [
    {
      // Already converted, no issues.
      code: 'import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";',
      filename: "test.sys.mjs",
    },
    {
      // Inside an if statement.
      code: 'if (foo) { const { XPCOMUtils } = ChromeUtils.importESModule("resource://gre/modules/XPCOMUtils.sys.mjs") }',
      filename: "test.sys.mjs",
    },
    {
      // Inside a function.
      code: 'function foo() { const { XPCOMUtils } = ChromeUtils.importESModule("resource://gre/modules/XPCOMUtils.sys.mjs") }',
      filename: "test.sys.mjs",
    },
    {
      // importESModule with two args cannot be converted.
      code: 'const { f } = ChromeUtils.importESModule("some/module.sys.mjs", { global : "shared" });',
      filename: "test.sys.mjs",
    },
    {
      // A non-system file attempting to import a system file should not be
      // converted.
      code: 'const { XPCOMUtils } = ChromeUtils.importESModule("resource://gre/modules/XPCOMUtils.sys.mjs")',
      filename: "test.mjs",
    },
  ],
  invalid: [
    {
      // Simple import in system module should be converted.
      code: 'const { XPCOMUtils } = ChromeUtils.importESModule("resource://gre/modules/XPCOMUtils.sys.mjs")',
      errors: callError(),
      filename: "test.sys.mjs",
      output:
        'import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs"',
    },
    {
      // Should handle rewritten variables as well.
      code: 'const { XPCOMUtils: foo } = ChromeUtils.importESModule("resource://gre/modules/XPCOMUtils.sys.mjs")',
      errors: callError(),
      filename: "test.sys.mjs",
      output:
        'import { XPCOMUtils as foo } from "resource://gre/modules/XPCOMUtils.sys.mjs"',
    },
    {
      // Should handle multiple variables.
      code: 'const { foo, XPCOMUtils } = ChromeUtils.importESModule("resource://gre/modules/XPCOMUtils.sys.mjs")',
      errors: callError(),
      filename: "test.sys.mjs",
      output:
        'import { foo, XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs"',
    },
  ],
});
