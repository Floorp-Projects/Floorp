/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/valid-lazy");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, name, messageId) {
  return { code, errors: [{ messageId, data: { name } }] };
}

ruleTester.run("valid-lazy", rule, {
  // Note: these tests build on top of one another, although lazy gets
  // re-declared, it
  valid: [
    `
      const lazy = {};
      XPCOMUtils.defineLazyGetter(lazy, "foo", () => {});
      lazy.foo.bar();
    `,
    `
      const lazy = {};
      XPCOMUtils.defineLazyModuleGetters(lazy, {
        foo: "foo.jsm",
      });
      lazy.foo.bar();
    `,
    `
      const lazy = {};
      ChromeUtils.defineESModuleGetters(lazy, {
        foo: "foo.mjs",
      });
      lazy.foo.bar();
    `,
    `
      const lazy = {};
      Integration.downloads.defineModuleGetter(lazy, "foo", "foo.jsm");
      lazy.foo.bar();
    `,
    `
      const lazy = createLazyLoaders({ foo: () => {}});
      lazy.foo.bar();
    `,
    `
      const lazy = {};
      loader.lazyRequireGetter(
        lazy,
        ["foo1", "foo2"],
        "bar",
        true
      );
      lazy.foo1.bar();
      lazy.foo2.bar();
    `,
  ],
  invalid: [
    invalidCode("lazy.bar", "bar", "unknownProperty"),
    invalidCode(
      `
        const lazy = {};
        XPCOMUtils.defineLazyGetter(lazy, "foo", "foo.jsm");
        XPCOMUtils.defineLazyGetter(lazy, "foo", "foo1.jsm");
        lazy.foo.bar();
      `,
      "foo",
      "duplicateSymbol"
    ),
    invalidCode(
      `
        const lazy = {};
        XPCOMUtils.defineLazyModuleGetters(lazy, {
          "foo-bar": "foo.jsm",
        });
        lazy["foo-bar"].bar();
      `,
      "foo-bar",
      "incorrectType"
    ),
    invalidCode(
      `const lazy = {};
       XPCOMUtils.defineLazyGetter(lazy, "foo", "foo.jsm");
      `,
      "foo",
      "unusedProperty"
    ),
  ],
});
