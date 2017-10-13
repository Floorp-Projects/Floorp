/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/no-cpows-in-tests");
var RuleTester = require("eslint/lib/testers/rule-tester");

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function wrapCode(code, filename = "browser_fake.js") {
  return {code, filename};
}

function invalidCode(code, item) {
  let message = `${item} is a possible Cross Process Object Wrapper (CPOW).`;
  let obj = wrapCode(code);
  obj.errors = [{message, type: "MemberExpression"}];
  return obj;
}

ruleTester.run("no-cpows-in-tests", rule, {
  valid: [
    "window.document",
    wrapCode("ContentTask.spawn(browser, null, () => { content.document; });")
  ],
  invalid: [
    invalidCode("let x = gBrowser.contentWindow;", "gBrowser.contentWindow"),
    invalidCode("let x = gBrowser.contentDocument;", "gBrowser.contentDocument"),
    invalidCode("let x = gBrowser.selectedBrowser.contentWindow;", "gBrowser.selectedBrowser.contentWindow"),
    invalidCode("let x = browser.contentDocument;", "browser.contentDocument"),
    invalidCode("let x = window.content;", "window.content")
  ]
});
