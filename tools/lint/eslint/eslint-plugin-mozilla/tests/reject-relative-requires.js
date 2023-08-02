/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/reject-relative-requires");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidError() {
  return [{ messageId: "rejectRelativeRequires", type: "CallExpression" }];
}

ruleTester.run("reject-relative-requires", rule, {
  valid: [
    'require("devtools/absolute/path")',
    'require("resource://gre/modules/SomeModule.jsm")',
    'loader.lazyRequireGetter(this, "path", "devtools/absolute/path", true)',
    'loader.lazyRequireGetter(this, "Path", "devtools/absolute/path")',
  ],
  invalid: [
    {
      code: 'require("./relative/path")',
      errors: invalidError(),
    },
    {
      code: 'require("../parent/folder/path")',
      errors: invalidError(),
    },
    {
      code: 'loader.lazyRequireGetter(this, "path", "./relative/path", true)',
      errors: invalidError(),
    },
    {
      code: 'loader.lazyRequireGetter(this, "path", "../parent/folder/path", true)',
      errors: invalidError(),
    },
    {
      code: 'loader.lazyRequireGetter(this, "path", "./relative/path")',
      errors: invalidError(),
    },
    {
      code: 'loader.lazyRequireGetter(this, "path", "../parent/folder/path")',
      errors: invalidError(),
    },
  ],
});
