/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that the environment variables are used to select a profile.
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
      {
        name: "Profile2",
        path: "Path2",
        default: true,
      },
      {
        name: "Profile3",
        path: "Path3",
      },
    ],
  };

  writeProfilesIni(profileData);
  checkProfileService(profileData);

  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  env.set("XRE_PROFILE_PATH", root.path);
  env.set("XRE_PROFILE_LOCAL_PATH", local.path);

  let { rootDir, localDir, profile, didCreate } = selectStartupProfile([
    "-P",
    "Profile3",
  ]);
  checkStartupReason("restart");

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
