/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/reject-chromeutils-import");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

const invalidError = [
  {
    messageId: "useImportESModule",
    type: "CallExpression",
  },
];

const invalidErrorLazy = [
  {
    messageId: "useImportESModuleLazy",
    type: "CallExpression",
  },
];

ruleTester.run("reject-chromeutils-import", rule, {
  valid: [
    'ChromeUtils.importESModule("resource://some/path/to/My.sys.mjs")',
    'ChromeUtils.defineESModuleGetters(obj, { My: "resource://some/path/to/My.sys.mjs" })',
  ],
  invalid: [
    {
      code: 'ChromeUtils.import("resource://some/path/to/My.jsm")',
      errors: invalidError,
    },
    {
      code: 'SpecialPowers.ChromeUtils.import("resource://some/path/to/My.jsm")',
      errors: invalidError,
    },
    {
      code: 'ChromeUtils.defineModuleGetter(obj, "My", "resource://some/path/to/My.jsm")',
      errors: invalidErrorLazy,
    },
    {
      code: 'SpecialPowers.ChromeUtils.defineModuleGetter(obj, "My", "resource://some/path/to/My.jsm")',
      errors: invalidErrorLazy,
    },
    {
      code: 'XPCOMUtils.defineLazyModuleGetters(obj, { My: "resource://some/path/to/My.jsm" })',
      errors: invalidErrorLazy,
    },
  ],
});
