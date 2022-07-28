/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/reject-multiple-getters-calls");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code) {
  return { code, errors: [{ messageId: "rejectMultipleCalls" }] };
}

ruleTester.run("reject-multiple-getters-calls", rule, {
  valid: [
    `
      ChromeUtils.defineESModuleGetters(lazy, {
        AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
        PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
      });
    `,
    `
      ChromeUtils.defineESModuleGetters(lazy, {
        AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
      });
      ChromeUtils.defineESModuleGetters(window, {
        PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
      });
    `,
    `
      ChromeUtils.defineESModuleGetters(lazy, {
        AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
      });
      if (cond) {
        ChromeUtils.defineESModuleGetters(lazy, {
          PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
        });
      }
    `,
  ],
  invalid: [
    invalidCode(`
      ChromeUtils.defineESModuleGetters(lazy, {
        AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
      });
      ChromeUtils.defineESModuleGetters(lazy, {
        PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
      });
    `),
  ],
});
