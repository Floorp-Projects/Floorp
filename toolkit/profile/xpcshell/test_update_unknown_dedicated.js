/*
 * Tests that an old-style default profile not previously used by any build gets
 * updated to a dedicated profile for this build.
 */

add_task(async () => {
  let hash = xreDirProvider.getInstallHash();
  let defaultProfile = makeRandomProfileDir("default");

  writeProfilesIni({
    profiles: [{
      name: PROFILE_DEFAULT,
      path: defaultProfile.leafName,
      default: true,
    }],
  });

  let service = getProfileService();
  let { profile: selectedProfile, didCreate } = selectStartupProfile();
  checkStartupReason("firstrun-claimed-default");

  let profileData = readProfilesIni();
  let installData = readInstallsIni();

  Assert.ok(profileData.options.startWithLastProfile, "Should be set to start with the last profile.");
  Assert.equal(profileData.profiles.length, 1, "Should have the right number of profiles.");

  let profile = profileData.profiles[0];
  Assert.equal(profile.name, PROFILE_DEFAULT, "Should have the right name.");
  Assert.equal(profile.path, defaultProfile.leafName, "Should be the original default profile.");
  Assert.ok(profile.default, "Should be marked as the old-style default.");

  Assert.equal(Object.keys(installData.installs).length, 1, "Should be a default for installs.");
  Assert.equal(installData.installs[hash].default, profile.path, "Should have the right default profile.");
  Assert.ok(!installData.installs[hash].locked, "Should not have locked as we didn't create this profile for this install.");

  checkProfileService(profileData, installData);

  Assert.ok(!didCreate, "Should not have created a new profile.");
  Assert.ok(!service.createdAlternateProfile, "Should not have created an alternate profile.");
  Assert.ok(selectedProfile.rootDir.equals(defaultProfile), "Should be using the right directory.");
  Assert.equal(selectedProfile.name, PROFILE_DEFAULT);
});
