/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that the profile service refuses to flush when the profiles.ini file
 * has been modified.
 */

function check_unchanged(service) {
  Assert.ok(
    !service.isListOutdated,
    "Should not have detected a modification."
  );
  try {
    service.flush();
    Assert.ok(true, "Should have flushed.");
  } catch (e) {
    Assert.ok(false, "Should have succeeded flushing.");
  }
}

function check_outdated(service) {
  Assert.ok(service.isListOutdated, "Should have detected a modification.");
  try {
    service.flush();
    Assert.ok(false, "Should have failed to flush.");
  } catch (e) {
    Assert.equal(
      e.result,
      Cr.NS_ERROR_DATABASE_CHANGED,
      "Should have refused to flush."
    );
  }
}

add_task(async () => {
  let service = getProfileService();

  Assert.ok(!service.isListOutdated, "Should not be modified yet.");

  let profilesini = gDataHome.clone();
  profilesini.append("profiles.ini");

  Assert.ok(!profilesini.exists(), "File should not exist yet.");
  profilesini.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o644);
  check_outdated(service);

  profilesini.remove(false);
  check_unchanged(service);

  let oldTime = profilesini.lastModifiedTime;
  profilesini.lastModifiedTime = oldTime - 10000;
  check_outdated(service);

  // We can't reset the modification time back to exactly what it was, so I
  // guess we can't do much more here :(
});
