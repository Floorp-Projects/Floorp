/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/reject-top-level-await");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({
  parserOptions: { ecmaVersion: "latest", sourceType: "module" },
});

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, messageId) {
  return { code, errors: [{ messageId: "rejectTopLevelAwait" }] };
}

ruleTester.run("reject-top-level-await", rule, {
  valid: [
    "async() => { await bar() }",
    "async() => { for await (let x of []) {} }",
  ],
  invalid: [
    invalidCode("await foo"),
    invalidCode("{ await foo }"),
    invalidCode("(await foo)"),
    invalidCode("for await (let x of []) {}"),
  ],
});
