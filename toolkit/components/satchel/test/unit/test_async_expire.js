/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.defineESModuleGetters(this, {
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
});

function promiseExpiration() {
  let promise = TestUtils.topicObserved(
    "satchel-storage-changed",
    (subject, data) => {
      return data == "formhistory-expireoldentries";
    }
  );

  // We can't easily fake a "daily idle" event, so for testing purposes form
  // history listens for another notification to trigger an immediate
  // expiration.
  Services.obs.notifyObservers(null, "formhistory-expire-now");

  return promise;
}

add_task(async function () {
  // ===== test init =====
  let testfile = do_get_file("asyncformhistory_expire.sqlite");
  let profileDir = do_get_profile();

  // Cleanup from any previous tests or failures.
  let dbFile = profileDir.clone();
  dbFile.append("formhistory.sqlite");
  if (dbFile.exists()) {
    dbFile.remove(false);
  }

  testfile.copyTo(profileDir, "formhistory.sqlite");
  Assert.ok(dbFile.exists());

  // We're going to clear this at the end, so it better have the default value now.
  Assert.ok(!Services.prefs.prefHasUserValue("browser.formfill.expire_days"));

  // Sanity check initial state
  Assert.equal(508, await promiseCountEntries(null, null));
  Assert.ok((await promiseCountEntries("name-A", "value-A")) > 0); // lastUsed == distant past
  Assert.ok((await promiseCountEntries("name-B", "value-B")) > 0); // lastUsed == distant future

  Assert.equal(CURRENT_SCHEMA, await getDBVersion(dbFile));

  // Add a new entry
  Assert.equal(0, await promiseCountEntries("name-C", "value-C"));
  await promiseAddEntry("name-C", "value-C");
  Assert.equal(1, await promiseCountEntries("name-C", "value-C"));

  // Update some existing entries to have ages relative to when the test runs.
  let now = 1000 * Date.now();
  let updateLastUsed = (results, age) => {
    let lastUsed = now - age * 24 * PR_HOURS;

    let changes = [];
    for (let result of results) {
      changes.push({ op: "update", lastUsed, guid: result.guid });
    }

    return changes;
  };

  let results = await FormHistory.search(["guid"], { lastUsed: 181 });
  await promiseUpdate(updateLastUsed(results, 181));

  results = await FormHistory.search(["guid"], { lastUsed: 179 });
  await promiseUpdate(updateLastUsed(results, 179));

  results = await FormHistory.search(["guid"], { lastUsed: 31 });
  await promiseUpdate(updateLastUsed(results, 31));

  results = await FormHistory.search(["guid"], { lastUsed: 29 });
  await promiseUpdate(updateLastUsed(results, 29));

  results = await FormHistory.search(["guid"], { lastUsed: 9999 });
  await promiseUpdate(updateLastUsed(results, 11));

  results = await FormHistory.search(["guid"], { lastUsed: 9 });
  await promiseUpdate(updateLastUsed(results, 9));

  Assert.ok((await promiseCountEntries("name-A", "value-A")) > 0);
  Assert.ok((await promiseCountEntries("181DaysOld", "foo")) > 0);
  Assert.ok((await promiseCountEntries("179DaysOld", "foo")) > 0);
  Assert.equal(509, await promiseCountEntries(null, null));

  // 2 entries are expected to expire.
  await promiseExpiration();

  Assert.equal(0, await promiseCountEntries("name-A", "value-A"));
  Assert.equal(0, await promiseCountEntries("181DaysOld", "foo"));
  Assert.ok((await promiseCountEntries("179DaysOld", "foo")) > 0);
  Assert.equal(507, await promiseCountEntries(null, null));

  // And again. No change expected.
  await promiseExpiration();

  Assert.equal(507, await promiseCountEntries(null, null));

  // Set formfill pref to 30 days.
  Services.prefs.setIntPref("browser.formfill.expire_days", 30);

  Assert.ok((await promiseCountEntries("179DaysOld", "foo")) > 0);
  Assert.ok((await promiseCountEntries("bar", "31days")) > 0);
  Assert.ok((await promiseCountEntries("bar", "29days")) > 0);
  Assert.equal(507, await promiseCountEntries(null, null));

  await promiseExpiration();

  Assert.equal(0, await promiseCountEntries("179DaysOld", "foo"));
  Assert.equal(0, await promiseCountEntries("bar", "31days"));
  Assert.ok((await promiseCountEntries("bar", "29days")) > 0);
  Assert.equal(505, await promiseCountEntries(null, null));

  // Set override pref to 10 days and expire. This expires a large batch of
  // entries, and should trigger a VACCUM to reduce file size.
  Services.prefs.setIntPref("browser.formfill.expire_days", 10);

  Assert.ok((await promiseCountEntries("bar", "29days")) > 0);
  Assert.ok((await promiseCountEntries("9DaysOld", "foo")) > 0);
  Assert.equal(505, await promiseCountEntries(null, null));

  await promiseExpiration();

  Assert.equal(0, await promiseCountEntries("bar", "29days"));
  Assert.ok((await promiseCountEntries("9DaysOld", "foo")) > 0);
  Assert.ok((await promiseCountEntries("name-B", "value-B")) > 0);
  Assert.ok((await promiseCountEntries("name-C", "value-C")) > 0);
  Assert.equal(3, await promiseCountEntries(null, null));
});
