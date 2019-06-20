/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that an old-style default profile not previously used by this build
 * gets selected when configured for legacy profiles.
 */

add_task(async () => {
  let defaultProfile = makeRandomProfileDir("default");

  // Just pretend this profile was last used by something in the profile dir.
  let greDir = gProfD.clone();
  greDir.append("app");
  writeCompatibilityIni(defaultProfile, greDir, greDir);

  writeProfilesIni({
    profiles: [{
      name: PROFILE_DEFAULT,
      path: defaultProfile.leafName,
      default: true,
    }],
  });

  enableLegacyProfiles();

  let { profile: selectedProfile, didCreate } = selectStartupProfile();
  checkStartupReason("default");

  let profileData = readProfilesIni();
  let installsINI = gDataHome.clone();
  installsINI.append("installs.ini");
  Assert.ok(!installsINI.exists(), "Installs database should not have been created.");

  Assert.ok(profileData.options.startWithLastProfile, "Should be set to start with the last profile.");
  Assert.equal(profileData.profiles.length, 1, "Should have the right number of profiles.");

  let profile = profileData.profiles[0];
  Assert.equal(profile.name, PROFILE_DEFAULT, "Should have the right name.");
  Assert.equal(profile.path, defaultProfile.leafName, "Should be the original default profile.");
  Assert.ok(profile.default, "Should be marked as the old-style default.");

  checkProfileService(profileData);

  Assert.ok(!didCreate, "Should not have created a new profile.");
  Assert.ok(selectedProfile.rootDir.equals(defaultProfile), "Should be using the right directory.");
  Assert.equal(selectedProfile.name, PROFILE_DEFAULT);
});
