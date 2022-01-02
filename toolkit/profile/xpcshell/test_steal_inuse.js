/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that an old-style default profile previously used by this build but
 * that has already been claimed by a different build gets stolen by this build.
 */

add_task(async () => {
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
    installs: {
      otherhash: {
        default: defaultProfile.leafName,
      },
    },
  });

  let { profile: selectedProfile, didCreate } = selectStartupProfile();
  checkStartupReason("firstrun-claimed-default");

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
    "Should only be one known installs."
  );
  Assert.equal(
    profileData.installs[hash].default,
    defaultProfile.leafName,
    "Should have taken the original default profile as the default for the current install."
  );
  Assert.ok(
    !profileData.installs[hash].locked,
    "Should not have locked as we're not the default app."
  );

  checkProfileService(profileData);

  Assert.ok(!didCreate, "Should not have created a new profile.");
  Assert.ok(
    selectedProfile.rootDir.equals(defaultProfile),
    "Should be using the right directory."
  );
  Assert.equal(selectedProfile.name, PROFILE_DEFAULT);
});
