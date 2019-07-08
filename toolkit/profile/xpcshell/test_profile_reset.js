/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that from an empty database with profile reset requested a new profile
 * is still created.
 */

add_task(async () => {
  let service = getProfileService();

  let { profile: selectedProfile, didCreate } = selectStartupProfile([], true);
  // With no profile we're just create a new profile and skip resetting it.
  checkStartupReason("firstrun-created-default");
  checkProfileService();

  let hash = xreDirProvider.getInstallHash();
  let profileData = readProfilesIni();

  Assert.ok(
    profileData.options.startWithLastProfile,
    "Should be set to start with the last profile."
  );
  Assert.equal(
    profileData.profiles.length,
    2,
    "Should have the right number of profiles, ours and the old-style default."
  );

  let profile = profileData.profiles[0];
  Assert.equal(profile.name, "default", "Should have the right name.");
  Assert.ok(profile.default, "Should be marked as the old-style default.");

  profile = profileData.profiles[1];
  Assert.equal(profile.name, DEDICATED_NAME, "Should have the right name.");
  Assert.ok(!profile.default, "Should not be marked as the old-style default.");

  Assert.equal(
    Object.keys(profileData.installs).length,
    1,
    "Should only be one known installs."
  );
  Assert.equal(
    profileData.installs[hash].default,
    profile.path,
    "Should have taken the new profile as the default for the current install."
  );
  Assert.ok(
    profileData.installs[hash].locked,
    "Should have locked as we created this profile."
  );

  checkProfileService(profileData);

  Assert.ok(didCreate, "Should have created a new profile.");
  Assert.ok(
    !service.createdAlternateProfile,
    "Should not have created an alternate profile."
  );
  Assert.equal(
    selectedProfile.name,
    profile.name,
    "Should be using the right profile."
  );
});
