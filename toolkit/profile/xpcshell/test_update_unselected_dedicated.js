/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that an old-style default profile not previously used by this build gets
 * ignored.
 */

add_task(async () => {
  let hash = xreDirProvider.getInstallHash();
  let defaultProfile = makeRandomProfileDir("default");

  // Just pretend this profile was last used by something in the profile dir.
  let greDir = gProfD.clone();
  greDir.append("app");
  writeCompatibilityIni(defaultProfile, greDir, greDir);

  writeProfilesIni({
    profiles: [
      {
        name: PROFILE_DEFAULT,
        path: defaultProfile.leafName,
        default: true,
      },
    ],
  });

  let { profile: selectedProfile, didCreate } = selectStartupProfile();
  checkStartupReason("firstrun-skipped-default");

  let profileData = readProfilesIni();

  Assert.ok(
    profileData.options.startWithLastProfile,
    "Should be set to start with the last profile."
  );
  Assert.equal(
    profileData.profiles.length,
    2,
    "Should have the right number of profiles."
  );

  // Since there is already a profile with the desired name on dev-edition, a
  // unique version will be used.
  let expectedName = AppConstants.MOZ_DEV_EDITION
    ? `${DEDICATED_NAME}-1`
    : DEDICATED_NAME;

  let profile = profileData.profiles[0];
  Assert.equal(profile.name, PROFILE_DEFAULT, "Should have the right name.");
  Assert.equal(
    profile.path,
    defaultProfile.leafName,
    "Should be the original default profile."
  );
  Assert.ok(profile.default, "Should be marked as the old-style default.");
  profile = profileData.profiles[1];
  Assert.equal(profile.name, expectedName, "Should have the right name.");
  Assert.notEqual(
    profile.path,
    defaultProfile.leafName,
    "Should not be the original default profile."
  );
  Assert.ok(!profile.default, "Should not be marked as the old-style default.");

  Assert.equal(
    Object.keys(profileData.installs).length,
    1,
    "Should be a default for this install."
  );
  Assert.equal(
    profileData.installs[hash].default,
    profile.path,
    "Should have marked the new profile as the default for this install."
  );
  Assert.ok(
    profileData.installs[hash].locked,
    "Should have locked as we created this profile for this install."
  );

  checkProfileService(profileData);

  Assert.ok(didCreate, "Should have created a new profile.");
  Assert.ok(
    !selectedProfile.rootDir.equals(defaultProfile),
    "Should be using the right directory."
  );
  Assert.equal(selectedProfile.name, expectedName);
});
