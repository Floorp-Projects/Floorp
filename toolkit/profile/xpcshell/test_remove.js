/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests adding and removing functions correctly.
 */

function compareLists(service, knownProfiles) {
  Assert.equal(
    service.profileCount,
    knownProfiles.length,
    "profileCount should be correct."
  );
  let serviceProfiles = Array.from(service.profiles);
  Assert.equal(
    serviceProfiles.length,
    knownProfiles.length,
    "Enumerator length should be correct."
  );

  for (let i = 0; i < knownProfiles.length; i++) {
    // Cannot use strictEqual here, it attempts to print out a string
    // representation of the profile objects and on some platforms that recurses
    // infinitely.
    Assert.ok(
      serviceProfiles[i] === knownProfiles[i],
      `Should have the right profile in position ${i}.`
    );
  }
}

function removeProfile(profiles, position) {
  dump(`Removing profile in position ${position}.`);
  Assert.greaterOrEqual(position, 0, "Should be removing a valid position.");
  Assert.less(
    position,
    profiles.length,
    "Should be removing a valid position."
  );

  let last = profiles.pop();

  if (profiles.length == position) {
    // We were asked to remove the last profile.
    last.remove(false);
    return;
  }

  profiles[position].remove(false);
  profiles[position] = last;
}

add_task(async () => {
  let service = getProfileService();
  let profiles = [];
  compareLists(service, profiles);

  profiles.push(service.createProfile(null, "profile1"));
  profiles.push(service.createProfile(null, "profile2"));
  profiles.push(service.createProfile(null, "profile3"));
  profiles.push(service.createProfile(null, "profile4"));
  profiles.push(service.createProfile(null, "profile5"));
  profiles.push(service.createProfile(null, "profile6"));
  profiles.push(service.createProfile(null, "profile7"));
  profiles.push(service.createProfile(null, "profile8"));
  profiles.push(service.createProfile(null, "profile9"));
  compareLists(service, profiles);

  // Test removing the first profile.
  removeProfile(profiles, 0);
  compareLists(service, profiles);

  // And the last profile.
  removeProfile(profiles, profiles.length - 1);
  compareLists(service, profiles);

  // Last but one...
  removeProfile(profiles, profiles.length - 2);
  compareLists(service, profiles);

  // Second one...
  removeProfile(profiles, 1);
  compareLists(service, profiles);

  // Something in the middle.
  removeProfile(profiles, 2);
  compareLists(service, profiles);

  let expectedNames = ["profile9", "profile7", "profile5", "profile4"];

  let serviceProfiles = Array.from(service.profiles);
  for (let i = 0; i < expectedNames.length; i++) {
    Assert.equal(serviceProfiles[i].name, expectedNames[i]);
  }

  removeProfile(profiles, 0);
  removeProfile(profiles, 0);
  removeProfile(profiles, 0);
  removeProfile(profiles, 0);

  Assert.equal(Array.from(service.profiles).length, 0, "All profiles gone.");
  Assert.equal(service.profileCount, 0, "All profiles gone.");
});
