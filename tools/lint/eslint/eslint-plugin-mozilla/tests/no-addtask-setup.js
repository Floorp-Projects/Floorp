/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/no-addtask-setup");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 9 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function callError(message) {
  return [{ message, type: "CallExpression" }];
}

ruleTester.run("no-addtask-setup", rule, {
  valid: [
    "add_task(function() {});",
    "add_task(function () {});",
    "add_task(function foo() {});",
    "add_task(async function() {});",
    "add_task(async function () {});",
    "add_task(async function foo() {});",
    "something(function setup() {});",
    "something(async function setup() {});",
    "add_task(setup);",
    "add_task(setup => {});",
    "add_task(async setup => {});",
  ],
  invalid: [
    {
      code: "add_task(function setup() {});",
      output: "add_setup(function() {});",
      errors: callError(
        "Do not use add_task() for setup, use add_setup() instead."
      ),
    },
    {
      code: "add_task(function setup () {});",
      output: "add_setup(function () {});",
      errors: callError(
        "Do not use add_task() for setup, use add_setup() instead."
      ),
    },
    {
      code: "add_task(async function setup() {});",
      output: "add_setup(async function() {});",
      errors: callError(
        "Do not use add_task() for setup, use add_setup() instead."
      ),
    },
    {
      code: "add_task(async function setup () {});",
      output: "add_setup(async function () {});",
      errors: callError(
        "Do not use add_task() for setup, use add_setup() instead."
      ),
    },
    {
      code: "add_task(async function setUp() {});",
      output: "add_setup(async function() {});",
      errors: callError(
        "Do not use add_task() for setup, use add_setup() instead."
      ),
    },
  ],
});
