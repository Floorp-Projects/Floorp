/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://services-common/utils.js");


let prefs = new Preferences("servicescommon.tests.");

function DummyLogger() {
  this.messages = [];
}
DummyLogger.prototype.warn = function warn(message) {
  this.messages.push(message);
};

function run_test() {
  run_next_test();
}

add_test(function test_set_basic() {
  let now = new Date();

  CommonUtils.setDatePref(prefs, "test00", now);
  let value = prefs.get("test00");
  do_check_eq(value, "" + now.getTime());

  let now2 = CommonUtils.getDatePref(prefs, "test00");

  do_check_eq(now.getTime(), now2.getTime());

  run_next_test();
});

add_test(function test_set_bounds_checking() {
  let d = new Date(2342354);

  let failed = false;
  try {
    CommonUtils.setDatePref(prefs, "test01", d);
  } catch (ex) {
    do_check_true(ex.message.startsWith("Trying to set"));
    failed = true;
  }

  do_check_true(failed);
  run_next_test();
});

add_test(function test_get_bounds_checking() {
  prefs.set("test_bounds_checking", "13241431");

  let log = new DummyLogger();
  let d = CommonUtils.getDatePref(prefs, "test_bounds_checking", 0, log);
  do_check_eq(d.getTime(), 0);
  do_check_eq(log.messages.length, 1);

  run_next_test();
});

add_test(function test_get_bad_default() {
  let failed = false;
  try {
    CommonUtils.getDatePref(prefs, "get_bad_default", new Date());
  } catch (ex) {
    do_check_true(ex.message.startsWith("Default value is not a number"));
    failed = true;
  }

  do_check_true(failed);
  run_next_test();
});

add_test(function test_get_invalid_number() {
  prefs.set("get_invalid_number", "hello world");

  let log = new DummyLogger();
  let d = CommonUtils.getDatePref(prefs, "get_invalid_number", 42, log);
  do_check_eq(d.getTime(), 42);
  do_check_eq(log.messages.length, 1);

  run_next_test();
});
