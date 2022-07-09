/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { EXIT_CODE } = ChromeUtils.import(
  "resource://gre/modules/BackgroundTasksManager.jsm"
).BackgroundTasksManager;

add_task(async function test_targeting() {
  // The task itself fails if any targeting getter fails.
  let exitCode = await do_backgroundtask("targeting");
  Assert.equal(EXIT_CODE.SUCCESS, exitCode);
});
