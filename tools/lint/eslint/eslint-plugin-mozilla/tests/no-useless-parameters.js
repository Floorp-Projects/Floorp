/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/no-useless-parameters");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function callError(messageId, data = {}) {
  return [{ messageId, data, type: "CallExpression" }];
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
    "window.getComputedStyle(elt, ':before');",
  ],
  invalid: [
    {
      code: "Services.prefs.clearUserPref('browser.search.custom', false);",
      output: "Services.prefs.clearUserPref('browser.search.custom');",
      errors: callError("onlyTakes", {
        fnName: "clearUserPref",
        params: "1 parameter",
      }),
    },
    {
      code: "Services.prefs.clearUserPref('browser.search.custom',\n   false);",
      output: "Services.prefs.clearUserPref('browser.search.custom');",
      errors: callError("onlyTakes", {
        fnName: "clearUserPref",
        params: "1 parameter",
      }),
    },
    {
      code: "Services.removeObserver('notification name', {}, false);",
      output: "Services.removeObserver('notification name', {});",
      errors: callError("onlyTakes", {
        fnName: "removeObserver",
        params: "2 parameters",
      }),
    },
    {
      code: "Services.removeObserver('notification name', {}, true);",
      output: "Services.removeObserver('notification name', {});",
      errors: callError("onlyTakes", {
        fnName: "removeObserver",
        params: "2 parameters",
      }),
    },
    {
      code: "Services.io.newURI('http://example.com', null, null);",
      output: "Services.io.newURI('http://example.com');",
      errors: callError("newURIParams"),
    },
    {
      code: "Services.io.newURI('http://example.com', 'utf8', null);",
      output: "Services.io.newURI('http://example.com', 'utf8');",
      errors: callError("newURIParams"),
    },
    {
      code: "Services.io.newURI('http://example.com', null);",
      output: "Services.io.newURI('http://example.com');",
      errors: callError("newURIParams"),
    },
    {
      code: "Services.io.newURI('http://example.com', '', '');",
      output: "Services.io.newURI('http://example.com');",
      errors: callError("newURIParams"),
    },
    {
      code: "Services.io.newURI('http://example.com', '');",
      output: "Services.io.newURI('http://example.com');",
      errors: callError("newURIParams"),
    },
    {
      code: "elt.addEventListener('click', handler, false);",
      output: "elt.addEventListener('click', handler);",
      errors: callError("obmittedWhenFalse", {
        fnName: "addEventListener",
        index: "third",
      }),
    },
    {
      code: "elt.removeEventListener('click', handler, false);",
      output: "elt.removeEventListener('click', handler);",
      errors: callError("obmittedWhenFalse", {
        fnName: "removeEventListener",
        index: "third",
      }),
    },
    {
      code: "Services.obs.addObserver(this, 'topic', false);",
      output: "Services.obs.addObserver(this, 'topic');",
      errors: callError("obmittedWhenFalse", {
        fnName: "addObserver",
        index: "third",
      }),
    },
    {
      code: "Services.prefs.addObserver('branch', this, false);",
      output: "Services.prefs.addObserver('branch', this);",
      errors: callError("obmittedWhenFalse", {
        fnName: "addObserver",
        index: "third",
      }),
    },
    {
      code: "array.appendElement(elt, false);",
      output: "array.appendElement(elt);",
      errors: callError("obmittedWhenFalse", {
        fnName: "appendElement",
        index: "second",
      }),
    },
    {
      code: "Services.obs.notifyObservers(obj, 'topic', null);",
      output: "Services.obs.notifyObservers(obj, 'topic');",
      errors: callError("obmittedWhenFalse", {
        fnName: "notifyObservers",
        index: "third",
      }),
    },
    {
      code: "Services.obs.notifyObservers(obj, 'topic', '');",
      output: "Services.obs.notifyObservers(obj, 'topic');",
      errors: callError("obmittedWhenFalse", {
        fnName: "notifyObservers",
        index: "third",
      }),
    },
    {
      code: "window.getComputedStyle(elt, null);",
      output: "window.getComputedStyle(elt);",
      errors: callError("obmittedWhenFalse", {
        fnName: "getComputedStyle",
        index: "second",
      }),
    },
    {
      code: "window.getComputedStyle(elt, '');",
      output: "window.getComputedStyle(elt);",
      errors: callError("obmittedWhenFalse", {
        fnName: "getComputedStyle",
        index: "second",
      }),
    },
  ],
});
