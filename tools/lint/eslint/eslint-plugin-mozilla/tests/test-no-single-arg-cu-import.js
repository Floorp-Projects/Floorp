/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var rule = require("../lib/rules/no-single-arg-cu-import");

//------------------------------------------------------------------------------
// Tests
//------------------------------------------------------------------------------

const ExpectedError = {
  message: "Single argument Cu.import exposes new globals to all modules",
  type: "CallExpression"
}

exports.runTest = function(ruleTester) {
  ruleTester.run("no-single-arg-cu-import", rule, {
    valid: [
      "Cu.import('fake', {});",
    ],
    invalid: [{
      code: "Cu.import('fake');",
      errors: [ExpectedError]
    }]
  });
};
