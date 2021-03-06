/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Setup that allows to use the profile service, lifted from
// `toolkit/profile/xpcshell/head.js`.
let gProfD = do_get_profile();
let gDataHome = gProfD.clone();
gDataHome.append("data");
gDataHome.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
let gDataHomeLocal = gProfD.clone();
gDataHomeLocal.append("local");
gDataHomeLocal.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o755);

let xreDirProvider = Cc["@mozilla.org/xre/directory-provider;1"].getService(
  Ci.nsIXREDirProvider
);
xreDirProvider.setUserDataDirectory(gDataHome, false);
xreDirProvider.setUserDataDirectory(gDataHomeLocal, true);

add_task(async function test_backgroundtask_locked_profile() {
  let profileService = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
    Ci.nsIToolkitProfileService
  );

  let profile = profileService.createUniqueProfile(
    do_get_profile(),
    "test_locked_profile"
  );
  let lock = profile.lock({});

  try {
    let exitCode = await do_backgroundtask("success", {
      extraEnv: { XRE_PROFILE_PATH: lock.directory.path },
    });
    Assert.equal(1, exitCode);
  } finally {
    lock.unlock();
  }
});
