/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { BackgroundTasksUtils } = ChromeUtils.import(
  "resource://gre/modules/BackgroundTasksUtils.jsm"
);

setupProfileService();

add_task(async function test_withProfileLock() {
  let profileService = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
    Ci.nsIToolkitProfileService
  );

  let profilePath = do_get_profile();
  profilePath.append(`test_withProfileLock`);
  let profile = profileService.createUniqueProfile(
    profilePath,
    "test_withProfileLock"
  );

  await BackgroundTasksUtils.withProfileLock(async lock => {
    BackgroundTasksUtils._throwIfNotLocked(lock);
  }, profile);

  // In debug builds, an unlocked lock crashes via `NS_ERROR` when
  // `.directory` is invoked.  There's no way to check that the lock
  // is unlocked, so we can't realistically test this scenario in
  // debug builds.
  if (!AppConstants.DEBUG) {
    await Assert.rejects(
      BackgroundTasksUtils.withProfileLock(async lock => {
        lock.unlock();
        BackgroundTasksUtils._throwIfNotLocked(lock);
      }, profile),
      /Profile is not locked/
    );
  }
});

add_task(async function test_readPreferences() {
  let profileService = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
    Ci.nsIToolkitProfileService
  );

  let profilePath = do_get_profile();
  profilePath.append(`test_readPreferences`);
  let profile = profileService.createUniqueProfile(
    profilePath,
    "test_readPreferences"
  );

  // Before we write any preferences, we fail to read.
  await Assert.rejects(
    BackgroundTasksUtils.withProfileLock(
      lock => BackgroundTasksUtils.readPreferences(null, lock),
      profile
    ),
    /NotFoundError/
  );

  let s = `user_pref("testPref.bool1", true);
  user_pref("testPref.bool2", false);
  user_pref("testPref.int1", 23);
  user_pref("testPref.int2", -1236);
  `;

  let prefsFile = profile.rootDir.clone();
  prefsFile.append("prefs.js");
  await IOUtils.writeUTF8(prefsFile.path, s);

  // Now we can read all the prefs.
  let prefs = await BackgroundTasksUtils.withProfileLock(
    lock => BackgroundTasksUtils.readPreferences(null, lock),
    profile
  );
  let expected = {
    "testPref.bool1": true,
    "testPref.bool2": false,
    "testPref.int1": 23,
    "testPref.int2": -1236,
  };
  Assert.deepEqual(prefs, expected, "Prefs read are correct");

  // And we can filter the read as well.
  prefs = await BackgroundTasksUtils.withProfileLock(
    lock =>
      BackgroundTasksUtils.readPreferences(name => name.endsWith("1"), lock),
    profile
  );
  expected = {
    "testPref.bool1": true,
    "testPref.int1": 23,
  };
  Assert.deepEqual(prefs, expected, "Filtered prefs read are correct");
});

add_task(async function test_readTelemetryClientID() {
  let profileService = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
    Ci.nsIToolkitProfileService
  );

  let profilePath = do_get_profile();
  profilePath.append(`test_readTelemetryClientID`);
  let profile = profileService.createUniqueProfile(
    profilePath,
    "test_readTelemetryClientID"
  );

  // Before we write any state, we fail to read.
  await Assert.rejects(
    BackgroundTasksUtils.withProfileLock(
      lock => BackgroundTasksUtils.readTelemetryClientID(lock),
      profile
    ),
    /NotFoundError/
  );

  let expected = {
    clientID: "9cc75672-6830-4cb6-a7fd-089d6c7ce34c",
    ecosystemClientID: "752f9d53-73fc-4006-a93c-d3e2e427f238",
  };
  let prefsFile = profile.rootDir.clone();
  prefsFile.append("datareporting");
  prefsFile.append("state.json");
  await IOUtils.writeUTF8(prefsFile.path, JSON.stringify(expected));

  // Now we can read the state.
  let state = await BackgroundTasksUtils.withProfileLock(
    lock => BackgroundTasksUtils.readTelemetryClientID(lock),
    profile
  );
  Assert.deepEqual(state, expected.clientID, "State read is correct");
});
