/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the case where the user has an default profile set for both the legacy
 * and new install hash. This should just use the default for the new install
 * hash.
 */

add_task(async () => {
  let currentHash = xreDirProvider.getInstallHash();
  let legacyHash = "F87E39E944FE466E";

  let defaultProfile = makeRandomProfileDir("default");
  let dedicatedProfile = makeRandomProfileDir("dedicated");
  let devProfile = makeRandomProfileDir("devedition");

  // Make sure we don't steal the old-style default.
  writeCompatibilityIni(defaultProfile);

  writeProfilesIni({
    profiles: [
      {
        name: "default",
        path: defaultProfile.leafName,
        default: true,
      },
      {
        name: "dedicated",
        path: dedicatedProfile.leafName,
      },
      {
        name: "dev-edition-default",
        path: devProfile.leafName,
      },
    ],
    installs: {
      [legacyHash]: {
        default: defaultProfile.leafName,
      },
      [currentHash]: {
        default: dedicatedProfile.leafName,
      },
      otherhash: {
        default: "foobar",
      },
    },
  });

  let { profile: selectedProfile, didCreate } = selectStartupProfile(
    [],
    false,
    legacyHash
  );
  checkStartupReason("default");

  let profileData = readProfilesIni();

  Assert.ok(
    profileData.options.startWithLastProfile,
    "Should be set to start with the last profile."
  );
  Assert.equal(
    profileData.profiles.length,
    3,
    "Should have the right number of profiles."
  );

  let profile = profileData.profiles[0];
  Assert.equal(profile.name, `dedicated`, "Should have the right name.");
  Assert.equal(
    profile.path,
    dedicatedProfile.leafName,
    "Should be the expected dedicated profile."
  );
  Assert.ok(!profile.default, "Should not be marked as the old-style default.");

  profile = profileData.profiles[1];
  Assert.equal(profile.name, "default", "Should have the right name.");
  Assert.equal(
    profile.path,
    defaultProfile.leafName,
    "Should be the original default profile."
  );
  Assert.ok(profile.default, "Should be marked as the old-style default.");

  profile = profileData.profiles[2];
  Assert.equal(
    profile.name,
    "dev-edition-default",
    "Should have the right name."
  );
  Assert.equal(
    profile.path,
    devProfile.leafName,
    "Should not be the original default profile."
  );
  Assert.ok(!profile.default, "Should not be marked as the old-style default.");

  Assert.equal(
    Object.keys(profileData.installs).length,
    3,
    "Should be three known installs."
  );
  Assert.equal(
    profileData.installs[currentHash].default,
    dedicatedProfile.leafName,
    "Should have switched to the new install hash."
  );
  Assert.equal(
    profileData.installs[legacyHash].default,
    defaultProfile.leafName,
    "Should have ignored old install hash."
  );
  Assert.equal(
    profileData.installs.otherhash.default,
    "foobar",
    "Should have kept the default for the other install."
  );

  checkProfileService(profileData);

  Assert.ok(!didCreate, "Should not have created a new profile.");
  Assert.ok(
    selectedProfile.rootDir.equals(dedicatedProfile),
    "Should be using the right directory."
  );
  Assert.equal(selectedProfile.name, "dedicated");
});
