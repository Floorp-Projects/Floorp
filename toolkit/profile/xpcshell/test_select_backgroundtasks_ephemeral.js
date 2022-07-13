/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Verify that background tasks don't touch `profiles.ini` for ephemeral profile
 * tasks.
 */

let condition = {
  skip_if: () => !AppConstants.MOZ_BACKGROUNDTASKS,
};

add_task(condition, async () => {
  writeProfilesIni(BACKGROUNDTASKS_PROFILE_DATA);

  // Pretend that this is a background task.  For a task that uses an ephemeral
  // profile, `profiles.ini` is untouched.
  const bts = Cc["@mozilla.org/backgroundtasks;1"].getService(
    Ci.nsIBackgroundTasks
  );
  bts.overrideBackgroundTaskNameForTesting("ephemeral_profile");

  let { didCreate } = selectStartupProfile();
  checkStartupReason("backgroundtask-ephemeral");

  Assert.equal(didCreate, true, "Created new ephemeral profile");

  let profileData = readProfilesIni();
  Assert.deepEqual(BACKGROUNDTASKS_PROFILE_DATA, profileData);
});
