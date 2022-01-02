/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/avoid-removeChild");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, message) {
  if (!message) {
    message =
      "use element.remove() instead of " +
      "element.parentNode.removeChild(element)";
  }
  return { code, errors: [{ message, type: "CallExpression" }] };
}

ruleTester.run("avoid-removeChild", rule, {
  valid: [
    "elt.remove();",
    "elt.parentNode.parentNode.removeChild(elt2.parentNode);",
    "elt.parentNode.removeChild(elt2);",
    "elt.removeChild(elt2);",
  ],
  invalid: [
    invalidCode("elt.parentNode.removeChild(elt);"),
    invalidCode("elt.parentNode.parentNode.removeChild(elt.parentNode);"),
    invalidCode("$(e).parentNode.removeChild($(e));"),
    invalidCode("$('e').parentNode.removeChild($('e'));"),
    invalidCode(
      "elt.removeChild(elt.firstChild);",
      "use element.firstChild.remove() instead of " +
        "element.removeChild(element.firstChild)"
    ),
  ],
});
