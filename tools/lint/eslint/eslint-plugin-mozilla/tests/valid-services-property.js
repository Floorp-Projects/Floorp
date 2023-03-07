/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/valid-services-property");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, messageId, data) {
  return { code, errors: [{ messageId, data }] };
}

process.env.MOZ_XPT_ARTIFACTS_DIR = `${__dirname}/xpidl`;

ruleTester.run("valid-services-property", rule, {
  valid: [
    "Services.uriFixup.keywordToURI()",
    "Services.uriFixup.FIXUP_FLAG_NONE",
  ],
  invalid: [
    invalidCode("Services.uriFixup.UNKNOWN_CONSTANT", "unknownProperty", {
      alias: "uriFixup",
      propertyName: "UNKNOWN_CONSTANT",
      checkedInterfaces: ["nsIURIFixup"],
    }),
    invalidCode("Services.uriFixup.foo()", "unknownProperty", {
      alias: "uriFixup",
      propertyName: "foo",
      checkedInterfaces: ["nsIURIFixup"],
    }),
  ],
});
