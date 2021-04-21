/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that when the profiles DB is missing the install data we reload it.
 */

add_task(async () => {
  let hash = xreDirProvider.getInstallHash();

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
      },
    ],
  };

  let installs = {
    [hash]: {
      default: "Path2",
    },
  };

  writeProfilesIni(profileData);
  writeInstallsIni({ installs });

  let { profile, didCreate } = selectStartupProfile();
  checkStartupReason("default");

  // Should have added the backup data to the service, check that is true.
  profileData.installs = installs;
  checkProfileService(profileData);

  Assert.ok(!didCreate, "Should not have created a new profile.");
  Assert.equal(
    profile.name,
    "Profile2",
    "Should have selected the right profile"
  );
});
