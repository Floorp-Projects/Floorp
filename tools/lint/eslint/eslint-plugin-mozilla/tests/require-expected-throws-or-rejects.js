/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/require-expected-throws-or-rejects");
var RuleTester = require("eslint/lib/testers/rule-tester");

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, messageId, name) {
  return {code, errors: [{messageId, data: {name}}]};
}

ruleTester.run("no-useless-run-test", rule, {
  valid: [
    "Assert.throws(() => foo(), /assertion/);",
    "Assert.throws(() => foo(), /assertion/, 'message');",
    "Assert.throws(() => foo(), ex => {}, 'message');",
    "Assert.throws(() => foo(), foo, 'message');",
    "Assert.rejects(foo, /assertion/)",
    "Assert.rejects(foo, /assertion/, 'msg')",
    "Assert.rejects(foo, ex => {}, 'msg')",
    "Assert.rejects(foo, foo, 'msg')"
  ],
  invalid: [
    invalidCode("Assert.throws(() => foo());",
                "needsTwoArguments", "throws"),

    invalidCode("Assert.throws(() => foo(), 'message');",
                "requireExpected", "throws"),

    invalidCode("Assert.throws(() => foo(), 'invalid', 'message');",
                "requireExpected", "throws"),

    invalidCode("Assert.throws(() => foo(), null, 'message');",
                "requireExpected", "throws"),

    invalidCode("Assert.throws(() => foo(), undefined, 'message');",
                "requireExpected", "throws"),

    invalidCode("Assert.rejects(foo)",
                "needsTwoArguments", "rejects"),

    invalidCode("Assert.rejects(foo, 'msg')",
                "requireExpected", "rejects"),

    invalidCode("Assert.rejects(foo, 'invalid', 'msg')",
                "requireExpected", "rejects"),

    invalidCode("Assert.rejects(foo, undefined, 'msg')",
                "requireExpected", "rejects")
  ]
});
