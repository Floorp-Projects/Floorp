/**
 * Tests that calling nsIToolkitProfile.remove on the default profile correctly
 * removes the profile.
 */

add_task(async () => {
  let hash = xreDirProvider.getInstallHash();
  let defaultProfile = makeRandomProfileDir("default");

  let profilesIni = {
    profiles: [{
      name: "default",
      path: defaultProfile.leafName,
      default: true,
    }],
  };
  writeProfilesIni(profilesIni);

  let installsIni = {
    installs: {
      [hash]: {
        default: defaultProfile.leafName,
      },
    },
  };
  writeInstallsIni(installsIni);

  let service = getProfileService();
  checkProfileService(profilesIni, installsIni);

  let { profile, didCreate } = selectStartupProfile();
  Assert.ok(!didCreate, "Should have not created a new profile.");
  Assert.equal(profile.name, "default", "Should have selected the default profile.");
  Assert.equal(profile, service.defaultProfile, "Should have selected the default profile.");

  checkProfileService(profilesIni, installsIni);

  // In an actual run of Firefox we wouldn't be able to delete the profile in
  // use because it would be locked. But we don't actually lock the profile in
  // tests.
  profile.remove(false);

  Assert.ok(!service.defaultProfile, "Should no longer be a default profile.");
  Assert.equal(profile, service.currentProfile, "Should still be the profile in use.");

  // These are the modifications that should have been made.
  profilesIni.profiles.pop();
  installsIni.installs[hash].default = "";

  checkProfileService(profilesIni, installsIni);

  service.flush();

  // And that should have flushed to disk correctly.
  checkProfileService();

  // checkProfileService doesn't differentiate between a blank default profile
  // for the install and a missing install.
  let installs = readInstallsIni();
  Assert.equal(installs.installs[hash].default, "", "Should be a blank default profile.");
});
