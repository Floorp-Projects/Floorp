/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that the environment variables are used to select a profile and that
 * on the first run of a dedicated profile build we don't snatch it if it is
 * locked by another install.
 */

add_task(async () => {
  let root = makeRandomProfileDir("foo");
  let local = gDataHomeLocal.clone();
  local.append("foo");

  writeCompatibilityIni(root);

  let profileData = {
    options: {
      startWithLastProfile: true,
    },
    profiles: [
      {
        name: PROFILE_DEFAULT,
        path: root.leafName,
        default: true,
      },
      {
        name: "Profile2",
        path: "Path2",
      },
      {
        name: "Profile3",
        path: "Path3",
      },
    ],
    // Another install is using the profile and it is locked.
    installs: {
      otherinstall: {
        default: root.leafName,
        locked: true,
      },
    },
  };

  writeProfilesIni(profileData);
  checkProfileService(profileData);

  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  env.set("XRE_PROFILE_PATH", root.path);
  env.set("XRE_PROFILE_LOCAL_PATH", local.path);

  let { rootDir, localDir, profile, didCreate } = selectStartupProfile();
  checkStartupReason("restart-skipped-default");

  // Since there is already a profile with the desired name on dev-edition, a
  // unique version will be used.
  let expectedName = AppConstants.MOZ_DEV_EDITION
    ? `${DEDICATED_NAME}-1`
    : DEDICATED_NAME;

  Assert.ok(didCreate, "Should have created a new profile.");
  Assert.ok(!rootDir.equals(root), "Should have selected the right root dir.");
  Assert.ok(
    !localDir.equals(local),
    "Should have selected the right local dir."
  );
  Assert.ok(profile, "A named profile was returned.");
  Assert.equal(profile.name, expectedName, "The right profile name was used.");

  let service = getProfileService();
  Assert.equal(
    service.defaultProfile,
    profile,
    "Should be the default profile."
  );
  Assert.equal(
    service.currentProfile,
    profile,
    "Should be the current profile."
  );

  profileData = readProfilesIni();

  Assert.equal(
    profileData.profiles[0].name,
    PROFILE_DEFAULT,
    "Should be the right profile."
  );
  Assert.ok(
    profileData.profiles[0].default,
    "Should be the old default profile."
  );
  Assert.equal(
    profileData.profiles[0].path,
    root.leafName,
    "Should be the correct path."
  );
  Assert.equal(
    profileData.profiles[1].name,
    expectedName,
    "Should be the right profile."
  );
  Assert.ok(
    !profileData.profiles[1].default,
    "Should not be the old default profile."
  );

  let hash = xreDirProvider.getInstallHash();
  Assert.equal(
    Object.keys(profileData.installs).length,
    2,
    "Should be one known install."
  );
  Assert.notEqual(
    profileData.installs[hash].default,
    root.leafName,
    "Should have marked the original default profile as the default for this install."
  );
  Assert.ok(
    profileData.installs[hash].locked,
    "Should have locked as we created the profile for this install."
  );
  Assert.equal(
    profileData.installs.otherinstall.default,
    root.leafName,
    "Should have left the other profile as the default for the other install."
  );
  Assert.ok(
    profileData.installs[hash].locked,
    "Should still be locked to the other install."
  );
});
