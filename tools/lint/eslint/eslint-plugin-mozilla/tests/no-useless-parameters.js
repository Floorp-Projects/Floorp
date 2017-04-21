/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/no-useless-parameters");
var RuleTester = require("eslint/lib/testers/rule-tester");

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function callError(message) {
  return [{message, type: "CallExpression"}];
}

ruleTester.run("no-useless-parameters", rule, {
  valid: [
    "Services.prefs.clearUserPref('browser.search.custom');",
    "Services.removeObserver('notification name', {});",
    "Services.io.newURI('http://example.com');",
    "Services.io.newURI('http://example.com', 'utf8');",
    "elt.addEventListener('click', handler);",
    "elt.addEventListener('click', handler, true);",
    "elt.addEventListener('click', handler, {once: true});",
    "elt.removeEventListener('click', handler);",
    "elt.removeEventListener('click', handler, true);",
    "Services.obs.addObserver(this, 'topic', true);",
    "Services.obs.addObserver(this, 'topic');",
    "Services.prefs.addObserver('branch', this, true);",
    "Services.prefs.addObserver('branch', this);",
    "array.appendElement(elt);",
    "Services.obs.notifyObservers(obj, 'topic', 'data');",
    "Services.obs.notifyObservers(obj, 'topic');",
    "window.getComputedStyle(elt);",
    "window.getComputedStyle(elt, ':before');"
  ],
  invalid: [
    {
      code: "Services.prefs.clearUserPref('browser.search.custom', false);",
      errors: callError("clearUserPref takes only 1 parameter.")
    },
    {
      code: "Services.removeObserver('notification name', {}, false);",
      errors: callError("removeObserver only takes 2 parameters.")
    },
    {
      code: "Services.removeObserver('notification name', {}, true);",
      errors: callError("removeObserver only takes 2 parameters.")
    },
    {
      code: "Services.io.newURI('http://example.com', null, null);",
      errors: callError("newURI's last parameters are optional.")
    },
    {
      code: "Services.io.newURI('http://example.com', 'utf8', null);",
      errors: callError("newURI's last parameters are optional.")
    },
    {
      code: "Services.io.newURI('http://example.com', null);",
      errors: callError("newURI's last parameters are optional.")
    },
    {
      code: "Services.io.newURI('http://example.com', '', '');",
      errors: callError("newURI's last parameters are optional.")
    },
    {
      code: "Services.io.newURI('http://example.com', '');",
      errors: callError("newURI's last parameters are optional.")
    },
    {
      code: "elt.addEventListener('click', handler, false);",
      errors: callError(
        "addEventListener's third parameter can be omitted when it's false.")
    },
    {
      code: "elt.removeEventListener('click', handler, false);",
      errors: callError(
        "removeEventListener's third parameter can be omitted when it's" +
        " false.")
    },
    {
      code: "Services.obs.addObserver(this, 'topic', false);",
      errors: callError(
        "addObserver's third parameter can be omitted when it's" +
        " false.")
    },
    {
      code: "Services.prefs.addObserver('branch', this, false);",
      errors: callError(
        "addObserver's third parameter can be omitted when it's" +
        " false.")
    },
    {
      code: "array.appendElement(elt, false);",
      errors: callError(
        "appendElement's second parameter can be omitted when it's" +
        " false.")
    },
    {
      code: "Services.obs.notifyObservers(obj, 'topic', null);",
      errors: callError(
        "notifyObservers's third parameter can be omitted.")
    },
    {
      code: "Services.obs.notifyObservers(obj, 'topic', '');",
      errors: callError(
        "notifyObservers's third parameter can be omitted.")
    },
    {
      code: "window.getComputedStyle(elt, null);",
      errors: callError("getComputedStyle's second parameter can be omitted.")
    },
    {
      code: "window.getComputedStyle(elt, '');",
      errors: callError("getComputedStyle's second parameter can be omitted.")
    }
  ]
});
