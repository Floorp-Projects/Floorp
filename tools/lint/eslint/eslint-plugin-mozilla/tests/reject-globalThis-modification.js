/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/reject-globalThis-modification");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCall(code) {
  return {
    code,
    errors: [
      {
        messageId: "rejectPassingGlobalThis",
        type: "CallExpression",
      },
    ],
  };
}

function invalidAssignment(code) {
  return {
    code,
    errors: [
      {
        messageId: "rejectModifyGlobalThis",
        type: "AssignmentExpression",
      },
    ],
  };
}

ruleTester.run("reject-globalThis-modification", rule, {
  valid: [
    `var x = globalThis.Array;`,
    `Array in globalThis;`,
    `result.deserialize(globalThis)`,
  ],
  invalid: [
    invalidAssignment(`
    globalThis.foo = 10;
`),
    invalidCall(`
    Object.defineProperty(globalThis, "foo", { value: 10 });
`),
    invalidCall(`
    Object.defineProperties(globalThis, {
      foo: { value: 10 },
    });
`),
    invalidCall(`
    Object.assign(globalThis, { foo: 10 });
`),
    invalidCall(`
    ChromeUtils.defineModuleGetter(
      globalThis, "AppConstants", "resource://gre/modules/AppConstants.jsm"
    );
`),
    invalidCall(`
    ChromeUtils.defineESMGetters(globalThis, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
    });
`),
    invalidCall(`
    XPCOMUtils.defineLazyModuleGetters(globalThis, {
      AppConstants: "resource://gre/modules/AppConstants.jsm",
    });
`),
    invalidCall(`
    someFunction(1, globalThis);
`),
  ],
});
