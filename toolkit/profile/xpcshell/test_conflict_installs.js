/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that the profile service refuses to flush when the install.ini file
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

add_task(async () => {
  let service = getProfileService();

  Assert.ok(!service.isListOutdated, "Should not be modified yet.");

  let installsini = gDataHome.clone();
  installsini.append("installs.ini");

  Assert.ok(!installsini.exists(), "File should not exist yet.");
  installsini.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o644);

  installsini.remove(false);
  // We have to do profile selection to actually have any install data.
  selectStartupProfile();
  check_unchanged(service);

  // We can't reset the modification time back to exactly what it was, so I
  // guess we can't do much more here :(
});
