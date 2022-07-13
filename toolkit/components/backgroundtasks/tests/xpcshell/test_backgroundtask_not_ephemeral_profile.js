/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_not_ephemeral_profile() {
  // Get the pref, and then update its value.  We ignore the initial return,
  // since the persistent profile may already exist.
  let exitCode = await do_backgroundtask("not_ephemeral_profile", {
    extraArgs: ["test.backgroundtask_specific_pref.exitCode", "80"],
  });

  // Do it again, confirming that the profile is persistent.
  exitCode = await do_backgroundtask("not_ephemeral_profile", {
    extraArgs: ["test.backgroundtask_specific_pref.exitCode", "81"],
  });
  Assert.equal(80, exitCode);

  exitCode = await do_backgroundtask("not_ephemeral_profile", {
    extraArgs: ["test.backgroundtask_specific_pref.exitCode", "82"],
  });
  Assert.equal(81, exitCode);
});
