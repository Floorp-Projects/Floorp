/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Verify that background tasks that create non-ephemeral profiles update
 * `profiles.ini` with a salted profile location.
 */

let condition = {
  skip_if: () => !AppConstants.MOZ_BACKGROUNDTASKS,
};

// MOZ_APP_VENDOR is empty on Thunderbird.
let vendor = AppConstants.MOZ_APP_NAME == "thunderbird" ? "" : "Mozilla";

add_task(condition, async () => {
  let hash = xreDirProvider.getInstallHash();

  writeProfilesIni(BACKGROUNDTASKS_PROFILE_DATA);

  // Pretend that this is a background task.  For a task that does *not* use an
  // ephemeral profile, i.e., that uses a persistent profile, `profiles.ini` is
  // updated with a new entry in section `BackgroundTaskProfiles`.
  const bts = Cc["@mozilla.org/backgroundtasks;1"].getService(
    Ci.nsIBackgroundTasks
  );
  // "not_ephemeral_profile" is a special name recognized by the
  // background task system.
  bts.overrideBackgroundTaskNameForTesting("not_ephemeral_profile");

  let { didCreate, rootDir } = selectStartupProfile();
  checkStartupReason("backgroundtask-not-ephemeral");

  Assert.equal(didCreate, true, "Created new non-ephemeral profile");

  let profileData = readProfilesIni();

  // Profile names are lexicographically ordered, and `not_ephemeral_profile`
  // sorts before `unrelated_task`.
  Assert.equal(profileData.backgroundTasksProfiles.length, 2);
  Assert.deepEqual(
    [profileData.backgroundTasksProfiles[1]],
    BACKGROUNDTASKS_PROFILE_DATA.backgroundTasksProfiles
  );

  let saltedPath = profileData.backgroundTasksProfiles[0].path;
  Assert.ok(
    saltedPath.endsWith(
      `.${vendor}BackgroundTask-${hash}-not_ephemeral_profile`
    ),
    `${saltedPath} ends with ".${vendor}BackgroundTask-${hash}-not_ephemeral_profile"`
  );
  Assert.ok(
    !saltedPath.startsWith(
      `.${vendor}BackgroundTask-${hash}-not_ephemeral_profile`
    ),
    `${saltedPath} is really salted`
  );
  Assert.deepEqual(profileData.backgroundTasksProfiles[0], {
    name: `${vendor}BackgroundTask-${hash}-not_ephemeral_profile`,
    path: saltedPath,
  });

  Assert.ok(
    rootDir.path.endsWith(saltedPath),
    `rootDir "${rootDir.path}" ends with salted path "${saltedPath}"`
  );

  // Really, "UAppData", but this is xpcshell.
  let backgroundTasksProfilesPath = gDataHome;
  if (!AppConstants.XP_UNIX || AppConstants.platform === "macosx") {
    backgroundTasksProfilesPath.append("Background Tasks Profiles");
  }
  Assert.ok(
    rootDir.path.startsWith(backgroundTasksProfilesPath.path),
    `rootDir "${rootDir.path}" is sibling to user profiles directory ${backgroundTasksProfilesPath}`
  );
});
