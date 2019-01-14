/*
 * Tests that from an empty database profile reset doesn't create a new profile.
 */

add_task(async () => {
  let service = getProfileService();

  let { profile, didCreate } = selectStartupProfile([], true);
  checkProfileService();

  Assert.ok(!didCreate, "Should not have created a new profile.");
  Assert.ok(!profile, "Should not be a returned profile.");
  Assert.equal(service.profileCount, 0, "Still should be no profiles.");
});
