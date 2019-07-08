/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Previous versions of Firefox automatically used a single profile even if it
 * wasn't marked as the default. So we should try to upgrade that one if it was
 * last used by this build. This test checks the case where it wasn't.
 */

add_task(async () => {
  let defaultProfile = makeRandomProfileDir("default");

  // Just pretend this profile was last used by something in the profile dir.
  let greDir = gProfD.clone();
  greDir.append("app");
  writeCompatibilityIni(defaultProfile, greDir, greDir);

  writeProfilesIni({
    profiles: [
      {
        name: "default",
        path: defaultProfile.leafName,
        default: false,
      },
    ],
  });

  let service = getProfileService();

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
  Assert.ok(!profile.default, "Should not be marked as the old-style default.");

  Assert.ok(!profileData.installs, "Should be no defaults for installs yet.");

  checkProfileService(profileData);

  let { profile: selectedProfile, didCreate } = selectStartupProfile();
  checkStartupReason("firstrun-skipped-default");
  Assert.ok(didCreate, "Should have created a new profile.");
  Assert.ok(
    service.createdAlternateProfile,
    "Should have created an alternate profile."
  );
  Assert.ok(
    !selectedProfile.rootDir.equals(defaultProfile),
    "Should be using the right directory."
  );
  Assert.equal(selectedProfile.name, DEDICATED_NAME);

  profileData = readProfilesIni();

  profile = profileData.profiles[0];
  Assert.equal(profile.name, "default", "Should have the right name.");
  Assert.equal(
    profile.path,
    defaultProfile.leafName,
    "Should be the original default profile."
  );
  Assert.ok(profile.default, "Should now be marked as the old-style default.");

  checkProfileService(profileData);
});
