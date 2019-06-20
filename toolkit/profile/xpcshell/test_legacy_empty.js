/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that setting MOZ_LEGACY_PROFILES disables dedicated profiles.
 */

add_task(async () => {
  enableLegacyProfiles();

  let service = getProfileService();
  let { profile, didCreate } = selectStartupProfile();
  checkStartupReason("firstrun-created-default");

  Assert.ok(didCreate, "Should have created a new profile.");
  Assert.equal(profile.name, PROFILE_DEFAULT, "Should have used the normal name.");
  if (AppConstants.MOZ_DEV_EDITION) {
    Assert.equal(service.profileCount, 2, "Should be two profiles.");
  } else {
    Assert.equal(service.profileCount, 1, "Should be only one profile.");
  }

  checkProfileService();
});
