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
      code: `const { AppConstants } = ChromeUtils.importESM("resource://gre/modules/AppConstants.sys.mjs");`,
    },
  ],
  invalid: [
    {
      code: `import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";`,
      errors: [{ messageId: "rejectStaticImportSystemModuleFromNonSystem" }],
    },
  ],
});
