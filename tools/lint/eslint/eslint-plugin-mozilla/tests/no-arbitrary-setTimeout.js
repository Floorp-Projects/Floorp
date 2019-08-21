/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/no-arbitrary-setTimeout");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function wrapCode(code, filename = "xpcshell/test_foo.js") {
  return { code, filename };
}

function invalidCode(code) {
  let message =
    "listen for events instead of setTimeout() with arbitrary delay";
  let obj = wrapCode(code);
  obj.errors = [{ message, type: "CallExpression" }];
  return obj;
}

ruleTester.run("no-arbitrary-setTimeout", rule, {
  valid: [
    wrapCode("setTimeout(function() {}, 0);"),
    wrapCode("setTimeout(function() {});"),
    wrapCode("setTimeout(function() {}, 10);", "test_foo.js"),
  ],
  invalid: [
    invalidCode("setTimeout(function() {}, 10);"),
    invalidCode("setTimeout(function() {}, timeout);"),
  ],
});
