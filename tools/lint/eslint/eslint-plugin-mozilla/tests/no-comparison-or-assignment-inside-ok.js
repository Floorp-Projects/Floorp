/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/no-comparison-or-assignment-inside-ok");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, output, messageId, options = []) {
  let rv = {
    code,
    errors: [{ messageId }],
  };
  if (output) {
    rv.output = output;
  }
  return rv;
}

ruleTester.run("no-comparison-or-assignment-inside-ok", rule, {
  valid: [
    "ok(foo)",
    "ok(!bar)",
    "ok(!foo, 'Message')",
    "ok(bar, 'Message')",
    "ok(!!foo, 'Message')",
  ],
  invalid: [
    // Assignment
    invalidCode("ok(foo = bar)", null, "assignment"),
    invalidCode("ok(foo = bar, 'msg')", null, "assignment"),

    // Comparisons:
    invalidCode("ok(foo == bar)", "Assert.equal(foo, bar)", "comparison", {
      operator: "==",
    }),
    invalidCode("ok(foo != bar)", "Assert.notEqual(foo, bar)", "comparison", {
      operator: "!=",
    }),
    invalidCode("ok(foo < bar)", "Assert.less(foo, bar)", "comparison", {
      operator: "<",
    }),
    invalidCode("ok(foo > bar)", "Assert.greater(foo, bar)", "comparison", {
      operator: ">",
    }),
    invalidCode(
      "ok(foo <= bar)",
      "Assert.lessOrEqual(foo, bar)",
      "comparison",
      { operator: "<=" }
    ),
    invalidCode(
      "ok(foo >= bar)",
      "Assert.greaterOrEqual(foo, bar)",
      "comparison",
      { operator: ">=" }
    ),
    invalidCode(
      "ok(foo === bar)",
      "Assert.strictEqual(foo, bar)",
      "comparison",
      { operator: "===" }
    ),
    invalidCode(
      "ok(foo !== bar)",
      "Assert.notStrictEqual(foo, bar)",
      "comparison",
      { operator: "!==" }
    ),

    // Comparisons with messages:
    invalidCode(
      "ok(foo == bar, 'hi')",
      "Assert.equal(foo, bar, 'hi')",
      "comparison",
      { operator: "==" }
    ),
    invalidCode(
      "ok(foo != bar, 'hi')",
      "Assert.notEqual(foo, bar, 'hi')",
      "comparison",
      { operator: "!=" }
    ),
    invalidCode(
      "ok(foo < bar, 'hi')",
      "Assert.less(foo, bar, 'hi')",
      "comparison",
      { operator: "<" }
    ),
    invalidCode(
      "ok(foo > bar, 'hi')",
      "Assert.greater(foo, bar, 'hi')",
      "comparison",
      { operator: ">" }
    ),
    invalidCode(
      "ok(foo <= bar, 'hi')",
      "Assert.lessOrEqual(foo, bar, 'hi')",
      "comparison",
      { operator: "<=" }
    ),
    invalidCode(
      "ok(foo >= bar, 'hi')",
      "Assert.greaterOrEqual(foo, bar, 'hi')",
      "comparison",
      { operator: ">=" }
    ),
    invalidCode(
      "ok(foo === bar, 'hi')",
      "Assert.strictEqual(foo, bar, 'hi')",
      "comparison",
      { operator: "===" }
    ),
    invalidCode(
      "ok(foo !== bar, 'hi')",
      "Assert.notStrictEqual(foo, bar, 'hi')",
      "comparison",
      { operator: "!==" }
    ),

    // Confusing bits that break fixup:
    invalidCode(
      "async () => ok((await foo) === bar, 'Oh no')",
      "async () => Assert.strictEqual(await foo, bar, 'Oh no')",
      "comparison",
      { operator: "===" }
    ),
  ],
});
