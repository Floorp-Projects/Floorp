/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/prefer-boolean-length-check");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidError() {
  return [{ messageId: "preferBooleanCheck", type: "BinaryExpression" }];
}

ruleTester.run("check-length", rule, {
  valid: [
    "if (foo.length && foo.length) {}",
    "if (!foo.length) {}",
    "if (foo.value == 0) {}",
    "if (foo.value > 0) {}",
    "if (0 == foo.value) {}",
    "if (0 < foo.value) {}",
    "var a = !!foo.length",
    "function bar() { return !!foo.length }",
  ],
  invalid: [
    {
      code: "if (foo.length == 0) {}",
      output: "if (!foo.length) {}",
      errors: invalidError(),
    },
    {
      code: "if (foo.length > 0) {}",
      output: "if (foo.length) {}",
      errors: invalidError(),
    },
    {
      code: "if (0 < foo.length) {}",
      output: "if (foo.length) {}",
      errors: invalidError(),
    },
    {
      code: "if (0 == foo.length) {}",
      output: "if (!foo.length) {}",
      errors: invalidError(),
    },
    {
      code: "if (foo && foo.length == 0) {}",
      output: "if (foo && !foo.length) {}",
      errors: invalidError(),
    },
    {
      code: "if (foo.bar.length == 0) {}",
      output: "if (!foo.bar.length) {}",
      errors: invalidError(),
    },
    {
      code: "var a = foo.length>0",
      output: "var a = !!foo.length",
      errors: invalidError(),
    },
    {
      code: "function bar() { return foo.length>0 }",
      output: "function bar() { return !!foo.length }",
      errors: invalidError(),
    },
    {
      code: "if (foo && bar.length>0) {}",
      output: "if (foo && bar.length) {}",
      errors: invalidError(),
    },
    {
      code: "while (foo && bar.length>0) {}",
      output: "while (foo && bar.length) {}",
      errors: invalidError(),
    },
    {
      code: "x = y && bar.length > 0",
      output: "x = y && !!bar.length",
      errors: invalidError(),
    },
    {
      code: "function bar() { return x && foo.length > 0}",
      output: "function bar() { return x && !!foo.length}",
      errors: invalidError(),
    },
  ],
});
