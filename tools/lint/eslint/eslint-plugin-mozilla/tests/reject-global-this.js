/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/reject-global-this");
var RuleTester = require("eslint").RuleTester;

// class static block is available from ES2022 = 13.
const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 13 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code) {
  let message = "JSM should not use the global this";
  return { code, errors: [{ message, type: "ThisExpression" }] };
}

ruleTester.run("reject-top-level-await", rule, {
  valid: [
    "function f() { this; }",
    "(function f() { this; });",
    "({ foo() { this; } });",
    "({ get foo() { this; } })",
    "({ set foo(x) { this; } })",
    "class X { foo() { this; } }",
    "class X { get foo() { this; } }",
    "class X { set foo(x) { this; } }",
    "class X { static foo() { this; } }",
    "class X { static get foo() { this; } }",
    "class X { static set foo(x) { this; } }",
    "class X { P = this; }",
    "class X { #P = this; }",
    "class X { static { this; } }",
  ],
  invalid: [
    invalidCode("this;"),
    invalidCode("() => this;"),

    invalidCode("this.foo = 10;"),
    invalidCode("ChromeUtils.defineModuleGetter(this, {});"),
  ],
});
