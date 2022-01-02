/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test_backgroundtask_specific_pref() {
  // First, verify this pref isn't set in Gecko itself.
  Assert.equal(
    -1,
    Services.prefs.getIntPref("test.backgroundtask_specific_pref.exitCode", -1)
  );

  // Second, verify that this pref is set in background tasks.
  // mochitest-chrome tests locally test both unpackaged and packaged
  // builds (with `--appname=dist`).
  let exitCode = await do_backgroundtask("backgroundtask_specific_pref", {
    extraArgs: ["test.backgroundtask_specific_pref.exitCode"],
  });
  Assert.equal(79, exitCode);
});
