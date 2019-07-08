/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that the environment variables are used to select a profile.
 */

add_task(async () => {
  let dir = makeRandomProfileDir("foo");

  let profileData = {
    options: {
      startWithLastProfile: true,
    },
    profiles: [
      {
        name: "Profile1",
        path: dir.leafName,
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
  env.set("XRE_PROFILE_PATH", dir.path);
  env.set("XRE_PROFILE_LOCAL_PATH", dir.path);

  let { rootDir, localDir, profile, didCreate } = selectStartupProfile();
  checkStartupReason("restart");

  Assert.ok(!didCreate, "Should not have created a new profile.");
  Assert.ok(rootDir.equals(dir), "Should have selected the right root dir.");
  Assert.ok(localDir.equals(dir), "Should have selected the right local dir.");
  Assert.ok(!profile, "No named profile matches this.");
});
