/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/rejects-requires-await");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 8 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, messageId) {
  return { code, errors: [{ messageId: "rejectRequiresAwait" }] };
}

ruleTester.run("reject-requires-await", rule, {
  valid: [
    "async() => { await Assert.rejects(foo, /assertion/) }",
    "async() => { await Assert.rejects(foo, /assertion/, 'msg') }",
  ],
  invalid: [
    invalidCode("Assert.rejects(foo)"),
    invalidCode("Assert.rejects(foo, 'msg')"),
  ],
});
