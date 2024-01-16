/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/use-console-createInstance");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

ruleTester.run("use-console-createInstance", rule, {
  valid: ['"resource://gre/modules/Foo.sys.mjs"'],
  invalid: [
    {
      code: '"resource://gre/modules/Console.sys.mjs"',
      errors: [
        {
          messageId: "useConsoleRatherThanModule",
          data: { module: "Console.sys.mjs" },
        },
      ],
    },
    {
      code: '"resource://gre/modules/Log.sys.mjs"',
      errors: [
        {
          messageId: "useConsoleRatherThanModule",
          data: { module: "Log.sys.mjs" },
        },
      ],
    },
  ],
});
