/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/reject-import-system-module-from-non-system");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({
  parserOptions: { ecmaVersion: 13, sourceType: "module" },
});

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

ruleTester.run("reject-import-system-module-from-non-system", rule, {
  valid: [
    {
      code: `const { Services } = ChromeUtils.importESM("resource://gre/modules/Services.sys.mjs");`,
    },
  ],
  invalid: [
    {
      code: `import { Services } from "resource://gre/modules/Services.sys.mjs";`,
      errors: [{ messageId: "rejectStaticImportSystemModuleFromNonSystem" }],
    },
  ],
});
