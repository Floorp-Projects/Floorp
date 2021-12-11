/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/* eslint-disable no-array-constructor, no-new-object */

const { assert } = ChromeUtils.import(
  "chrome://remote/content/shared/webdriver/Assert.jsm"
);
const { error } = ChromeUtils.import(
  "chrome://remote/content/shared/webdriver/Errors.jsm"
);

add_test(function test_session() {
  assert.session({ id: "foo" });

  const invalidTypes = [
    null,
    undefined,
    [],
    {},
    { id: undefined },
    { id: null },
    { id: true },
    { id: 1 },
    { id: [] },
    { id: {} },
  ];

  for (const invalidType of invalidTypes) {
    Assert.throws(() => assert.session(invalidType), /InvalidSessionIDError/);
  }

  Assert.throws(() => assert.session({ id: null }, "custom"), /custom/);

  run_next_test();
});

add_test(function test_platforms() {
  // at least one will fail
  let raised;
  for (let fn of [assert.desktop, assert.mobile]) {
    try {
      fn();
    } catch (e) {
      raised = e;
    }
  }
  ok(raised instanceof error.UnsupportedOperationError);

  run_next_test();
});

add_test(function test_noUserPrompt() {
  assert.noUserPrompt(null);
  assert.noUserPrompt(undefined);
  Assert.throws(() => assert.noUserPrompt({}), /UnexpectedAlertOpenError/);
  Assert.throws(() => assert.noUserPrompt({}, "custom"), /custom/);

  run_next_test();
});

add_test(function test_defined() {
  assert.defined({});
  Assert.throws(() => assert.defined(undefined), /InvalidArgumentError/);
  Assert.throws(() => assert.noUserPrompt({}, "custom"), /custom/);

  run_next_test();
});

add_test(function test_number() {
  assert.number(1);
  assert.number(0);
  assert.number(-1);
  assert.number(1.2);
  for (let i of ["foo", "1", {}, [], NaN, Infinity, undefined]) {
    Assert.throws(() => assert.number(i), /InvalidArgumentError/);
  }

  Assert.throws(() => assert.number("foo", "custom"), /custom/);

  run_next_test();
});

add_test(function test_callable() {
  assert.callable(function() {});
  assert.callable(() => {});

  for (let typ of [undefined, "", true, {}, []]) {
    Assert.throws(() => assert.callable(typ), /InvalidArgumentError/);
  }

  Assert.throws(() => assert.callable("foo", "custom"), /custom/);

  run_next_test();
});

add_test(function test_integer() {
  assert.integer(1);
  assert.integer(0);
  assert.integer(-1);
  Assert.throws(() => assert.integer("foo"), /InvalidArgumentError/);
  Assert.throws(() => assert.integer(1.2), /InvalidArgumentError/);

  Assert.throws(() => assert.integer("foo", "custom"), /custom/);

  run_next_test();
});

add_test(function test_positiveInteger() {
  assert.positiveInteger(1);
  assert.positiveInteger(0);
  Assert.throws(() => assert.positiveInteger(-1), /InvalidArgumentError/);
  Assert.throws(() => assert.positiveInteger("foo"), /InvalidArgumentError/);
  Assert.throws(() => assert.positiveInteger("foo", "custom"), /custom/);

  run_next_test();
});

add_test(function test_boolean() {
  assert.boolean(true);
  assert.boolean(false);
  Assert.throws(() => assert.boolean("false"), /InvalidArgumentError/);
  Assert.throws(() => assert.boolean(undefined), /InvalidArgumentError/);
  Assert.throws(() => assert.boolean(undefined, "custom"), /custom/);

  run_next_test();
});

add_test(function test_string() {
  assert.string("foo");
  assert.string(`bar`);
  Assert.throws(() => assert.string(42), /InvalidArgumentError/);
  Assert.throws(() => assert.string(42, "custom"), /custom/);

  run_next_test();
});

add_test(function test_open() {
  assert.open({ currentWindowGlobal: {} });

  for (let typ of [null, undefined, { currentWindowGlobal: null }]) {
    Assert.throws(() => assert.open(typ), /NoSuchWindowError/);
  }

  Assert.throws(() => assert.open(null, "custom"), /custom/);

  run_next_test();
});

add_test(function test_object() {
  assert.object({});
  assert.object(new Object());
  for (let typ of [42, "foo", true, null, undefined]) {
    Assert.throws(() => assert.object(typ), /InvalidArgumentError/);
  }

  Assert.throws(() => assert.object(null, "custom"), /custom/);

  run_next_test();
});

add_test(function test_in() {
  assert.in("foo", { foo: 42 });
  for (let typ of [{}, 42, true, null, undefined]) {
    Assert.throws(() => assert.in("foo", typ), /InvalidArgumentError/);
  }

  Assert.throws(() => assert.in("foo", { bar: 42 }, "custom"), /custom/);

  run_next_test();
});

add_test(function test_array() {
  assert.array([]);
  assert.array(new Array());
  Assert.throws(() => assert.array(42), /InvalidArgumentError/);
  Assert.throws(() => assert.array({}), /InvalidArgumentError/);

  Assert.throws(() => assert.array(42, "custom"), /custom/);

  run_next_test();
});

add_test(function test_that() {
  equal(1, assert.that(n => n + 1)(1));
  Assert.throws(() => assert.that(() => false)(), /InvalidArgumentError/);
  Assert.throws(() => assert.that(val => val)(false), /InvalidArgumentError/);
  Assert.throws(
    () => assert.that(val => val, "foo", error.SessionNotCreatedError)(false),
    /SessionNotCreatedError/
  );

  Assert.throws(() => assert.that(() => false, "custom")(), /custom/);

  run_next_test();
});

/* eslint-enable no-array-constructor, no-new-object */
