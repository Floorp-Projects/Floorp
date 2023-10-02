/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/use-chromeutils-import");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function callError(messageId) {
  return [{ messageId, type: "CallExpression" }];
}

ruleTester.run("use-chromeutils-import", rule, {
  valid: [
    `ChromeUtils.import("resource://gre/modules/AppConstants.jsm");`,
    `ChromeUtils.import("resource://gre/modules/AppConstants.jsm", this);`,
    `ChromeUtils.defineModuleGetter(this, "AppConstants",
                                    "resource://gre/modules/AppConstants.jsm");`,
  ],
  invalid: [
    {
      code: `Cu.import("resource://gre/modules/AppConstants.jsm");`,
      output: `ChromeUtils.import("resource://gre/modules/AppConstants.jsm");`,
      errors: callError("useChromeUtilsImport"),
    },
    {
      code: `Cu.import("resource://gre/modules/AppConstants.jsm", this);`,
      output: `ChromeUtils.import("resource://gre/modules/AppConstants.jsm", this);`,
      errors: callError("useChromeUtilsImport"),
    },
    {
      code: `Components.utils.import("resource://gre/modules/AppConstants.jsm");`,
      output: `ChromeUtils.import("resource://gre/modules/AppConstants.jsm");`,
      errors: callError("useChromeUtilsImport"),
    },
  ],
});
