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

const env = { browser: true };

/**
 * A test case boilerplate simulating chrome privileged script.
 * @param {string} code
 */
function mockChromeScript(code) {
  return {
    code,
    filename: "foo.sys.mjs",
    env,
  };
}

/**
 * A test case boilerplate simulating content script.
 * @param {string} code
 */
function mockContentScript(code) {
  return {
    code,
    filename: "foo.js",
    env,
  };
}

ruleTester.run("use-isInstance", rule, {
  valid: [
    mockChromeScript("(() => {}) instanceof Function;"),
    mockChromeScript("({}) instanceof Object;"),
    mockChromeScript("Node instanceof Object;"),
    mockChromeScript("node instanceof lazy.Node;"),
    mockChromeScript("var Node;node instanceof Node;"),
    mockChromeScript("file instanceof lazy.File;"),
    mockChromeScript("file instanceof OS.File;"),
    mockChromeScript("file instanceof OS.File.Error;"),
    mockChromeScript("file instanceof lazy.OS.File;"),
    mockChromeScript("file instanceof lazy.OS.File.Error;"),
    mockChromeScript("file instanceof lazy.lazy.OS.File;"),
    mockChromeScript("var File;file instanceof File;"),
    mockChromeScript("foo instanceof RandomGlobalThing;"),
    mockChromeScript("foo instanceof lazy.RandomGlobalThing;"),
    mockContentScript("node instanceof Node;"),
    mockContentScript("file instanceof File;"),
    mockContentScript(
      "SpecialPowers.ChromeUtils.import(''); file instanceof File;"
    ),
  ],
  invalid: [
    {
      code: "node instanceof Node",
      output: "Node.isInstance(node)",
      env,
      errors,
      filename: "foo.sys.mjs",
    },
    {
      code: "text instanceof win.Text",
      output: "win.Text.isInstance(text)",
      errors,
      filename: "foo.sys.mjs",
    },
    {
      code: "target instanceof this.contentWindow.HTMLAudioElement",
      output: "this.contentWindow.HTMLAudioElement.isInstance(target)",
      errors,
      filename: "foo.sys.mjs",
    },
    {
      code: "target instanceof File",
      output: "File.isInstance(target)",
      env,
      errors,
      filename: "foo.sys.mjs",
    },
    {
      code: "target instanceof win.File",
      output: "win.File.isInstance(target)",
      errors,
      filename: "foo.sys.mjs",
    },
    {
      code: "window.arguments[0] instanceof window.XULElement",
      output: "window.XULElement.isInstance(window.arguments[0])",
      errors,
      filename: "foo.sys.mjs",
    },
    {
      code: "ChromeUtils.import(''); node instanceof Node",
      output: "ChromeUtils.import(''); Node.isInstance(node)",
      env,
      errors,
      filename: "foo.js",
    },
    {
      code: "ChromeUtils.import(''); file instanceof File",
      output: "ChromeUtils.import(''); File.isInstance(file)",
      env,
      errors,
      filename: "foo.js",
    },
  ],
});
