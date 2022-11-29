/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests the readonly-immutable-nolock VFS.

add_task(async function test() {
  const path = PathUtils.join(PathUtils.profileDir, "ro");
  await IOUtils.makeDirectory(path);
  const dbpath = PathUtils.join(path, "test-immutable.sqlite");

  let conn = await Sqlite.openConnection({ path: dbpath });
  await conn.execute("PRAGMA journal_mode = WAL");
  await conn.execute("CREATE TABLE test (id INTEGER PRIMARY KEY)");
  Assert.ok(await IOUtils.exists(dbpath + "-wal"), "wal journal exists");
  await conn.close();

  // The wal should have been merged at this point, but just in case...
  info("Remove auxiliary files and set the folder as readonly");
  await IOUtils.remove(dbpath + "-wal", { ignoreAbsent: true });
  await IOUtils.setPermissions(path, 0o555);
  registerCleanupFunction(async () => {
    await IOUtils.setPermissions(path, 0o777);
    await IOUtils.remove(path, { recursive: true });
  });

  // Windows doesn't disallow creating files in read only folders.
  if (AppConstants.platform == "macosx" || AppConstants.platform == "linux") {
    await Assert.rejects(
      Sqlite.openConnection({ path: dbpath, readOnly: true }),
      /NS_ERROR_FILE/,
      "Should not be able to open the db because it can't create a wal journal"
    );
  }

  // Open the database with ignoreLockingMode.
  let conn2 = await Sqlite.openConnection({
    path: dbpath,
    ignoreLockingMode: true,
  });
  await conn2.execute("SELECT * FROM sqlite_master");
  Assert.ok(
    !(await IOUtils.exists(dbpath + "-wal")),
    "wal journal was not created"
  );
  await conn2.close();
});
