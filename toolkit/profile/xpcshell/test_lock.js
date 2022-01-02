/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that when the default application claims the old-style default profile
 * it locks it to itself.
 */

add_task(async () => {
  gIsDefaultApp = true;
  let defaultProfile = makeRandomProfileDir("default");

  writeCompatibilityIni(defaultProfile);

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

  let hash = xreDirProvider.getInstallHash();
  let profileData = readProfilesIni();

  Assert.ok(
    profileData.options.startWithLastProfile,
    "Should be set to start with the last profile."
  );
  Assert.equal(
    profileData.profiles.length,
    1,
    "Should have the right number of profiles."
  );

  let profile = profileData.profiles[0];
  Assert.equal(profile.name, PROFILE_DEFAULT, "Should have the right name.");
  Assert.equal(
    profile.path,
    defaultProfile.leafName,
    "Should be the original default profile."
  );
  Assert.ok(profile.default, "Should be marked as the old-style default.");

  Assert.equal(
    Object.keys(profileData.installs).length,
    1,
    "Should be only one known install."
  );
  Assert.equal(
    profileData.installs[hash].default,
    defaultProfile.leafName,
    "Should have marked the original default profile as the default for this install."
  );
  Assert.ok(
    profileData.installs[hash].locked,
    "Should have locked as we're the default app."
  );

  checkProfileService(profileData);

  Assert.ok(!didCreate, "Should not have created a new profile.");
  Assert.ok(
    selectedProfile.rootDir.equals(defaultProfile),
    "Should be using the right directory."
  );
  Assert.equal(selectedProfile.name, PROFILE_DEFAULT);
});
