/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/no-compare-against-boolean-literals");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function callError() {
  return [{ messageId: "noCompareBoolean", type: "BinaryExpression" }];
}

ruleTester.run("no-compare-against-boolean-literals", rule, {
  valid: [`if (!foo) {}`, `if (!!foo) {}`],
  invalid: [
    {
      code: `if (foo == true) {}`,
      errors: callError(),
    },
    {
      code: `if (foo != true) {}`,
      errors: callError(),
    },
    {
      code: `if (foo == false) {}`,
      errors: callError(),
    },
    {
      code: `if (foo != false) {}`,
      errors: callError(),
    },
    {
      code: `if (true == foo) {}`,
      errors: callError(),
    },
    {
      code: `if (true != foo) {}`,
      errors: callError(),
    },
    {
      code: `if (false == foo) {}`,
      errors: callError(),
    },
    {
      code: `if (false != foo) {}`,
      errors: callError(),
    },
  ],
});
