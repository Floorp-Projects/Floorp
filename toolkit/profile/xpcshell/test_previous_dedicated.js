/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * If install.ini lists a default profile for this build but that profile no
 * longer exists don't try to steal the old-style default even if it was used
 * by this build. It means this install has previously used dedicated profiles.
 */

add_task(async () => {
  let hash = xreDirProvider.getInstallHash();
  let defaultProfile = makeRandomProfileDir("default");

  writeCompatibilityIni(defaultProfile);

  writeProfilesIni({
    profiles: [
      {
        name: "default",
        path: defaultProfile.leafName,
        default: true,
      },
    ],
    installs: {
      [hash]: {
        default: "foobar",
      },
    },
  });

  testStartsProfileManager();

  let profileData = readProfilesIni();

  Assert.ok(
    profileData.options.startWithLastProfile,
    "Should be set to start with the last profile."
  );
  Assert.equal(
    profileData.profiles.length,
    1,
    "Should have the right number of profiles."
  );

  let profile = profileData.profiles[0];
  Assert.equal(profile.name, "default", "Should have the right name.");
  Assert.equal(
    profile.path,
    defaultProfile.leafName,
    "Should be the original default profile."
  );
  Assert.ok(profile.default, "Should be marked as the old-style default.");

  // We keep the data here so we don't steal on the next reboot...
  Assert.equal(
    Object.keys(profileData.installs).length,
    1,
    "Still list the broken reference."
  );

  checkProfileService(profileData);
});
