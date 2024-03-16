/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/no-more-globals");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

function makeTest(code, errors = []) {
  return {
    code,
    errors,
    filename: __dirname + "/helper-no-more-globals.js",
  };
}

ruleTester.run("no-more-globals", rule, {
  valid: [
    makeTest("function foo() {}"),
    makeTest("var foo = 5;"),
    makeTest("let foo = 42;"),
  ],
  invalid: [
    makeTest("console.log('hello');", [
      { messageId: "removedGlobal", data: { name: "foo" } },
    ]),
    makeTest("let bar = 42;", [
      { messageId: "newGlobal", data: { name: "bar" } },
      { messageId: "removedGlobal", data: { name: "foo" } },
    ]),
  ],
});
