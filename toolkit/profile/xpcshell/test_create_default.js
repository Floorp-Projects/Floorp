/*
 * Tests that from an empty database a default profile is created.
 */

add_task(async () => {
  let service = getProfileService();

  let { profile, didCreate } = selectStartupProfile();
  checkProfileService();

  Assert.ok(didCreate, "Should have created a new profile.");
  if (AppConstants.MOZ_DEV_EDITION) {
    Assert.equal(service.profileCount, 2, "Should be two profiles.");
  } else {
    Assert.equal(service.profileCount, 1, "Should be only one profile.");
    Assert.equal(profile, service.selectedProfile, "Should now be the selected profile.");
  }
  Assert.equal(profile.name, PROFILE_DEFAULT, "Should have created a new profile with the right name.");
});
