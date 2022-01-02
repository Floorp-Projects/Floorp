/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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
    Assert.ok(false);
  } catch (ex) {
    Assert.ok(true);
  }
});
