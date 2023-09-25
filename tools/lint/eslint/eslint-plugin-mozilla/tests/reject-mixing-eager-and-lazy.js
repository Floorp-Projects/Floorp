/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/reject-mixing-eager-and-lazy");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({
  parserOptions: { ecmaVersion: "latest", sourceType: "module" },
});

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, uri) {
  return { code, errors: [{ messageId: "mixedEagerAndLazy", data: { uri } }] };
}

ruleTester.run("reject-mixing-eager-and-lazy", rule, {
  valid: [
    `
    ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
`,
    `
    ChromeUtils.importESModule("resource://gre/modules/AppConstants.sys.mjs");
`,
    `
    import{ AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
`,
    `
    ChromeUtils.defineModuleGetter(
      lazy, "AppConstants", "resource://gre/modules/AppConstants.jsm"
    );
`,
    `
    XPCOMUtils.defineLazyModuleGetters(lazy, {
      AppConstants: "resource://gre/modules/AppConstants.jsm",
    });
`,
    `
    ChromeUtils.defineESModuleGetters(lazy, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
    });
`,
    `
    if (some_condition) {
      ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
    }
    XPCOMUtils.defineLazyModuleGetters(lazy, {
      AppConstants: "resource://gre/modules/AppConstants.jsm"
    });
`,
    `
    ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
    XPCOMUtils.defineLazyModuleGetters(sandbox, {
      AppConstants: "resource://gre/modules/AppConstants.jsm",
    });
`,
  ],
  invalid: [
    invalidCode(
      `
    ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
    ChromeUtils.defineModuleGetter(
      lazy, "AppConstants", "resource://gre/modules/AppConstants.jsm"
    );
`,
      "resource://gre/modules/AppConstants.jsm"
    ),
    invalidCode(
      `
    ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
    XPCOMUtils.defineLazyModuleGetters(lazy, {
      AppConstants: "resource://gre/modules/AppConstants.jsm",
    });
`,
      "resource://gre/modules/AppConstants.jsm"
    ),
    invalidCode(
      `
    ChromeUtils.importESModule("resource://gre/modules/AppConstants.sys.mjs");
    ChromeUtils.defineESModuleGetters(lazy, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
    });
`,
      "resource://gre/modules/AppConstants.sys.mjs"
    ),
    invalidCode(
      `
    import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
    ChromeUtils.defineESModuleGetters(lazy, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
    });
`,
      "resource://gre/modules/AppConstants.sys.mjs"
    ),
  ],
});
