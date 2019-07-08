/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * When profiles.ini is missing there isn't any point in restoring from any
 * installs.ini, the profiles it refers to are gone anyway.
 */

add_task(async () => {
  let hash = xreDirProvider.getInstallHash();

  let installs = {
    [hash]: {
      default: "Path2",
    },
    otherhash: {
      default: "foo",
    },
    anotherhash: {
      default: "bar",
    },
  };

  writeInstallsIni({ installs });

  let { profile, didCreate } = selectStartupProfile();
  checkStartupReason("firstrun-created-default");

  let service = getProfileService();
  Assert.ok(didCreate, "Should have created a new profile.");
  Assert.equal(
    profile.name,
    DEDICATED_NAME,
    "Should have created the right profile"
  );
  Assert.ok(
    !service.createdAlternateProfile,
    "Should not have created an alternate profile."
  );

  let profilesData = readProfilesIni();
  Assert.equal(
    Object.keys(profilesData.installs).length,
    1,
    "Should be only one known install"
  );
  Assert.ok(hash in profilesData.installs, "Should be the expected install.");
  Assert.notEqual(
    profilesData.installs[hash].default,
    "Path2",
    "Didn't import the previous data."
  );

  checkProfileService(profilesData);
});
