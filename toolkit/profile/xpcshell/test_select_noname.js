/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that when passing the -P command line argument and not passing a
 * profile name the profile manager is opened.
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

  testStartsProfileManager(["-P"]);
});
