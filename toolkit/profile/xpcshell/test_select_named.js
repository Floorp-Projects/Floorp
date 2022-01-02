/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that from a database of profiles the correct profile is selected.
 */

add_task(async () => {
  let profileData = {
    options: {
      startWithLastProfile: true,
    },
    profiles: [
      {
        name: "Profile1",
        path: "Path1",
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

  let { profile, didCreate } = selectStartupProfile(["-P", "Profile1"]);
  checkStartupReason("argument-p");

  Assert.ok(!didCreate, "Should not have created a new profile.");
  Assert.equal(
    profile.name,
    "Profile1",
    "Should have chosen the right profile"
  );
});
