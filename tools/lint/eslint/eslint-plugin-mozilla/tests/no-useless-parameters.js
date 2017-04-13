/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/no-useless-parameters");

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function callError(message) {
  return [{message, type: "CallExpression"}];
}

exports.runTest = function(ruleTester) {
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
        code: "window.getComputedStyle(elt, null);",
        errors: callError("getComputedStyle's second parameter can be omitted.")
      },
      {
        code: "window.getComputedStyle(elt, '');",
        errors: callError("getComputedStyle's second parameter can be omitted.")
      }
    ]
  });
};
