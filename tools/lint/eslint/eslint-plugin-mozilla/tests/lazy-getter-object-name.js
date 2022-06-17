/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/lazy-getter-object-name");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code) {
  let message =
    "The variable name of the object passed to ChromeUtils.defineESModuleGetters must be `lazy`";
  return { code, errors: [{ message, type: "CallExpression" }] };
}

ruleTester.run("lazy-getter-object-name", rule, {
  valid: [
    `
    ChromeUtils.defineESModuleGetters(lazy, {
      Services: "resource://gre/modules/Services.sys.mjs",
    });
`,
  ],
  invalid: [
    invalidCode(`
    ChromeUtils.defineESModuleGetters(obj, {
      Services: "resource://gre/modules/Services.sys.mjs",
    });
`),
    invalidCode(`
    ChromeUtils.defineESModuleGetters(this, {
      Services: "resource://gre/modules/Services.sys.mjs",
    });
`),
    invalidCode(`
    ChromeUtils.defineESModuleGetters(window, {
      Services: "resource://gre/modules/Services.sys.mjs",
    });
`),
  ],
});
