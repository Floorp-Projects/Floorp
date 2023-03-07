/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/lazy-getter-object-name");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code) {
  return {
    code,
    errors: [{ messageId: "mustUseLazy", type: "CallExpression" }],
  };
}

ruleTester.run("lazy-getter-object-name", rule, {
  valid: [
    `
    ChromeUtils.defineESModuleGetters(lazy, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
    });
`,
  ],
  invalid: [
    invalidCode(`
    ChromeUtils.defineESModuleGetters(obj, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
    });
`),
    invalidCode(`
    ChromeUtils.defineESModuleGetters(this, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
    });
`),
    invalidCode(`
    ChromeUtils.defineESModuleGetters(window, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
    });
`),
  ],
});
