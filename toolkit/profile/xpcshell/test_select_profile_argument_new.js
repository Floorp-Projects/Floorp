/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that selecting a profile directory with the "profile" argument finds
 * doesn't match the incorrect profile.
 */

add_task(async () => {
  let root = makeRandomProfileDir("foo");
  let local = gDataHomeLocal.clone();
  local.append("foo");
  let empty = makeRandomProfileDir("empty");

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
    empty.path,
  ]);
  checkStartupReason("argument-profile");

  Assert.ok(!didCreate, "Should not have created a new profile.");
  Assert.ok(rootDir.equals(empty), "Should have selected the right root dir.");
  Assert.ok(
    localDir.equals(empty),
    "Should have selected the right local dir."
  );
  Assert.ok(!profile, "No named profile matches this.");
});
