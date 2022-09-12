/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var os = require("os");
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

const tests = {
  valid: ["Ci.nsIURIFixup", "Ci.nsIURIFixup.FIXUP_FLAG_NONE"],
  invalid: [
    invalidCode("Ci.nsIURIFixup.UNKNOWN_CONSTANT", "unknownProperty", {
      interface: "nsIURIFixup",
      property: "UNKNOWN_CONSTANT",
    }),
    invalidCode("Ci.nsIFoo", "unknownInterface", {
      interface: "nsIFoo",
    }),
  ],
};

// For ESLint tests, we only have a couple of xpt examples in the xpidl directory.
// Therefore we can pretend that these interfaces no longer exist.
switch (os.platform) {
  case "windows":
    tests.invalid.push(
      invalidCode("Ci.nsIJumpListShortcut", "missingInterface")
    );
    break;
  case "darwin":
    tests.invalid.push(
      invalidCode("Ci.nsIMacShellService", "missingInterface")
    );
    break;
  case "linux":
    tests.invalid.push(
      invalidCode("Ci.mozISandboxReporter", "missingInterface")
    );
}

ruleTester.run("valid-ci-uses", rule, tests);
