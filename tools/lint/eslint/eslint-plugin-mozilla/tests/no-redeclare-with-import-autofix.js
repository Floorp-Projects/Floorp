/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/no-redeclare-with-import-autofix");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, output, messageId, options = []) {
  let rv = {
    code,
    errors: [{ messageId }],
    options,
  };
  if (output) {
    rv.output = output;
  }
  return rv;
}

ruleTester.run("no-redeclare-with-import-autofix", rule, {
  valid: [
    'const foo = "whatever";',
    'let foo = "whatever";',
    'const {foo} = {foo: "whatever"};',
    'const {foo} = ChromeUtils.import("foo.jsm")',
    'let {foo} = ChromeUtils.import("foo.jsm")',
    'const {foo} = ChromeUtils.importESModule("foo.sys.mjs")',
    'let {foo} = ChromeUtils.importESModule("foo.sys.mjs")',
  ],
  invalid: [
    invalidCode(
      `var {foo} = ChromeUtils.importESModule("foo.sys.mjs");
var {foo} = ChromeUtils.importESModule("foo.sys.mjs");`,
      'var {foo} = ChromeUtils.importESModule("foo.sys.mjs");\n',
      "duplicateImport"
    ),
    invalidCode(
      `var {foo} = ChromeUtils.import("foo.jsm");
var {foo} = ChromeUtils.import("foo.jsm");`,
      'var {foo} = ChromeUtils.import("foo.jsm");\n',
      "duplicateImport"
    ),

    invalidCode(
      `var {foo} = ChromeUtils.import("foo.jsm");
var {foo, bar} = ChromeUtils.import("foo.jsm");`,
      `var {foo} = ChromeUtils.import("foo.jsm");
var { bar} = ChromeUtils.import("foo.jsm");`,
      "duplicateImport"
    ),

    invalidCode(
      `var {foo} = ChromeUtils.import("foo.jsm");
var {bar, foo} = ChromeUtils.import("foo.jsm");`,
      `var {foo} = ChromeUtils.import("foo.jsm");
var {bar} = ChromeUtils.import("foo.jsm");`,
      "duplicateImport"
    ),

    invalidCode(`var foo = 5; var foo = 10;`, "", "redeclared", [
      {
        errorForNonImports: true,
      },
    ]),
  ],
});
