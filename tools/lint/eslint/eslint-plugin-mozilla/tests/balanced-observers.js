/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/balanced-observers");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function error(code, message) {
  return {
    code,
    errors: [{ message, type: "Identifier" }],
  };
}

ruleTester.run("balanced-observers", rule, {
  valid: [
    "Services.obs.addObserver(observer, 'observable');" +
      "Services.obs.removeObserver(observer, 'observable');",
    "Services.prefs.addObserver('preference.name', otherObserver);" +
      "Services.prefs.removeObserver('preference.name', otherObserver);",
  ],
  invalid: [
    error(
      // missing Services.obs.removeObserver
      "Services.obs.addObserver(observer, 'observable');",
      "No corresponding 'removeObserver(\"observable\")' was found."
    ),

    error(
      // wrong observable name for Services.obs.removeObserver
      "Services.obs.addObserver(observer, 'observable');" +
        "Services.obs.removeObserver(observer, 'different-observable');",
      "No corresponding 'removeObserver(\"observable\")' was found."
    ),

    error(
      // missing Services.prefs.removeObserver
      "Services.prefs.addObserver('preference.name', otherObserver);",
      "No corresponding 'removeObserver(\"preference.name\")' was found."
    ),

    error(
      // wrong observable name for Services.prefs.removeObserver
      "Services.prefs.addObserver('preference.name', otherObserver);" +
        "Services.prefs.removeObserver('other.preference', otherObserver);",
      "No corresponding 'removeObserver(\"preference.name\")' was found."
    ),

    error(
      // mismatch Services.prefs vs Services.obs
      "Services.obs.addObserver(observer, 'observable');" +
        "Services.prefs.removeObserver(observer, 'observable');",
      "No corresponding 'removeObserver(\"observable\")' was found."
    ),

    error(
      "Services.prefs.addObserver('preference.name', otherObserver);" +
        // mismatch Services.prefs vs Services.obs
        "Services.obs.removeObserver('preference.name', otherObserver);",
      "No corresponding 'removeObserver(\"preference.name\")' was found."
    ),
  ],
});
