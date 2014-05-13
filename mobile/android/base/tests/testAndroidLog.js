// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(function test_AndroidLog() {
  Components.utils.import("resource://gre/modules/AndroidLog.jsm");

  do_check_true(!!AndroidLog);

  do_check_true("v" in AndroidLog && typeof AndroidLog.v == "function");
  do_check_true("d" in AndroidLog && typeof AndroidLog.d == "function");
  do_check_true("i" in AndroidLog && typeof AndroidLog.i == "function");
  do_check_true("w" in AndroidLog && typeof AndroidLog.w == "function");
  do_check_true("e" in AndroidLog && typeof AndroidLog.e == "function");

  // I don't know how to check that these messages actually make it to the log,
  // but at least we can ensure that they don't cause the test process to crash
  // (because of some change to the native object being accessed via ctypes)
  // and return the right values (the number of bytes--not characters--logged).
  do_check_eq(48, AndroidLog.v("AndroidLogTest", "This is a verbose message."));
  do_check_eq(46, AndroidLog.d("AndroidLogTest", "This is a debug message."));
  do_check_eq(46, AndroidLog.i("AndroidLogTest", "This is an info message."));
  do_check_eq(48, AndroidLog.w("AndroidLogTest", "This is a warning message."));
  do_check_eq(47, AndroidLog.e("AndroidLogTest", "This is an error message."));

  // Ensure the functions work when bound with null value for thisArg parameter.
  do_check_eq(48, AndroidLog.v.bind(null, "AndroidLogTest")("This is a verbose message."));
  do_check_eq(46, AndroidLog.d.bind(null, "AndroidLogTest")("This is a debug message."));
  do_check_eq(46, AndroidLog.i.bind(null, "AndroidLogTest")("This is an info message."));
  do_check_eq(48, AndroidLog.w.bind(null, "AndroidLogTest")("This is a warning message."));
  do_check_eq(47, AndroidLog.e.bind(null, "AndroidLogTest")("This is an error message."));

  // Ensure the functions work when the tag length is greater than the maximum
  // tag length.
  let tag = "X".repeat(AndroidLog.MAX_TAG_LENGTH + 1);
  do_check_eq(AndroidLog.MAX_TAG_LENGTH + 54, AndroidLog.v(tag, "This is a verbose message with a too-long tag."));
  do_check_eq(AndroidLog.MAX_TAG_LENGTH + 52, AndroidLog.d(tag, "This is a debug message with a too-long tag."));
  do_check_eq(AndroidLog.MAX_TAG_LENGTH + 52, AndroidLog.i(tag, "This is an info message with a too-long tag."));
  do_check_eq(AndroidLog.MAX_TAG_LENGTH + 54, AndroidLog.w(tag, "This is a warning message with a too-long tag."));
  do_check_eq(AndroidLog.MAX_TAG_LENGTH + 53, AndroidLog.e(tag, "This is an error message with a too-long tag."));

  // We should also ensure that the module is accessible from a ChromeWorker,
  // but there doesn't seem to be a way to load a ChromeWorker from this test.
});

run_next_test();
