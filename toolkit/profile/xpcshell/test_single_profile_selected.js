/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Previous versions of Firefox automatically used a single profile even if it
 * wasn't marked as the default. So we should try to upgrade that one if it was
 * last used by this build. This test checks the case where it was.
 */

add_task(async () => {
  let defaultProfile = makeRandomProfileDir("default");

  writeCompatibilityIni(defaultProfile);

  writeProfilesIni({
    profiles: [
      {
        name: "default",
        path: defaultProfile.leafName,
        default: false,
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
    1,
    "Should have the right number of profiles."
  );

  let profile = profileData.profiles[0];
  Assert.equal(profile.name, "default", "Should have the right name.");
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
    !profileData.installs[hash].locked,
    "Should not have locked as we're not the default app."
  );

  checkProfileService(profileData);

  Assert.ok(!didCreate, "Should not have created a new profile.");
  let service = getProfileService();
  Assert.ok(
    !service.createdAlternateProfile,
    "Should not have created an alternate profile."
  );
  Assert.ok(
    selectedProfile.rootDir.equals(defaultProfile),
    "Should be using the right directory."
  );
  Assert.equal(selectedProfile.name, "default");
});
