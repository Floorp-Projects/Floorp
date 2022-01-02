/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/no-compare-against-boolean-literals");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function callError(message) {
  return [{ message, type: "BinaryExpression" }];
}

const MESSAGE = "Don't compare for inexact equality against boolean literals";

ruleTester.run("no-compare-against-boolean-literals", rule, {
  valid: [`if (!foo) {}`, `if (!!foo) {}`],
  invalid: [
    {
      code: `if (foo == true) {}`,
      errors: callError(MESSAGE),
    },
    {
      code: `if (foo != true) {}`,
      errors: callError(MESSAGE),
    },
    {
      code: `if (foo == false) {}`,
      errors: callError(MESSAGE),
    },
    {
      code: `if (foo != false) {}`,
      errors: callError(MESSAGE),
    },
    {
      code: `if (true == foo) {}`,
      errors: callError(MESSAGE),
    },
    {
      code: `if (true != foo) {}`,
      errors: callError(MESSAGE),
    },
    {
      code: `if (false == foo) {}`,
      errors: callError(MESSAGE),
    },
    {
      code: `if (false != foo) {}`,
      errors: callError(MESSAGE),
    },
  ],
});
