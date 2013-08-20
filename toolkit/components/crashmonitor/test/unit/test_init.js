/* -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that calling |init| twice throws an error
 */
add_task(function test_init() {
  CrashMonitor.init();
  try {
    CrashMonitor.init();
    do_check_true(false);
  } catch (ex) {
    do_check_true(true);
  }
});
