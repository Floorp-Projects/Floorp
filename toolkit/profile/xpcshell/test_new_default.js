/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that an old-style default profile previously used by this build gets
 * updated to a dedicated profile for this build.
 */

add_task(async () => {
  let mydefaultProfile = makeRandomProfileDir("mydefault");
  let defaultProfile = makeRandomProfileDir("default");
  let devDefaultProfile = makeRandomProfileDir("devedition");

  writeCompatibilityIni(mydefaultProfile);
  writeCompatibilityIni(devDefaultProfile);

  writeProfilesIni({
    profiles: [
      {
        name: "mydefault",
        path: mydefaultProfile.leafName,
        default: true,
      },
      {
        name: "default",
        path: defaultProfile.leafName,
      },
      {
        name: "dev-edition-default",
        path: devDefaultProfile.leafName,
      },
    ],
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
    3,
    "Should have the right number of profiles."
  );

  let profile = profileData.profiles[0];
  Assert.equal(profile.name, "default", "Should have the right name.");
  Assert.equal(
    profile.path,
    defaultProfile.leafName,
    "Should be the original non-default profile."
  );
  Assert.ok(!profile.default, "Should not be marked as the old-style default.");

  profile = profileData.profiles[1];
  Assert.equal(
    profile.name,
    "dev-edition-default",
    "Should have the right name."
  );
  Assert.equal(
    profile.path,
    devDefaultProfile.leafName,
    "Should be the original dev default profile."
  );
  Assert.ok(!profile.default, "Should not be marked as the old-style default.");

  profile = profileData.profiles[2];
  Assert.equal(profile.name, "mydefault", "Should have the right name.");
  Assert.equal(
    profile.path,
    mydefaultProfile.leafName,
    "Should be the original default profile."
  );
  Assert.ok(profile.default, "Should be marked as the old-style default.");

  Assert.equal(
    Object.keys(profileData.installs).length,
    1,
    "Should be only one known install."
  );
  if (AppConstants.MOZ_DEV_EDITION) {
    Assert.equal(
      profileData.installs[hash].default,
      devDefaultProfile.leafName,
      "Should have marked the original dev default profile as the default for this install."
    );
  } else {
    Assert.equal(
      profileData.installs[hash].default,
      mydefaultProfile.leafName,
      "Should have marked the original default profile as the default for this install."
    );
  }

  Assert.ok(
    !profileData.installs[hash].locked,
    "Should not be locked as we're not the default app."
  );

  checkProfileService(profileData);

  Assert.ok(!didCreate, "Should not have created a new profile.");
  if (AppConstants.MOZ_DEV_EDITION) {
    Assert.ok(
      selectedProfile.rootDir.equals(devDefaultProfile),
      "Should be using the right directory."
    );
    Assert.equal(selectedProfile.name, "dev-edition-default");
  } else {
    Assert.ok(
      selectedProfile.rootDir.equals(mydefaultProfile),
      "Should be using the right directory."
    );
    Assert.equal(selectedProfile.name, "mydefault");
  }
});
