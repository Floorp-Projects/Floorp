/*
 * Tests that from an empty database profile reset doesn't create a new profile.
 */

add_task(async () => {
  let service = getProfileService();

  let { profile, didCreate } = selectStartupProfile([], true);
  // Profile reset will normally end up in a restart.
  checkStartupReason("unknown");
  checkProfileService();

  Assert.ok(!didCreate, "Should not have created a new profile.");
  Assert.ok(!profile, "Should not be a returned profile.");
  Assert.equal(service.profileCount, 0, "Still should be no profiles.");
  Assert.ok(!service.createdAlternateProfile, "Should not have created an alternate profile.");
});
