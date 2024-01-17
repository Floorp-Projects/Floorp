/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/valid-lazy");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

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
       ChromeUtils.defineLazyGetter(lazy, "foo", () => {});
       if (x) { lazy.foo.bar(); }
     `,
    `
       const lazy = {};
       ChromeUtils.defineESModuleGetters(lazy, {
         foo: "foo.mjs",
       });
       if (x) { lazy.foo.bar(); }
     `,
    `
       const lazy = {};
       Integration.downloads.defineESModuleGetter(lazy, "foo", "foo.sys.mjs");
       if (x) { lazy.foo.bar(); }
     `,
    `
       const lazy = createLazyLoaders({ foo: () => {}});
       if (x) { lazy.foo.bar(); }
     `,
    `
       const lazy = {};
       loader.lazyRequireGetter(
         lazy,
         ["foo1", "foo2"],
         "bar",
         true
       );
       if (x) {
         lazy.foo1.bar();
         lazy.foo2.bar();
       }
     `,
    // Test for top-level unconditional.
    `
       const lazy = {};
       ChromeUtils.defineLazyGetter(lazy, "foo", () => {});
       if (x) { lazy.foo.bar(); }
       for (;;) { lazy.foo.bar(); }
       for (var x in y) { lazy.foo.bar(); }
       for (var x of y) { lazy.foo.bar(); }
       while (true) { lazy.foo.bar(); }
       do { lazy.foo.bar(); } while (true);
       switch (x) { case 1: lazy.foo.bar(); }
       try { lazy.foo.bar(); } catch (e) {}
       function f() { lazy.foo.bar(); }
       (function f() { lazy.foo.bar(); });
       () => { lazy.foo.bar(); };
       class C {
         constructor() { lazy.foo.bar(); }
         foo() { lazy.foo.bar(); }
         get x() { lazy.foo.bar(); }
         set x(v) { lazy.foo.bar(); }
         a = lazy.foo.bar();
         #b = lazy.foo.bar();
         static {
           lazy.foo.bar();
         }
       }
       a && lazy.foo.bar();
       a || lazy.foo.bar();
       a ?? lazy.foo.bar();
       a ? lazy.foo.bar() : b;
       a?.b[lazy.foo.bar()];
       a ||= lazy.foo.bar();
       a &&= lazy.foo.bar();
       a ??= lazy.foo.bar();
       var { x = lazy.foo.bar() } = {};
       var [ y = lazy.foo.bar() ] = [];
     `,
  ],
  invalid: [
    invalidCode("if (x) { lazy.bar; }", "bar", "unknownProperty"),
    invalidCode(
      `
         const lazy = {};
         ChromeUtils.defineLazyGetter(lazy, "foo", "foo.jsm");
         ChromeUtils.defineLazyGetter(lazy, "foo", "foo1.jsm");
         if (x) { lazy.foo.bar(); }
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
         if (x) { lazy["foo-bar"].bar(); }
       `,
      "foo-bar",
      "incorrectType"
    ),
    invalidCode(
      `const lazy = {};
        ChromeUtils.defineLazyGetter(lazy, "foo", "foo.jsm");
       `,
      "foo",
      "unusedProperty"
    ),
    invalidCode(
      `const lazy = {};
       ChromeUtils.defineLazyGetter(lazy, "foo1", () => {});
       lazy.foo1.bar();`,
      "foo1",
      "topLevelAndUnconditional"
    ),
    invalidCode(
      `const lazy = {};
       ChromeUtils.defineLazyGetter(lazy, "foo1", () => {});
       { x = -f(1 + lazy.foo1.bar()); }`,
      "foo1",
      "topLevelAndUnconditional"
    ),
  ],
});
