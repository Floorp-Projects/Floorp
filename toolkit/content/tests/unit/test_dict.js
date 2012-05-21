/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Dict.jsm");

/**
 * Test that a few basic get, set, has and del operations work.
 */
function test_get_set_has_del() {
  let dict = new Dict({foo: "bar"});
  dict.set("baz", 200);
  do_check_eq(dict.get("foo"), "bar");
  do_check_eq(dict.get("baz"), 200);
  do_check_true(dict.has("foo"));
  do_check_true(dict.has("baz"));
  // Now delete the entries
  do_check_true(dict.del("foo"));
  do_check_true(dict.del("baz"));
  do_check_false(dict.has("foo"));
  do_check_false(dict.has("baz"));
  // and make sure del returns false
  do_check_false(dict.del("foo"));
  do_check_false(dict.del("baz"));
}

/**
 * Test that the second parameter of get (default value) works.
 */
function test_get_default() {
  let dict = new Dict();
  do_check_true(dict.get("foo") === undefined);
  do_check_eq(dict.get("foo", "bar"), "bar");
}

/**
 * Test that there are no collisions with builtins.
 */
function test_collisions_with_builtins() {
  let dict = new Dict();
  // First check that a new dictionary doesn't already have builtins.
  do_check_false(dict.has("toString"));
  do_check_false(dict.has("watch"));
  do_check_false(dict.has("__proto__"));

  // Add elements in an attempt to collide with builtins.
  dict.set("toString", "toString");
  dict.set("watch", "watch");
  // This is a little evil. We set __proto__ to an object to try to make it look
  // up the prototype chain.
  dict.set("__proto__", {prototest: "prototest"});

  // Now check that the entries exist.
  do_check_true(dict.has("toString"));
  do_check_true(dict.has("watch"));
  do_check_true(dict.has("__proto__"));
  // ...and that we aren't looking up the prototype chain.
  do_check_false(dict.has("prototest"));
}

/**
 * Test that the "count" property works as expected.
 */
function test_count() {
  let dict = new Dict({foo: "bar"});
  do_check_eq(dict.count, 1);
  dict.set("baz", "quux");
  do_check_eq(dict.count, 2);
  // This shouldn't change the count
  dict.set("baz", "quux2");
  do_check_eq(dict.count, 2);

  do_check_true(dict.del("baz"));
  do_check_eq(dict.count, 1);
  // This shouldn't change the count either
  do_check_false(dict.del("not"));
  do_check_eq(dict.count, 1);
  do_check_true(dict.del("foo"));
  do_check_eq(dict.count, 0);
}

/**
 * Test that the copy function works as expected.
 */
function test_copy() {
  let obj = {};
  let dict1 = new Dict({foo: "bar", baz: obj});
  let dict2 = dict1.copy();
  do_check_eq(dict2.get("foo"), "bar");
  do_check_eq(dict2.get("baz"), obj);
  // Make sure the two update independent of each other.
  dict1.del("foo");
  do_check_false(dict1.has("foo"));
  do_check_true(dict2.has("foo"));
  dict2.set("test", 400);
  do_check_true(dict2.has("test"));
  do_check_false(dict1.has("test"));

  // Check that the copy is shallow and not deep.
  dict1.get("baz").prop = "proptest";
  do_check_eq(dict2.get("baz").prop, "proptest");
}

// This is used by both test_listers and test_iterators.
function _check_lists(keys, values, items) {
  do_check_eq(keys.length, 2);
  do_check_true(keys.indexOf("x") != -1);
  do_check_true(keys.indexOf("y") != -1);

  do_check_eq(values.length, 2);
  do_check_true(values.indexOf("a") != -1);
  do_check_true(values.indexOf("b") != -1);

  // This is a little more tricky -- we need to check that one of the two
  // entries is ["x", "a"] and the other is ["y", "b"].
  do_check_eq(items.length, 2);
  do_check_eq(items[0].length, 2);
  do_check_eq(items[1].length, 2);
  let ix = (items[0][0] == "x") ? 0 : 1;
  let iy = (ix == 0) ? 1 : 0;
  do_check_eq(items[ix][0], "x");
  do_check_eq(items[ix][1], "a");
  do_check_eq(items[iy][0], "y");
  do_check_eq(items[iy][1], "b");
}

/**
 * Test the list functions.
 */
function test_listers() {
  let dict = new Dict({"x": "a", "y": "b"});
  let keys = dict.listkeys();
  let values = dict.listvalues();
  let items = dict.listitems();
  _check_lists(keys, values, items);
}

/**
 * Test the iterator functions.
 */
function test_iterators() {
  let dict = new Dict({"x": "a", "y": "b"});
  // Convert the generators to lists
  let keys = [x for (x in dict.keys)];
  let values = [x for (x in dict.values)];
  let items = [x for (x in dict.items)];
  _check_lists(keys, values, items);
}

/**
 * Test that setting a property throws an exception in strict mode.
 */
function test_set_property_strict() {
  "use strict";
  var dict = new Dict();
  var thrown = false;
  try {
    dict.foo = "bar";
  }
  catch (ex) {
    thrown = true;
  }
  do_check_true(thrown);
}

/**
 * Test that setting a property has no effect in non-strict mode.
 */
function test_set_property_non_strict() {
  let dict = new Dict();
  dict.foo = "bar";
  do_check_false("foo" in dict);
  let realget = dict.get;
  dict.get = "baz";
  do_check_eq(dict.get, realget);
}

/**
 * Tests setting a property by a lazy getter.
 */
function test_set_property_lazy_getter() {
  let thunkCalled = false;

  let setThunk = function(dict) {
    thunkCalled = false;
    dict.setAsLazyGetter("foo", function() {
      thunkCalled = true;
      return "bar";
    });
  };

  let (dict = new Dict()) {
    setThunk(dict);

    // Test that checking for the key existence does not invoke
    // the getter function.
    do_check_true(dict.has("foo"));
    do_check_false(thunkCalled);
    do_check_true(dict.isLazyGetter("foo"));

    // Calling get the first time should invoke the getter function
    // and unmark the key as a lazy getter.
    do_check_eq(dict.get("foo"), "bar");
    do_check_true(thunkCalled);
    do_check_false(dict.isLazyGetter("foo"));

    // Calling get again should not invoke the getter function
    thunkCalled = false;
    do_check_eq(dict.get("foo"), "bar");
    do_check_false(thunkCalled);
    do_check_false(dict.isLazyGetter("foo"));
  }

  // Test that listvalues works for lazy keys.
  let (dict = new Dict()) {
    setThunk(dict);
    do_check_true(dict.isLazyGetter("foo"));

    let (listvalues = dict.listvalues()) {
      do_check_false(dict.isLazyGetter("foo"));
      do_check_true(thunkCalled);
      do_check_true(listvalues.length, 1);
      do_check_eq(listvalues[0], "bar");
    }

    thunkCalled = false;

    // Retrieving the list again shouldn't invoke our getter.
    let (listvalues = dict.listvalues()) {
      do_check_false(dict.isLazyGetter("foo"));
      do_check_false(thunkCalled);
      do_check_true(listvalues.length, 1);
      do_check_eq(listvalues[0], "bar");
    }
  }

  // Test that the values iterator also works as expected.
  let (dict = new Dict()) {
    setThunk(dict);
    let values = dict.values;

    // Our getter shouldn't be called before the iterator reaches it.
    do_check_true(dict.isLazyGetter("foo"));
    do_check_false(thunkCalled);
    do_check_eq(values.next(), "bar");
    do_check_true(thunkCalled);

    thunkCalled = false;
    do_check_false(dict.isLazyGetter("foo"));
    do_check_eq(dict.get("foo"), "bar");
    do_check_false(thunkCalled);
  }
}

var tests = [
  test_get_set_has_del,
  test_get_default,
  test_collisions_with_builtins,
  test_count,
  test_copy,
  test_listers,
  test_iterators,
  test_set_property_strict,
  test_set_property_non_strict,
  test_set_property_lazy_getter
];

function run_test() {
  for (let [, test] in Iterator(tests))
    test();
}
