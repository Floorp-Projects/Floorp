/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that an old-style default profile not previously used by any build
 * doesn't get updated to a dedicated profile for this build and we don't set
 * the flag to show the user info about dedicated profiles.
 */

add_task(async () => {
  let hash = xreDirProvider.getInstallHash();
  let defaultProfile = makeRandomProfileDir("default");

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
  checkStartupReason("firstrun-created-default");

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
    "Should be a default for installs."
  );
  Assert.equal(
    profileData.installs[hash].default,
    profile.path,
    "Should have the right default profile."
  );
  Assert.ok(
    profileData.installs[hash].locked,
    "Should have locked as we created this profile for this install."
  );

  checkProfileService(profileData);

  Assert.ok(didCreate, "Should have created a new profile.");
  Assert.ok(
    !selectedProfile.rootDir.equals(defaultProfile),
    "Should not be using the old directory."
  );
  Assert.equal(selectedProfile.name, expectedName);
});
