/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Verify that background tasks that use non-ephemeral profiles re-use existing
 * salted profile locations from `profiles.ini`.
 */

let condition = {
  skip_if: () => !AppConstants.MOZ_BACKGROUNDTASKS,
};

// MOZ_APP_VENDOR is empty on Thunderbird.
let vendor = AppConstants.MOZ_APP_NAME == "thunderbird" ? "" : "Mozilla";

add_task(condition, async () => {
  let hash = xreDirProvider.getInstallHash();

  let saltedPath = `saltSALT.${vendor}BackgroundTask-${hash}-not_ephemeral_profile`;
  let profileName = `${vendor}BackgroundTask-${hash}-not_ephemeral_profile`;

  // See note about ordering below.
  BACKGROUNDTASKS_PROFILE_DATA.backgroundTasksProfiles.splice(0, 0, {
    name: profileName,
    path: saltedPath,
  });

  writeProfilesIni(BACKGROUNDTASKS_PROFILE_DATA);

  // Pretend that this is a background task.  For a task that does *not* use an
  // ephemeral profile, i.e., that uses a persistent profile, when
  // `profiles.ini` section `BackgroundTasksProfiles` contains the relevant
  // entry, that profile path is re-used.
  const bts = Cc["@mozilla.org/backgroundtasks;1"].getService(
    Ci.nsIBackgroundTasks
  );
  // "not_ephemeral_profile" is a special name recognized by the
  // background task system.
  bts.overrideBackgroundTaskNameForTesting("not_ephemeral_profile");

  let { didCreate, rootDir } = selectStartupProfile();

  checkStartupReason("backgroundtask-not-ephemeral");

  //test directory will be created
  Assert.equal(
    didCreate,
    true,
    "Re-used existing non-ephemeral profile, but should create a new directory for it"
  );

  Assert.equal(
    rootDir.exists(),
    true,
    "rootDir has a directory in the file system"
  );

  let profileData = readProfilesIni();

  // Profile names are lexicographically ordered, and `not_ephemeral_profile`
  // sorts before `unrelated_task`.
  Assert.equal(profileData.backgroundTasksProfiles.length, 2);

  //make sure the new salted path is recorded in profiles.ini
  let createdProfile = profileData.backgroundTasksProfiles.find(
    searchProfile => searchProfile.name == profileName
  );
  Assert.ok(
    rootDir.path.endsWith(createdProfile.path),
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
