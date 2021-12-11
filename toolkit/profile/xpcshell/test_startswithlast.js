/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that if profiles.ini is set to not start with the last profile then
 * we show the profile manager in preference to assigning the old default.
 */

add_task(async () => {
  let defaultProfile = makeRandomProfileDir("default");

  writeCompatibilityIni(defaultProfile);

  writeProfilesIni({
    options: {
      startWithLastProfile: false,
    },
    profiles: [
      {
        name: PROFILE_DEFAULT,
        path: defaultProfile.leafName,
        default: true,
      },
    ],
  });

  testStartsProfileManager();
});
