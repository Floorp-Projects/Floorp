// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const TAG = "AndroidLogTest";

const VERBOSE_MESSAGE = "This is a verbose message.";
const DEBUG_MESSAGE = "This is a debug message.";
const INFO_MESSAGE = "This is an info message.";
const WARNING_MESSAGE = "This is a warning message.";
const ERROR_MESSAGE = "This is an error message.";

// Number of bytes we expect to log.  This isn't equivalent to the number
// of characters, although the difference is consistent, so we can calculate it
// from the lengths of the messages and tag.  We include the length of "Gecko"
// because the module prepends it to the tag.
const VERBOSE_BYTES = "Gecko".length + TAG.length + VERBOSE_MESSAGE.length + 3;
const DEBUG_BYTES = "Gecko".length + TAG.length + DEBUG_MESSAGE.length + 3;
const INFO_BYTES = "Gecko".length + TAG.length + INFO_MESSAGE.length + 3;
const WARNING_BYTES = "Gecko".length + TAG.length + WARNING_MESSAGE.length + 3;
const ERROR_BYTES = "Gecko".length + TAG.length + ERROR_MESSAGE.length + 3;

add_task(function test_AndroidLog() {
  Components.utils.import("resource://gre/modules/AndroidLog.jsm");

  do_check_true(!!AndroidLog);

  do_check_true("v" in AndroidLog && typeof AndroidLog.v == "function");
  do_check_true("d" in AndroidLog && typeof AndroidLog.d == "function");
  do_check_true("i" in AndroidLog && typeof AndroidLog.i == "function");
  do_check_true("w" in AndroidLog && typeof AndroidLog.w == "function");
  do_check_true("e" in AndroidLog && typeof AndroidLog.e == "function");

  // Ensure that the functions don't cause the test process to crash
  // (because of some change to the native object being accessed via ctypes)
  // and return the right values (the number of bytes logged).
  // XXX Ensure that these messages actually make it to the log (bug 1046096).
  do_check_eq(VERBOSE_BYTES, AndroidLog.v(TAG, VERBOSE_MESSAGE));
  do_check_eq(DEBUG_BYTES, AndroidLog.d(TAG, DEBUG_MESSAGE));
  do_check_eq(INFO_BYTES, AndroidLog.i(TAG, INFO_MESSAGE));
  do_check_eq(WARNING_BYTES, AndroidLog.w(TAG, WARNING_MESSAGE));
  do_check_eq(ERROR_BYTES, AndroidLog.e(TAG, ERROR_MESSAGE));

  // Ensure the functions work when bound with null value for thisArg parameter.
  do_check_eq(VERBOSE_BYTES, AndroidLog.v.bind(null, TAG)(VERBOSE_MESSAGE));
  do_check_eq(DEBUG_BYTES, AndroidLog.d.bind(null, TAG)(DEBUG_MESSAGE));
  do_check_eq(INFO_BYTES, AndroidLog.i.bind(null, TAG)(INFO_MESSAGE));
  do_check_eq(WARNING_BYTES, AndroidLog.w.bind(null, TAG)(WARNING_MESSAGE));
  do_check_eq(ERROR_BYTES, AndroidLog.e.bind(null, TAG)(ERROR_MESSAGE));

  // Ensure the functions work when the module object is "bound" to a tag.
  let Log = AndroidLog.bind(TAG);
  do_check_eq(VERBOSE_BYTES, Log.v(VERBOSE_MESSAGE));
  do_check_eq(DEBUG_BYTES, Log.d(DEBUG_MESSAGE));
  do_check_eq(INFO_BYTES, Log.i(INFO_MESSAGE));
  do_check_eq(WARNING_BYTES, Log.w(WARNING_MESSAGE));
  do_check_eq(ERROR_BYTES, Log.e(ERROR_MESSAGE));

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
