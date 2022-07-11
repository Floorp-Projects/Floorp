/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/reject-eager-module-in-lazy-getter");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, uri) {
  return { code, errors: [{ messageId: "eagerModule", data: { uri } }] };
}

ruleTester.run("reject-eager-module-in-lazy-getter", rule, {
  valid: [
    `
    XPCOMUtils.defineLazyModuleGetter(
      lazy, "Integration", "resource://gre/modules/Integration.jsm"
    );
`,
    `
    ChromeUtils.defineModuleGetter(
      lazy, "Integration", "resource://gre/modules/Integration.jsm"
    );
`,
    `
    XPCOMUtils.defineLazyModuleGetters(lazy, {
      Integration: "resource://gre/modules/Integration.jsm",
    });
`,
    `
    ChromeUtils.defineESModuleGetters(lazy, {
      Integration: "resource://gre/modules/Integration.sys.mjs",
    });
`,
  ],
  invalid: [
    invalidCode(
      `
    XPCOMUtils.defineLazyModuleGetter(
      lazy, "AppConstants", "resource://gre/modules/AppConstants.jsm"
    );
`,
      "resource://gre/modules/AppConstants.jsm"
    ),
    invalidCode(
      `
    ChromeUtils.defineModuleGetter(
      lazy, "XPCOMUtils", "resource://gre/modules/XPCOMUtils.jsm"
    );
`,
      "resource://gre/modules/XPCOMUtils.jsm"
    ),
    invalidCode(
      `
    XPCOMUtils.defineLazyModuleGetters(lazy, {
      AppConstants: "resource://gre/modules/AppConstants.jsm",
    });
`,
      "resource://gre/modules/AppConstants.jsm"
    ),
    invalidCode(
      `
    ChromeUtils.defineESModuleGetters(lazy, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
    });
`,
      "resource://gre/modules/AppConstants.sys.mjs"
    ),
  ],
});
