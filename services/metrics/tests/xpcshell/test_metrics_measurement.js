/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {utils: Cu} = Components;

Cu.import("resource://testing-common/services/metrics/mocks.jsm");


function run_test() {
  run_next_test();
};

add_test(function test_constructor() {
  let m = new DummyMeasurement();
  do_check_eq(m.name, "DummyMeasurement");
  do_check_eq(m.version, 2);

  run_next_test();
});

add_test(function test_add_string() {
  let m = new DummyMeasurement();

  m.setValue("string", "hello world");
  do_check_eq(m.getValue("string"), "hello world");

  let failed = false;
  try {
    m.setValue("string", 46);
  } catch (ex) {
    do_check_true(ex.message.startsWith("STRING field expects a string"));
    failed = true;
  } finally {
    do_check_true(failed);
  }

  run_next_test();
});

add_test(function test_add_uint32() {
  let m = new DummyMeasurement();

  m.setValue("uint32", 52342);
  do_check_eq(m.getValue("uint32"), 52342);

  let failed = false;
  try {
    m.setValue("uint32", -1);
  } catch (ex) {
    failed = true;
    do_check_true(ex.message.startsWith("UINT32 field expects a positive"));
  } finally {
    do_check_true(failed);
    failed = false;
  }

  try {
    m.setValue("uint32", "foo");
  } catch (ex) {
    failed = true;
    do_check_true(ex.message.startsWith("UINT32 field expects an integer"));
  } finally {
    do_check_true(failed);
    failed = false;
  }

  try {
    m.setValue("uint32", Math.pow(2, 32));
  } catch (ex) {
    failed = true;
    do_check_true(ex.message.startsWith("Value is too large"));
  } finally {
    do_check_true(failed);
    failed = false;
  }

  run_next_test();
});

add_test(function test_validate() {
  let m = new DummyMeasurement();

  let failed = false;
  try {
    m.validate();
  } catch (ex) {
    failed = true;
    do_check_true(ex.message.startsWith("Required field not defined"));
  } finally {
    do_check_true(failed);
    failed = false;
  }

  run_next_test();
});

add_test(function test_toJSON() {
  let m = new DummyMeasurement();

  m.setValue("string", "foo bar");

  let json = JSON.parse(JSON.stringify(m));
  do_check_eq(Object.keys(json).length, 3);
  do_check_eq(json.name, "DummyMeasurement");
  do_check_eq(json.version, 2);
  do_check_true("fields" in json);

  do_check_eq(Object.keys(json.fields).length, 1);
  do_check_eq(json.fields.string, "foo bar");

  run_next_test();
});

