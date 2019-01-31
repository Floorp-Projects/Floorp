/*
 * Tests that from a database of profiles the default profile is selected.
 */

add_task(async () => {
  let profileData = {
    options: {
      startWithLastProfile: true,
    },
    profiles: [{
      name: "Profile1",
      path: "Path1",
    }, {
      name: "Profile3",
      path: "Path3",
    }],
  };

  if (AppConstants.MOZ_DEV_EDITION) {
    profileData.profiles.push({
      name: "default",
      path: "Path2",
      default: true,
    }, {
      name: PROFILE_DEFAULT,
      path: "Path4",
    });
  } else {
    profileData.profiles.push({
      name: PROFILE_DEFAULT,
      path: "Path2",
      default: true,
    });
  }

  writeProfilesIni(profileData);

  let service = getProfileService();
  checkProfileService(profileData);

  let { profile, didCreate } = selectStartupProfile();

  Assert.ok(!didCreate, "Should not have created a new profile.");
  Assert.equal(profile, service.selectedProfile, "Should have returned the selected profile.");
  Assert.equal(profile.name, PROFILE_DEFAULT, "Should have selected the right profile");
});
