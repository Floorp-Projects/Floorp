/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Dump of version we migrate from
var schema_version3 = `
PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
  CREATE TABLE groups (id INTEGER PRIMARY KEY, name TEXT NOT NULL);
  INSERT INTO "groups" VALUES(1,'foo.com');
  INSERT INTO "groups" VALUES(2,'bar.com');

  CREATE TABLE settings (id INTEGER PRIMARY KEY, name TEXT NOT NULL);
  INSERT INTO "settings" VALUES(1,'zoom-setting');
  INSERT INTO "settings" VALUES(2,'dir-setting');

  CREATE TABLE prefs (id INTEGER PRIMARY KEY, groupID INTEGER REFERENCES groups(id), settingID INTEGER NOT NULL REFERENCES settings(id), value BLOB);
  INSERT INTO "prefs" VALUES(1,1,1,0.5);
  INSERT INTO "prefs" VALUES(2,1,2,'/download/dir');
  INSERT INTO "prefs" VALUES(3,2,1,0.3);
  INSERT INTO "prefs" VALUES(4,NULL,1,0.1);

  CREATE INDEX groups_idx ON groups(name);
  CREATE INDEX settings_idx ON settings(name);
  CREATE INDEX prefs_idx ON prefs(groupID, settingID);
COMMIT;`;

function prepareVersion3Schema(callback) {
  var dbFile = Services.dirsvc.get("ProfD", Ci.nsIFile);
  dbFile.append("content-prefs.sqlite");

  ok(!dbFile.exists(), "Db should not exist yet.");

  var dbConnection = Services.storage.openDatabase(dbFile);
  equal(dbConnection.schemaVersion, 0);

  dbConnection.executeSimpleSQL(schema_version3);
  dbConnection.schemaVersion = 3;

  dbConnection.close();
}

add_task(async function resetBeforeTests() {
  prepareVersion3Schema();
});

// WARNING: Database will reset after every test. This limitation comes from
// the fact that we ContentPrefService constructor is run only once per test file
// and so migration will be run only once.
add_task(async function testMigration() {
  // Test migrated db content.
  await schemaVersionIs(4);
  let dbExpectedState = [
    [null, "zoom-setting", 0.1],
    ["bar.com", "zoom-setting", 0.3],
    ["foo.com", "zoom-setting", 0.5],
    ["foo.com", "dir-setting", "/download/dir"],
  ];
  await dbOK(dbExpectedState);

  // Migrated fields should have timestamp set to 0.
  await new Promise(resolve =>
    cps.removeAllDomainsSince(1000, null, makeCallback(resolve))
  );
  await dbOK(dbExpectedState);

  await new Promise(resolve =>
    cps.removeAllDomainsSince(0, null, makeCallback(resolve))
  );
  await dbOK([[null, "zoom-setting", 0.1]]);

  // Test that dates are present after migration (column is added).
  const timestamp = 1234;
  await setWithDate("a.com", "pref-name", "val", timestamp);
  let actualTimestamp = await getDate("a.com", "pref-name");
  equal(actualTimestamp, timestamp);
  await reset();
});
