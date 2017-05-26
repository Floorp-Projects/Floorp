/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This is the most basic testcase.  It makes some trivial assertions,
 * then sets a timeout, and exits the test harness when that timeout
 * fires. This is meant to demonstrate that there is a complete event
 * system available to test scripts.
 * Available functions are described at:
 * http://developer.mozilla.org/en/docs/Writing_xpcshell-based_unit_tests
 */
function run_test() {
  do_check_eq(57, 57)
  do_check_neq(1, 2)
  do_check_true(true);

  do_test_pending();
  do_timeout(100, do_test_finished);
}
