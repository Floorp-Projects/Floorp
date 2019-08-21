/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/use-chromeutils-import");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function callError(message) {
  return [{ message, type: "CallExpression" }];
}

const MESSAGE_IMPORT = "Please use ChromeUtils.import instead of Cu.import";
const MESSAGE_DEFINE =
  "Please use ChromeUtils.defineModuleGetter instead of " +
  "XPCOMUtils.defineLazyModuleGetter";

ruleTester.run("use-chromeutils-import", rule, {
  valid: [
    `ChromeUtils.import("resource://gre/modules/Service.jsm");`,
    `ChromeUtils.import("resource://gre/modules/Service.jsm", this);`,
    `ChromeUtils.defineModuleGetter(this, "Services",
                                    "resource://gre/modules/Service.jsm");`,
    `XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                       "resource://gre/modules/Service.jsm",
                                       "Foo");`,
    `XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                       "resource://gre/modules/Service.jsm",
                                       undefined, preServicesLambda);`,
    {
      options: [{ allowCu: true }],
      code: `Cu.import("resource://gre/modules/Service.jsm");`,
    },
  ],
  invalid: [
    {
      code: `Cu.import("resource://gre/modules/Services.jsm");`,
      output: `ChromeUtils.import("resource://gre/modules/Services.jsm");`,
      errors: callError(MESSAGE_IMPORT),
    },
    {
      code: `Cu.import("resource://gre/modules/Services.jsm", this);`,
      output: `ChromeUtils.import("resource://gre/modules/Services.jsm", this);`,
      errors: callError(MESSAGE_IMPORT),
    },
    {
      code: `Components.utils.import("resource://gre/modules/Services.jsm");`,
      output: `ChromeUtils.import("resource://gre/modules/Services.jsm");`,
      errors: callError(MESSAGE_IMPORT),
    },
    {
      code: `Components.utils.import("resource://gre/modules/Services.jsm");`,
      output: `ChromeUtils.import("resource://gre/modules/Services.jsm");`,
      errors: callError(MESSAGE_IMPORT),
    },
    {
      options: [{ allowCu: true }],
      code: `Components.utils.import("resource://gre/modules/Services.jsm", this);`,
      output: `ChromeUtils.import("resource://gre/modules/Services.jsm", this);`,
      errors: callError(MESSAGE_IMPORT),
    },
    {
      code: `XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                               "resource://gre/modules/Services.jsm");`,
      output: `ChromeUtils.defineModuleGetter(this, "Services",
                                               "resource://gre/modules/Services.jsm");`,
      errors: callError(MESSAGE_DEFINE),
    },
  ],
});
