/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/use-services");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, name) {
  let message = `Use Services.${name} rather than getService().`;
  return { code, errors: [{ message, type: "CallExpression" }] };
}

ruleTester.run("use-services", rule, {
  valid: [
    'Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator)',
    'Components.classes["@mozilla.org/uuid-generator;1"].getService(Components.interfaces.nsIUUIDGenerator)',
    "Services.wm.addListener()",
  ],
  invalid: [
    invalidCode(
      'Cc["@mozilla.org/appshell/window-mediator;1"].getService(Ci.nsIWindowMediator);',
      "wm"
    ),
    invalidCode(
      'Components.classes["@mozilla.org/toolkit/app-startup;1"].getService(Components.interfaces.nsIAppStartup);',
      "startup"
    ),
  ],
});
