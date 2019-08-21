/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/no-useless-run-test");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, output) {
  let message =
    "Useless run_test function - only contains run_next_test; whole function can be removed";
  return { code, output, errors: [{ message, type: "FunctionDeclaration" }] };
}

ruleTester.run("no-useless-run-test", rule, {
  valid: [
    "function run_test() {}",
    "function run_test() {let args = 1; run_next_test();}",
    "function run_test() {fakeCall(); run_next_test();}",
  ],
  invalid: [
    // Single-line case.
    invalidCode("function run_test() { run_next_test(); }", ""),
    // Multiple-line cases
    invalidCode(
      `
function run_test() {
  run_next_test();
}`,
      ``
    ),
    invalidCode(
      `
foo();
function run_test() {
 run_next_test();
}
`,
      `
foo();
`
    ),
    invalidCode(
      `
foo();
function run_test() {
  run_next_test();
}
bar();
`,
      `
foo();
bar();
`
    ),
    invalidCode(
      `
foo();

function run_test() {
  run_next_test();
}

bar();`,
      `
foo();

bar();`
    ),
    invalidCode(
      `
foo();

function run_test() {
  run_next_test();
}

// A comment
bar();
`,
      `
foo();

// A comment
bar();
`
    ),
    invalidCode(
      `
foo();

/**
 * A useful comment.
 */
function run_test() {
  run_next_test();
}

// A comment
bar();
`,
      `
foo();

/**
 * A useful comment.
 */

// A comment
bar();
`
    ),
  ],
});
