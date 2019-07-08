/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that from a database of profiles the default profile is selected.
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
        name: "Profile3",
        path: "Path3",
      },
    ],
    installs: {
      [hash]: {
        default: "Path2",
      },
    },
  };

  if (AppConstants.MOZ_DEV_EDITION) {
    profileData.profiles.push(
      {
        name: "default",
        path: "Path2",
        default: true,
      },
      {
        name: PROFILE_DEFAULT,
        path: "Path4",
      }
    );
  } else {
    profileData.profiles.push({
      name: PROFILE_DEFAULT,
      path: "Path2",
      default: true,
    });
  }

  writeProfilesIni(profileData);

  let { profile, didCreate } = selectStartupProfile();
  checkStartupReason("default");

  let service = getProfileService();
  checkProfileService(profileData);

  Assert.ok(!didCreate, "Should not have created a new profile.");
  Assert.equal(
    profile,
    service.defaultProfile,
    "Should have returned the default profile."
  );
  Assert.equal(
    profile.name,
    "default",
    "Should have selected the right profile"
  );
  Assert.ok(
    !service.createdAlternateProfile,
    "Should not have created an alternate profile."
  );
});
