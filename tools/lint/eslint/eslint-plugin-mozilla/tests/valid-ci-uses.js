/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/valid-ci-uses");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 13 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, messageId, data) {
  return { code, errors: [{ messageId, data }] };
}

process.env.TEST_XPIDLDIR = `${__dirname}/xpidl`;

ruleTester.run("valid-ci-uses", rule, {
  valid: ["Ci.nsIURIFixup.FIXUP_FLAG_NONE"],
  invalid: [
    invalidCode("Ci.nsIURIFixup.UNKNOWN_CONSTANT", "unknownProperty", {
      interface: "nsIURIFixup",
      property: "UNKNOWN_CONSTANT",
    }),
  ],
});
