/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that selecting a profile directory with the "profile" argument finds
 * the matching profile.
 */

add_task(async () => {
  let root = makeRandomProfileDir("foo");
  let local = gDataHomeLocal.clone();
  local.append("foo");

  let profileData = {
    options: {
      startWithLastProfile: true,
    },
    profiles: [
      {
        name: "Profile1",
        path: root.leafName,
      },
    ],
  };

  writeProfilesIni(profileData);
  checkProfileService(profileData);

  let { rootDir, localDir, profile, didCreate } = selectStartupProfile([
    "-profile",
    root.path,
  ]);
  checkStartupReason("argument-profile");

  Assert.ok(!didCreate, "Should not have created a new profile.");
  Assert.ok(rootDir.equals(root), "Should have selected the right root dir.");
  Assert.ok(
    localDir.equals(local),
    "Should have selected the right local dir."
  );
  Assert.ok(profile, "A named profile matches this.");
  Assert.equal(profile.name, "Profile1", "The right profile was matched.");

  let service = getProfileService();
  Assert.notEqual(
    service.defaultProfile,
    profile,
    "Should not be the default profile."
  );
});
