/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.defineESModuleGetters(this, {
  Sqlite: "resource://gre/modules/Sqlite.sys.mjs",
});

// Dump of version we migrate from
const schema_queries = [
  "PRAGMA foreign_keys=OFF",
  "CREATE TABLE groups (id INTEGER PRIMARY KEY, name TEXT NOT NULL)",
  `INSERT INTO groups VALUES (1,'foo.com'),
                             (2,'bar.com'),
                             (3,'blob:resource://pdf.js/ed645567-3eea-4ff1-94fd-efb04812afe0'),
                             (4,'blob:https://chat.mozilla.org/35d6a992-6e18-4957-8216-070c53b9bc83')`,
  "CREATE TABLE settings (id INTEGER PRIMARY KEY, name TEXT NOT NULL)",
  `INSERT INTO settings VALUES (1,'zoom-setting'),
                               (2,'browser.download.lastDir')`,
  `CREATE TABLE prefs (id INTEGER PRIMARY KEY,
                       groupID INTEGER REFERENCES groups(id),
                       settingID INTEGER NOT NULL REFERENCES settings(id),
                       value BLOB,
                       timestamp INTEGER NOT NULL DEFAULT 0)`,
  `INSERT INTO prefs VALUES (1,1,1,0.5,0),
                            (2,1,2,'/download/dir',0),
                            (3,2,1,0.3,0),
                            (4,NULL,1,0.1,0),
                            (5,3,2,'/download/dir',0),
                            (6,4,2,'/download/dir',0),
                            (7,4,1,0.1,0)`,
  "CREATE INDEX groups_idx ON groups(name)",
  "CREATE INDEX settings_idx ON settings(name)",
  "CREATE INDEX prefs_idx ON prefs(timestamp, groupID, settingID)",
];

add_setup(async function () {
  let conn = await Sqlite.openConnection({
    path: PathUtils.join(PathUtils.profileDir, "content-prefs.sqlite"),
  });
  Assert.equal(await conn.getSchemaVersion(), 0);
  await conn.executeTransaction(async () => {
    for (let query of schema_queries) {
      await conn.execute(query);
    }
  });
  await conn.setSchemaVersion(5);
  await conn.close();
});

add_task(async function test() {
  // Test migrated db content.
  await schemaVersionIs(CURRENT_DB_VERSION);
  let dbExpectedState = [
    [null, "zoom-setting", 0.1],
    ["bar.com", "zoom-setting", 0.3],
    ["foo.com", "zoom-setting", 0.5],
    ["foo.com", "browser.download.lastDir", "/download/dir"],
  ];
  await dbOK(dbExpectedState);
});
