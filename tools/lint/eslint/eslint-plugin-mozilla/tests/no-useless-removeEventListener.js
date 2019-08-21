/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/no-useless-removeEventListener");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code) {
  let message =
    "use {once: true} instead of removeEventListener " +
    "as the first instruction of the listener";
  return { code, errors: [{ message, type: "CallExpression" }] };
}

ruleTester.run("no-useless-removeEventListener", rule, {
  valid: [
    // Listeners that aren't a function are always valid.
    "elt.addEventListener('click', handler);",
    "elt.addEventListener('click', handler, true);",
    "elt.addEventListener('click', handler, {once: true});",

    // Should not fail on empty functions.
    "elt.addEventListener('click', function() {});",

    // Should not reject when removing a listener for another event.
    "elt.addEventListener('click', function listener() {" +
      "  elt.removeEventListener('keypress', listener);" +
      "});",

    // Should not reject when there's another instruction before
    // removeEventListener.
    "elt.addEventListener('click', function listener() {" +
      "  elt.focus();" +
      "  elt.removeEventListener('click', listener);" +
      "});",

    // Should not reject when wantsUntrusted is true.
    "elt.addEventListener('click', function listener() {" +
      "  elt.removeEventListener('click', listener);" +
      "}, false, true);",

    // Should not reject when there's a literal and a variable
    "elt.addEventListener('click', function listener() {" +
      "  elt.removeEventListener(eventName, listener);" +
      "});",

    // Should not reject when there's 2 different variables
    "elt.addEventListener(event1, function listener() {" +
      "  elt.removeEventListener(event2, listener);" +
      "});",

    // Should not fail if this is a different type of event listener function.
    "myfunc.addEventListener(listener);",
  ],
  invalid: [
    invalidCode(
      "elt.addEventListener('click', function listener() {" +
        "  elt.removeEventListener('click', listener);" +
        "});"
    ),
    invalidCode(
      "elt.addEventListener('click', function listener() {" +
        "  elt.removeEventListener('click', listener, true);" +
        "}, true);"
    ),
    invalidCode(
      "elt.addEventListener('click', function listener() {" +
        "  elt.removeEventListener('click', listener);" +
        "}, {once: true});"
    ),
    invalidCode(
      "elt.addEventListener('click', function listener() {" +
        "  /* Comment */" +
        "  elt.removeEventListener('click', listener);" +
        "});"
    ),
    invalidCode(
      "elt.addEventListener('click', function() {" +
        "  elt.removeEventListener('click', arguments.callee);" +
        "});"
    ),
    invalidCode(
      "elt.addEventListener(eventName, function listener() {" +
        "  elt.removeEventListener(eventName, listener);" +
        "});"
    ),
  ],
});
