/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;

Cu.import("chrome://marionette/content/assert.js");
Cu.import("chrome://marionette/content/error.js");

add_test(function test_session() {
  assert.session({sessionId: "foo"});
  for (let typ of [null, undefined, ""]) {
    Assert.throws(() => assert.session({sessionId: typ}), InvalidSessionIDError);
  }

  run_next_test();
});

add_test(function test_platforms() {
  // at least one will fail
  let raised;
  for (let fn of [assert.firefox, assert.fennec, assert.b2g, assert.mobile]) {
    try {
      fn();
    } catch (e) {
      raised = e;
    }
  }
  ok(raised instanceof UnsupportedOperationError);

  run_next_test();
});

add_test(function test_defined() {
  assert.defined({});
  Assert.throws(() => assert.defined(undefined), InvalidArgumentError);

  run_next_test();
});

add_test(function test_number() {
  assert.number(1);
  assert.number(0);
  assert.number(-1);
  assert.number(1.2);
  for (let i of ["foo", "1", {}, [], NaN, Infinity, undefined]) {
    Assert.throws(() => assert.number(i), InvalidArgumentError);
  }
  run_next_test();
});

add_test(function test_callable() {
  assert.callable(function () {});
  assert.callable(() => {});

  for (let typ of [undefined, "", true, {}, []]) {
    Assert.throws(() => assert.callable(typ), InvalidArgumentError);
  }

  run_next_test();
});

add_test(function test_integer() {
  assert.integer(1);
  assert.integer(0);
  assert.integer(-1);
  Assert.throws(() => assert.integer("foo"), InvalidArgumentError);
  Assert.throws(() => assert.integer(1.2), InvalidArgumentError);

  run_next_test();
});

add_test(function test_positiveInteger() {
  assert.positiveInteger(1);
  assert.positiveInteger(0);
  Assert.throws(() => assert.positiveInteger(-1), InvalidArgumentError);
  Assert.throws(() => assert.positiveInteger("foo"), InvalidArgumentError);

  run_next_test();
});

add_test(function test_boolean() {
  assert.boolean(true);
  assert.boolean(false);
  Assert.throws(() => assert.boolean("false"), InvalidArgumentError);
  Assert.throws(() => assert.boolean(undefined), InvalidArgumentError);

  run_next_test();
});

add_test(function test_string() {
  assert.string("foo");
  assert.string(`bar`);
  Assert.throws(() => assert.string(42), InvalidArgumentError);

  run_next_test();
});

add_test(function test_object() {
  assert.object({});
  assert.object(new Object());
  for (let typ of [42, "foo", true, null, undefined]) {
    Assert.throws(() => assert.object(typ), InvalidArgumentError);
  }

  run_next_test();
});

add_test(function test_in() {
  assert.in("foo", {foo: 42});
  for (let typ of [{}, 42, true, null, undefined]) {
    Assert.throws(() => assert.in("foo", typ), InvalidArgumentError);
  }

  run_next_test();
});

add_test(function test_array() {
  assert.array([]);
  assert.array(new Array());
  Assert.throws(() => assert.array(42), InvalidArgumentError);
  Assert.throws(() => assert.array({}), InvalidArgumentError);

  run_next_test();
});

add_test(function test_that() {
  equal(1, assert.that(n => n + 1)(1));
  Assert.throws(() => assert.that(() => false)());
  Assert.throws(() => assert.that(val => val)(false));
  Assert.throws(() => assert.that(val => val, "foo", SessionNotCreatedError)(false),
      SessionNotCreatedError);

  run_next_test();
});
