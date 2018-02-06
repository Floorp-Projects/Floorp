/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/no-define-cc-etc");
var RuleTester = require("eslint/lib/testers/rule-tester");

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, varName) {
  return {code, errors: [{
    message: `${varName} is now defined in global scope, a separate definition is no longer necessary.`,
    type: "VariableDeclarator"
  }]};
}

ruleTester.run("no-define-cc-etc", rule, {
  valid: [
    "var Cm = Components.manager;",
    "const CC = Components.Constructor;",
    "var {CC: Constructor, Cm: manager} = Components;",
    "const {CC: Constructor, Cm: manager} = Components;",
    "foo.Cc.test();"
  ],
  invalid: [
    invalidCode("var Cc;", "Cc"),
    invalidCode("let Cc;", "Cc"),
    invalidCode("let Ci;", "Ci"),
    invalidCode("let Cr;", "Cr"),
    invalidCode("let Cu;", "Cu"),
    invalidCode("var Cc = Components.classes;", "Cc"),
    invalidCode("const {Cc} = Components;", "Cc"),
    invalidCode("let {Cc, Cm} = Components", "Cc"),
    invalidCode("const Cu = Components.utils;", "Cu"), {
      code: "var {Ci: interfaces, Cc: classes} = Components;",
      errors: [{
        message: `Ci is now defined in global scope, a separate definition is no longer necessary.`,
        type: "VariableDeclarator"
      }, {
        message: `Cc is now defined in global scope, a separate definition is no longer necessary.`,
        type: "VariableDeclarator"
      }]
    }
  ]
});
