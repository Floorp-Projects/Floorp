/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This test exercises functionality and also ensures the exit codes,
// which are a public API, do not change over time.
const { EXIT_CODE } = ChromeUtils.import(
  "resource://gre/modules/BackgroundUpdate.jsm"
).BackgroundUpdate;

setupProfileService();

// Ensure launched background tasks don't see this xpcshell as a concurrent
// instance.
let syncManager = Cc["@mozilla.org/updates/update-sync-manager;1"].getService(
  Ci.nsIUpdateSyncManager
);
let lockFile = do_get_profile();
lockFile.append("customExePath");
lockFile.append("customExe");
syncManager.resetLock(lockFile);

add_task(async function test_default_profile_does_not_exist() {
  // Pretend there's no default profile.
  let exitCode = await do_backgroundtask("backgroundupdate", {
    extraEnv: {
      MOZ_BACKGROUNDTASKS_NO_DEFAULT_PROFILE: "1",
    },
  });
  Assert.equal(EXIT_CODE.DEFAULT_PROFILE_DOES_NOT_EXIST, exitCode);
  Assert.equal(11, exitCode);
});

add_task(async function test_default_profile_cannot_be_locked() {
  // Now, lock the default profile.
  let profileService = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
    Ci.nsIToolkitProfileService
  );

  let file = do_get_profile();
  file.append("profile_cannot_be_locked");

  let profile = profileService.createUniqueProfile(
    file,
    "test_default_profile"
  );
  let lock = profile.lock({});

  try {
    let exitCode = await do_backgroundtask("backgroundupdate", {
      extraEnv: {
        MOZ_BACKGROUNDTASKS_DEFAULT_PROFILE_PATH: lock.directory.path,
      },
    });
    Assert.equal(EXIT_CODE.DEFAULT_PROFILE_CANNOT_BE_LOCKED, exitCode);
    Assert.equal(12, exitCode);
  } finally {
    lock.unlock();
  }
});

add_task(async function test_default_profile_cannot_be_read() {
  // Finally, provide an empty default profile, one without prefs.
  let file = do_get_profile();
  file.append("profile_cannot_be_read");

  await IOUtils.makeDirectory(file.path);

  let exitCode = await do_backgroundtask("backgroundupdate", {
    extraEnv: {
      MOZ_BACKGROUNDTASKS_DEFAULT_PROFILE_PATH: file.path,
    },
  });
  Assert.equal(EXIT_CODE.DEFAULT_PROFILE_CANNOT_BE_READ, exitCode);
  Assert.equal(13, exitCode);
});
