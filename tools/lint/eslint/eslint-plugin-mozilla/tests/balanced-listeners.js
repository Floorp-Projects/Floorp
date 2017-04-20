/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/balanced-listeners");
var RuleTester = require("eslint/lib/testers/rule-tester");

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function error(code, message) {
  return {
    code,
    errors: [{message, type: "Identifier"}]
  };
}

ruleTester.run("balanced-listeners", rule, {
  valid: [
    "elt.addEventListener('event', handler);" +
    "elt.removeEventListener('event', handler);",

    "elt.addEventListener('event', handler, true);" +
    "elt.removeEventListener('event', handler, true);",

    "elt.addEventListener('event', handler, false);" +
    "elt.removeEventListener('event', handler, false);",

    "elt.addEventListener('event', handler);" +
    "elt.removeEventListener('event', handler, false);",

    "elt.addEventListener('event', handler, false);" +
    "elt.removeEventListener('event', handler);",

    "elt.addEventListener('event', handler, {capture: false});" +
    "elt.removeEventListener('event', handler);",

    "elt.addEventListener('event', handler);" +
    "elt.removeEventListener('event', handler, {capture: false});",

    "elt.addEventListener('event', handler, {capture: true});" +
    "elt.removeEventListener('event', handler, true);",

    "elt.addEventListener('event', handler, true);" +
    "elt.removeEventListener('event', handler, {capture: true});",

    "elt.addEventListener('event', handler, {once: true});",

    "elt.addEventListener('event', handler, {once: true, capture: true});"
  ],
  invalid: [
    error("elt.addEventListener('click', handler, false);",
          "No corresponding 'removeEventListener(click)' was found."),

    error("elt.addEventListener('click', handler, false);" +
          "elt.removeEventListener('click', handler, true);",
          "No corresponding 'removeEventListener(click)' was found."),

    error("elt.addEventListener('click', handler, {capture: false});" +
          "elt.removeEventListener('click', handler, true);",
          "No corresponding 'removeEventListener(click)' was found."),

    error("elt.addEventListener('click', handler, {capture: true});" +
          "elt.removeEventListener('click', handler);",
          "No corresponding 'removeEventListener(click)' was found."),

    error("elt.addEventListener('click', handler, true);" +
          "elt.removeEventListener('click', handler);",
          "No corresponding 'removeEventListener(click)' was found.")
  ]
});
