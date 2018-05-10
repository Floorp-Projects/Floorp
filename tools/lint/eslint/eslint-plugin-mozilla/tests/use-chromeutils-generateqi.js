/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/use-chromeutils-generateqi");
var RuleTester = require("eslint/lib/testers/rule-tester");

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function callError(message) {
  return [{message, type: "CallExpression"}];
}
function error(message, type) {
  return [{message, type}];
}

const MSG_NO_JS_QUERY_INTERFACE = (
  "Please use ChromeUtils.generateQI rather than manually creating " +
  "JavaScript QueryInterface functions");

const MSG_NO_XPCOMUTILS_GENERATEQI = (
  "Please use ChromeUtils.generateQI instead of XPCOMUtils.generateQI");

/* globals nsIFlug */
function QueryInterface(iid) {
  if (iid.equals(Ci.nsISupports) ||
      iid.equals(Ci.nsIMeh) ||
      iid.equals(nsIFlug) ||
      iid.equals(Ci.amIFoo)) {
    return this;
  }
  throw Cr.NS_ERROR_NO_INTERFACE;
}

ruleTester.run("use-chromeutils-generateqi", rule, {
  valid: [
    `X.prototype.QueryInterface = ChromeUtils.generateQI(["nsIMeh"]);`,
    `X.prototype = { QueryInterface: ChromeUtils.generateQI(["nsIMeh"]) }`
  ],
  invalid: [
    {
      code: `X.prototype.QueryInterface = XPCOMUtils.generateQI(["nsIMeh"]);`,
      output: `X.prototype.QueryInterface = ChromeUtils.generateQI(["nsIMeh"]);`,
      errors: callError(MSG_NO_XPCOMUTILS_GENERATEQI)
    },
    {
      code: `X.prototype = { QueryInterface: XPCOMUtils.generateQI(["nsIMeh"]) };`,
      output: `X.prototype = { QueryInterface: ChromeUtils.generateQI(["nsIMeh"]) };`,
      errors: callError(MSG_NO_XPCOMUTILS_GENERATEQI)
    },
    {
      code: `X.prototype = { QueryInterface: ${QueryInterface} };`,
      output: `X.prototype = { QueryInterface: ChromeUtils.generateQI(["nsIMeh", "nsIFlug", "amIFoo"]) };`,
      errors: error(MSG_NO_JS_QUERY_INTERFACE, "Property")
    },
    {
      code: `X.prototype = { ${String(QueryInterface).replace(/^function /, "")} };`,
      output: `X.prototype = { QueryInterface: ChromeUtils.generateQI(["nsIMeh", "nsIFlug", "amIFoo"]) };`,
      errors: error(MSG_NO_JS_QUERY_INTERFACE, "Property")
    },
    {
      code: `X.prototype.QueryInterface = ${QueryInterface};`,
      output: `X.prototype.QueryInterface = ChromeUtils.generateQI(["nsIMeh", "nsIFlug", "amIFoo"]);`,
      errors: error(MSG_NO_JS_QUERY_INTERFACE, "AssignmentExpression")
    }
  ]
});

