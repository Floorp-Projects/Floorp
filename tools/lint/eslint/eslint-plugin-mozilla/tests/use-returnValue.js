/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/use-returnValue");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, methodName) {
  let message = `{Array/String}.${methodName} doesn't modify the instance in-place`;
  return { code, errors: [{ message, type: "ExpressionStatement" }] };
}

ruleTester.run("use-returnValue", rule, {
  valid: [
    "a = foo.concat(bar)",
    "b = bar.concat([1,3,4])",
    "c = baz.concat()",
    "d = qux.join(' ')",
    "e = quux.slice(1)",
  ],
  invalid: [
    invalidCode("foo.concat(bar)", "concat"),
    invalidCode("bar.concat([1,3,4])", "concat"),
    invalidCode("baz.concat()", "concat"),
    invalidCode("qux.join(' ')", "join"),
    invalidCode("quux.slice(1)", "slice"),
  ],
});
