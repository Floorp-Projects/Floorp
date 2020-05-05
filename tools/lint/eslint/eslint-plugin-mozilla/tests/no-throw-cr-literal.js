/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/no-throw-cr-literal");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, output, messageId) {
  return {
    code,
    output,
    errors: [{ messageId, type: "ThrowStatement" }],
  };
}

ruleTester.run("no-throw-cr-literal", rule, {
  valid: [
    'throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);',
    'throw Components.Exception("", Components.results.NS_ERROR_UNEXPECTED);',
    'function t() { throw Components.Exception("", Cr.NS_ERROR_NO_CONTENT); }',
    // We don't handle combined values, regular no-throw-literal catches them
    'throw Components.results.NS_ERROR_UNEXPECTED + "whoops";',
  ],
  invalid: [
    invalidCode(
      "throw Cr.NS_ERROR_NO_INTERFACE;",
      'throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);',
      "bareCR"
    ),
    invalidCode(
      "throw Components.results.NS_ERROR_ABORT;",
      'throw Components.Exception("", Components.results.NS_ERROR_ABORT);',
      "bareComponentsResults"
    ),
    invalidCode(
      "function t() { throw Cr.NS_ERROR_NULL_POINTER; }",
      'function t() { throw Components.Exception("", Cr.NS_ERROR_NULL_POINTER); }',
      "bareCR"
    ),
    invalidCode(
      "throw new Error(Cr.NS_ERROR_ABORT);",
      'throw Components.Exception("", Cr.NS_ERROR_ABORT);',
      "newErrorCR"
    ),
    invalidCode(
      "throw new Error(Components.results.NS_ERROR_ABORT);",
      'throw Components.Exception("", Components.results.NS_ERROR_ABORT);',
      "newErrorComponentsResults"
    ),
  ],
});
