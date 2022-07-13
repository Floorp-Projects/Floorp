/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

setupProfileService();

async function doOne(extraArgs = [], extraEnv = {}) {
  let sentinel = Services.uuid.generateUUID().toString();
  sentinel = sentinel.substring(1, sentinel.length - 1);

  let stdoutLines = [];
  let exitCode = await do_backgroundtask("unique_profile", {
    extraArgs: [sentinel, ...extraArgs],
    extraEnv,
    onStdoutLine: line => stdoutLines.push(line),
  });
  Assert.equal(0, exitCode);

  let infos = [];
  let profiles = [];
  for (let line of stdoutLines) {
    if (line.includes(sentinel)) {
      let info = JSON.parse(line.split(sentinel)[1]);
      infos.push(info);
      profiles.push(info[1]);
    }
  }

  Assert.equal(
    1,
    profiles.length,
    `Found 1 profile: ${JSON.stringify(profiles)}`
  );

  return profiles[0];
}

// Run a background task which displays its profile directory, and verify that
// the "normal profile configuration" mechanisms override the background
// task-specific default.
add_task(async function test_backgroundtask_profile_service_configuration() {
  let profileService = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
    Ci.nsIToolkitProfileService
  );

  let profile = profileService.createUniqueProfile(
    do_get_profile(),
    "test_locked_profile"
  );
  let path = profile.rootDir.path;

  Assert.ok(
    profile.rootDir.exists(),
    `Temporary profile directory does exists: ${profile.rootDir}`
  );

  let profileDir = await doOne(["--profile", path]);
  Assert.equal(profileDir, path);

  profileDir = await doOne([], { XRE_PROFILE_PATH: path });
  Assert.equal(profileDir, path);
});
