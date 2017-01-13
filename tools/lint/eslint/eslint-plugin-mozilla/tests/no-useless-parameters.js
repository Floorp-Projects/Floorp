/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var rule = require("../lib/rules/no-useless-parameters");

//------------------------------------------------------------------------------
// Tests
//------------------------------------------------------------------------------

function callError(message) {
  return [{message: message, type: "CallExpression"}];
}

exports.runTest = function(ruleTester) {
  ruleTester.run("no-useless-parameters", rule, {
    valid: [
      "Services.prefs.clearUserPref('browser.search.custom');",
      "Services.prefs.getBoolPref('browser.search.custom');",
      "Services.prefs.getCharPref('browser.search.custom');",
      "Services.prefs.getIntPref('browser.search.custom');",
      "Services.removeObserver('notification name', {});",
      "Services.io.newURI('http://example.com');",
      "Services.io.newURI('http://example.com', 'utf8');",
    ],
    invalid: [
      {
        code: "Services.prefs.clearUserPref('browser.search.custom', false);",
        errors: callError("clearUserPref takes only 1 parameter.")
      },
      {
        code: "Services.prefs.getBoolPref('browser.search.custom', true);",
        errors: callError("getBoolPref takes only 1 parameter.")
      },
      {
        code: "Services.prefs.getCharPref('browser.search.custom', 'a');",
        errors: callError("getCharPref takes only 1 parameter.")
      },
      {
        code: "Services.prefs.getIntPref('browser.search.custom', 42);",
        errors: callError("getIntPref takes only 1 parameter.")
      },
      {
        code: "Services.removeObserver('notification name', {}, false);",
        errors: callError("removeObserver only takes 2 parameters.")
      },
      {
        code: "Services.removeObserver('notification name', {}, true);",
        errors: callError("removeObserver only takes 2 parameters.")
      },
      {
        code: "Services.io.newURI('http://example.com', null, null);",
        errors: callError("newURI's optional parameters passed as null.")
      },
      {
        code: "Services.io.newURI('http://example.com', 'utf8', null);",
        errors: callError("newURI's optional parameters passed as null.")
      }
    ]
  });
};
