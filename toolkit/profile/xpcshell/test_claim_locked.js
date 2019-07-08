/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that an old-style default profile already locked to a different install
 * isn't claimed by this install.
 */

add_task(async () => {
  let defaultProfile = makeRandomProfileDir("default");

  writeCompatibilityIni(defaultProfile);

  writeProfilesIni({
    profiles: [
      {
        name: "Foo",
        path: defaultProfile.leafName,
        default: true,
      },
    ],
    installs: {
      other: {
        default: defaultProfile.leafName,
        locked: true,
      },
    },
  });

  let { profile: selectedProfile, didCreate } = selectStartupProfile();

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

  let hash = xreDirProvider.getInstallHash();
  Assert.equal(
    Object.keys(profileData.installs).length,
    2,
    "Should be two known installs."
  );
  Assert.notEqual(
    profileData.installs[hash].default,
    defaultProfile.leafName,
    "Should not have marked the original default profile as the default for this install."
  );
  Assert.ok(
    profileData.installs[hash].locked,
    "Should have locked as we created this profile for this install."
  );

  checkProfileService(profileData);

  Assert.ok(didCreate, "Should have created a new profile.");
  Assert.ok(
    !selectedProfile.rootDir.equals(defaultProfile),
    "Should be using a different directory."
  );
  Assert.equal(selectedProfile.name, DEDICATED_NAME);
});
