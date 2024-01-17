/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/use-chromeutils-definelazygetter");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function callError(messageId) {
  return [{ messageId, type: "CallExpression" }];
}

ruleTester.run("use-chromeutils-definelazygetter", rule, {
  valid: [
    `ChromeUtils.defineLazyGetter(lazy, "textEncoder", function () { return new TextEncoder(); });`,
  ],
  invalid: [
    {
      code: `XPCOMUtils.defineLazyGetter(lazy, "textEncoder", function () { return new TextEncoder(); });`,
      output: `ChromeUtils.defineLazyGetter(lazy, "textEncoder", function () { return new TextEncoder(); });`,
      errors: callError("useChromeUtilsDefineLazyGetter"),
    },
  ],
});
