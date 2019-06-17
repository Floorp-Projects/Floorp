/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * If a user has modified a relative profile path then there may be issues where
 * the profile default setting doesn't match.
 */

add_task(async () => {
  let hash = xreDirProvider.getInstallHash();

  let profileData = {
    options: {
      startWithLastProfile: true,
    },
    profiles: [{
      name: "Profile1",
      path: "../data/test",
    }, {
      name: "Profile2",
      path: "Path2",
    }],
    installs: {
      [hash]: {
        default: "test",
      },
    },
  };

  writeProfilesIni(profileData);

  let { profile, didCreate } = selectStartupProfile();
  checkStartupReason("default");

  let service = getProfileService();

  Assert.ok(!didCreate, "Should not have created a new profile.");
  Assert.equal(profile.name, "Profile1", "Should have selected the expected profile");
  Assert.ok(!service.createdAlternateProfile, "Should not have created an alternate profile.");

  Assert.equal(profile.name, service.defaultProfile.name, "Should have selected the right default.");

  service.flush();
  checkProfileService();
});
