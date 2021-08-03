/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that when running out of a package we won't migrate from an existing
   profile that is out of date.
 */

add_task(async () => {
  // Create a default profile to migrate from.
  let dir = makeRandomProfileDir(DEDICATED_NAME);
  let profileData = {
    options: {
      startWithLastProfile: true,
    },
    profiles: [
      {
        name: DEDICATED_NAME,
        path: dir.leafName,
        default: true,
      },
    ],
  };
  writeProfilesIni(profileData);
  checkProfileService(profileData);

  // Create a lockfile to make it look like this profile has been used recently.
  let lockFile = dir.clone();
  lockFile.append("parent.lock");
  // Don't fail if the file already exists.
  try {
    lockFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o777);
  } catch (_e) {}
  await IOUtils.touch(lockFile.path);

  // Write a compatibility that will simulate attempting a downgrade.
  writeCompatibilityIni(dir, undefined, undefined, { downgrade: true });

  // Switch on simulated package mode and try to select a profile.
  simulateWinPackageEnvironment();

  let { rootDir, profile, didCreate } = selectStartupProfile();

  // If the package-specific migration logic selects a profile to migrate, then
  // we'll have "firstrun-migrated-default" set as the reason. But since we set
  // up a downgrade, we should skip through that logic and get into the "normal"
  // selection code for a dedicated profile. That code doesn't do any downgrade
  // check (that wouldn't happen until later), so it should decide to claim our
  // only profile as the new dedicated default.
  checkStartupReason("firstrun-claimed-default");

  Assert.ok(!didCreate, "Should not have created a new profile.");
  Assert.ok(profile, "Should have selected an existing profile.");
  Assert.ok(rootDir.equals(dir), "Should have selected our root dir.");
  // The localDir will likely be different, but it doesn't really matter.
});
