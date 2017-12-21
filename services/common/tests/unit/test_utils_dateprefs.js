/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://services-common/utils.js");


var prefs = new Preferences("servicescommon.tests.");

function DummyLogger() {
  this.messages = [];
}
DummyLogger.prototype.warn = function warn(message) {
  this.messages.push(message);
};

add_test(function test_set_basic() {
  let now = new Date();

  CommonUtils.setDatePref(prefs, "test00", now);
  let value = prefs.get("test00");
  Assert.equal(value, "" + now.getTime());

  let now2 = CommonUtils.getDatePref(prefs, "test00");

  Assert.equal(now.getTime(), now2.getTime());

  run_next_test();
});

add_test(function test_set_bounds_checking() {
  let d = new Date(2342354);

  let failed = false;
  try {
    CommonUtils.setDatePref(prefs, "test01", d);
  } catch (ex) {
    Assert.ok(ex.message.startsWith("Trying to set"));
    failed = true;
  }

  Assert.ok(failed);
  run_next_test();
});

add_test(function test_get_bounds_checking() {
  prefs.set("test_bounds_checking", "13241431");

  let log = new DummyLogger();
  let d = CommonUtils.getDatePref(prefs, "test_bounds_checking", 0, log);
  Assert.equal(d.getTime(), 0);
  Assert.equal(log.messages.length, 1);

  run_next_test();
});

add_test(function test_get_bad_default() {
  let failed = false;
  try {
    CommonUtils.getDatePref(prefs, "get_bad_default", new Date());
  } catch (ex) {
    Assert.ok(ex.message.startsWith("Default value is not a number"));
    failed = true;
  }

  Assert.ok(failed);
  run_next_test();
});

add_test(function test_get_invalid_number() {
  prefs.set("get_invalid_number", "hello world");

  let log = new DummyLogger();
  let d = CommonUtils.getDatePref(prefs, "get_invalid_number", 42, log);
  Assert.equal(d.getTime(), 42);
  Assert.equal(log.messages.length, 1);

  run_next_test();
});
