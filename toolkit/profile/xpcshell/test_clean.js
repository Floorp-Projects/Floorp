/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests from a clean state.
 * Then does some testing that creating new profiles and marking them as
 * selected works.
 */

add_task(async () => {
  let service = getProfileService();

  let target = gDataHome.clone();
  target.append("profiles.ini");
  Assert.ok(!target.exists(), "profiles.ini should not exist yet.");
  target.leafName = "installs.ini";
  Assert.ok(!target.exists(), "installs.ini should not exist yet.");

  // Create a new profile to use.
  let newProfile = service.createProfile(null, "dedicated");
  service.flush();

  let profileData = readProfilesIni();

  Assert.equal(
    profileData.profiles.length,
    1,
    "Should have the right number of profiles."
  );

  let profile = profileData.profiles[0];
  Assert.equal(profile.name, "dedicated", "Should have the right name.");
  Assert.ok(!profile.default, "Should not be marked as the old-style default.");

  // The new profile hasn't been marked as the default yet!
  Assert.equal(
    Object.keys(profileData.installs).length,
    0,
    "Should be no defaults for installs yet."
  );

  checkProfileService(profileData);

  Assert.ok(
    service.startWithLastProfile,
    "Should be set to start with the last profile."
  );
  service.startWithLastProfile = false;
  Assert.ok(
    !service.startWithLastProfile,
    "Should be set to not start with the last profile."
  );

  service.defaultProfile = newProfile;
  service.flush();

  profileData = readProfilesIni();

  Assert.equal(
    profileData.profiles.length,
    1,
    "Should have the right number of profiles."
  );

  profile = profileData.profiles[0];
  Assert.equal(profile.name, "dedicated", "Should have the right name.");
  Assert.ok(!profile.default, "Should not be marked as the old-style default.");

  let hash = xreDirProvider.getInstallHash();
  Assert.equal(
    Object.keys(profileData.installs).length,
    1,
    "Should be only one known install."
  );
  Assert.equal(
    profileData.installs[hash].default,
    profileData.profiles[0].path,
    "Should have marked the new profile as the default for this install."
  );

  checkProfileService(profileData);

  let otherProfile = service.createProfile(null, "another");
  service.defaultProfile = otherProfile;

  service.flush();

  profileData = readProfilesIni();

  Assert.equal(
    profileData.profiles.length,
    2,
    "Should have the right number of profiles."
  );

  profile = profileData.profiles[0];
  Assert.equal(profile.name, "another", "Should have the right name.");
  Assert.ok(!profile.default, "Should not be marked as the old-style default.");

  profile = profileData.profiles[1];
  Assert.equal(profile.name, "dedicated", "Should have the right name.");
  Assert.ok(!profile.default, "Should not be marked as the old-style default.");

  Assert.equal(
    Object.keys(profileData.installs).length,
    1,
    "Should be only one known install."
  );
  Assert.equal(
    profileData.installs[hash].default,
    profileData.profiles[0].path,
    "Should have marked the new profile as the default for this install."
  );

  checkProfileService(profileData);

  newProfile.remove(true);
  service.flush();

  profileData = readProfilesIni();

  Assert.equal(
    profileData.profiles.length,
    1,
    "Should have the right number of profiles."
  );

  profile = profileData.profiles[0];
  Assert.equal(profile.name, "another", "Should have the right name.");
  Assert.ok(!profile.default, "Should not be marked as the old-style default.");

  Assert.equal(
    Object.keys(profileData.installs).length,
    1,
    "Should be only one known install."
  );
  Assert.equal(
    profileData.installs[hash].default,
    profileData.profiles[0].path,
    "Should have marked the new profile as the default for this install."
  );

  checkProfileService(profileData);

  otherProfile.remove(true);
  service.flush();

  profileData = readProfilesIni();

  Assert.equal(
    profileData.profiles.length,
    0,
    "Should have the right number of profiles."
  );

  // We leave a reference to the missing profile to stop us trying to steal the
  // old-style default profile on next startup.
  Assert.equal(
    Object.keys(profileData.installs).length,
    1,
    "Should be only one known install."
  );

  checkProfileService(profileData);
});
