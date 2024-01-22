/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test file tests the backupToFileAsync function on
 * mozIStorageAsyncConnection, which is implemented for mozStorageConnection.
 * (but not implemented for mozilla::dom::cache::Connection).
 */

// The name of the backup database file that will be created.
const BACKUP_FILE_NAME = "test_storage.sqlite.backup";
// The number of rows to insert into the test table in the source
// database.
const TEST_ROWS = 10;
// The page size to set on the source database. During setup, we assert that
// this does not match the default page size.
const TEST_PAGE_SIZE = 512;

/**
 * This setup function creates a table inside of the test database and inserts
 * some test rows. Critically, it keeps the connection to the database _open_
 * so that we can test the scenario where a database is copied with existing
 * open connections.
 *
 * The database is closed in a cleanup function.
 */
add_setup(async () => {
  let conn = await openAsyncDatabase(getTestDB());

  Assert.notEqual(
    conn.defaultPageSize,
    TEST_PAGE_SIZE,
    "Should not default to having the TEST_PAGE_SIZE"
  );

  await executeSimpleSQLAsync(conn, "PRAGMA page_size = " + TEST_PAGE_SIZE);

  let createStmt = conn.createAsyncStatement("CREATE TABLE test(name TEXT)");
  await executeAsync(createStmt);
  createStmt.finalize();

  registerCleanupFunction(async () => {
    await asyncClose(conn);
  });
});

/**
 * Erases the test table and inserts TEST_ROWS rows into it.
 *
 * @param {mozIStorageAsyncConnection} connection
 *   The connection to use to prepare the database.
 * @returns {Promise<undefined>}
 */
async function prepareSourceDatabase(connection) {
  await executeSimpleSQLAsync(connection, "DELETE from test");
  for (let i = 0; i < TEST_ROWS; ++i) {
    let name = `Database row #${i}`;
    let stmt = connection.createAsyncStatement(
      "INSERT INTO test (name) VALUES (:name)"
    );
    stmt.params.name = name;
    let result = await executeAsync(stmt);
    stmt.finalize();
    Assert.ok(Components.isSuccessCode(result), `Inserted test row #${i}`);
  }
}

/**
 * Gets the test DB prepared with the testing table and rows.
 *
 * @returns {Promise<mozIStorageAsyncConnection>}
 */
async function getPreparedAsyncDatabase() {
  let connection = await openAsyncDatabase(getTestDB());
  await prepareSourceDatabase(connection);
  return connection;
}

/**
 * Creates a copy of the database connected to via connection, and
 * returns an nsIFile pointing at the created copy file once the
 * copy is complete.
 *
 * @param {mozIStorageAsyncConnection} connection
 *   A connection to a database that should be copied.
 * @returns {Promise<nsIFile>}
 */
async function createCopy(connection) {
  let destFilePath = PathUtils.join(PathUtils.profileDir, BACKUP_FILE_NAME);
  let destFile = await IOUtils.getFile(destFilePath);
  Assert.ok(
    !(await IOUtils.exists(destFilePath)),
    "Backup file shouldn't exist yet."
  );

  await new Promise(resolve => {
    connection.backupToFileAsync(destFile, result => {
      Assert.ok(Components.isSuccessCode(result));
      resolve(result);
    });
  });

  return destFile;
}

/**
 * Opens up the database at file, asserts that the page_size matches
 * TEST_PAGE_SIZE, and that the number of rows in the test table matches
 * expectedEntries. Closes the connection after these assertions.
 *
 * @param {nsIFile} file
 *   The database file to be opened and queried.
 * @param {number} [expectedEntries=TEST_ROWS]
 *   The expected number of rows in the test table. Defaults to TEST_ROWS.
 * @returns {Promise<undefined>}
 */
async function assertSuccessfulCopy(file, expectedEntries = TEST_ROWS) {
  let conn = await openAsyncDatabase(file);

  await executeSimpleSQLAsync(conn, "PRAGMA page_size", resultSet => {
    let result = resultSet.getNextRow();
    Assert.equal(TEST_PAGE_SIZE, result.getResultByIndex(0).getAsUint32());
  });

  let stmt = conn.createAsyncStatement("SELECT COUNT(*) FROM test");
  let results = await new Promise(resolve => {
    executeAsync(stmt, resolve);
  });
  stmt.finalize();
  let row = results.getNextRow();
  let count = row.getResultByName("COUNT(*)");
  Assert.equal(count, expectedEntries, "Got the expected entries");

  Assert.ok(
    !file.leafName.endsWith(".tmp"),
    "Should not end in .tmp extension"
  );

  await asyncClose(conn);
}

/**
 * Test the basic behaviour of backupToFileAsync, and ensure that the copied
 * database has the same characteristics and contents as the source database.
 */
add_task(async function test_backupToFileAsync() {
  let newConnection = await getPreparedAsyncDatabase();
  let copyFile = await createCopy(newConnection);
  Assert.ok(
    await IOUtils.exists(copyFile.path),
    "A new file was created by backupToFileAsync"
  );

  await assertSuccessfulCopy(copyFile);
  await IOUtils.remove(copyFile.path);
  await asyncClose(newConnection);
});

/**
 * Tests that if insertions are underway during a copy, that those insertions
 * show up in the copied database.
 */
add_task(async function test_backupToFileAsync_during_insert() {
  let newConnection = await getPreparedAsyncDatabase();
  const NEW_ENTRIES = 5;

  let copyFilePromise = createCopy(newConnection);
  let inserts = [];
  for (let i = 0; i < NEW_ENTRIES; ++i) {
    let name = `New database row #${i}`;
    let stmt = newConnection.createAsyncStatement(
      "INSERT INTO test (name) VALUES (:name)"
    );
    stmt.params.name = name;
    inserts.push(executeAsync(stmt));
    stmt.finalize();
  }
  await Promise.all(inserts);
  let copyFile = await copyFilePromise;

  Assert.ok(
    await IOUtils.exists(copyFile.path),
    "A new file was created by backupToFileAsync"
  );

  await assertSuccessfulCopy(copyFile, TEST_ROWS + NEW_ENTRIES);
  await IOUtils.remove(copyFile.path);
  await asyncClose(newConnection);
});

/**
 * Tests the behaviour of backupToFileAsync as exposed through Sqlite.sys.mjs.
 */
add_task(async function test_backupToFileAsync_via_Sqlite_module() {
  let xpcomConnection = await getPreparedAsyncDatabase();
  let moduleConnection = await Sqlite.openConnection({
    path: xpcomConnection.databaseFile.path,
  });

  let copyFilePath = PathUtils.join(PathUtils.profileDir, BACKUP_FILE_NAME);
  await moduleConnection.backup(copyFilePath);
  let copyFile = await IOUtils.getFile(copyFilePath);
  Assert.ok(await IOUtils.exists(copyFilePath), "A new file was created");

  await assertSuccessfulCopy(copyFile);
  await IOUtils.remove(copyFile.path);
  await moduleConnection.close();
  await asyncClose(xpcomConnection);
});
