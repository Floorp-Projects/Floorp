/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test_backgroundtask_minruntime() {
  let startTime = new Date().getTime();
  let exitCode = await do_backgroundtask("minruntime");
  Assert.equal(0, exitCode);
  let finishTime = new Date().getTime();

  // minruntime sets backgroundTaskMinRuntimeMS = 2000;
  // Have some tolerance for flaky timers.
  Assert.ok(
    finishTime - startTime > 1800,
    "Runtime was at least 2 seconds (approximately)."
  );
});
