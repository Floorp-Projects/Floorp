/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/use-isInstance");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

const errors = [
  {
    message:
      "Please prefer .isInstance() in chrome scripts over the standard instanceof operator for DOM interfaces, " +
      "since the latter will return false when the object is created from a different context.",
    type: "BinaryExpression",
  },
];

ruleTester.run("use-isInstance", rule, {
  valid: [
    "(() => {}) instanceof Function;",
    "({}) instanceof Object;",
    "Node instanceof Object;",
    "file instanceof OS.File;",
    "file instanceof OS.File.Error;",
    "file instanceof lazy.OS.File;",
    "file instanceof lazy.OS.File.Error;",
    "file instanceof lazy.lazy.OS.File;",
  ],
  invalid: [
    {
      code: "node instanceof Node",
      output: "Node.isInstance(node)",
      errors,
    },
    {
      code: "text instanceof win.Text",
      output: "win.Text.isInstance(text)",
      errors,
    },
    {
      code: "target instanceof this.contentWindow.HTMLAudioElement",
      output: "this.contentWindow.HTMLAudioElement.isInstance(target)",
      errors,
    },
    {
      code: "target instanceof File",
      output: "File.isInstance(target)",
      errors,
    },
    {
      code: "target instanceof win.File",
      output: "win.File.isInstance(target)",
      errors,
    },
    {
      code: "window.arguments[0] instanceof window.XULElement",
      output: "window.XULElement.isInstance(window.arguments[0])",
      errors,
    },
  ],
});
