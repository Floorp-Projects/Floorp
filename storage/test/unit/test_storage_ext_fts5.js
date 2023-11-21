/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests support for the fts5 extension.

// Some example statements in this tests are taken from the SQLite FTS5
// documentation page: https://sqlite.org/fts5.html

add_setup(async function () {
  cleanup();
});

add_task(async function test_synchronous() {
  info("Testing synchronous connection");
  let conn = getOpenedUnsharedDatabase();
  Assert.throws(
    () =>
      conn.executeSimpleSQL(
        "CREATE VIRTUAL TABLE email USING fts5(sender, title, body);"
      ),
    /NS_ERROR_FAILURE/,
    "Should not be able to use FTS5 without loading the extension"
  );

  await loadFTS5Extension(conn);

  conn.executeSimpleSQL(
    "CREATE VIRTUAL TABLE email USING fts5(sender, title, body, tokenize='trigram');"
  );

  conn.executeSimpleSQL(
    `INSERT INTO email(sender, title, body) VALUES
    ('Mark', 'Fox', 'The quick brown fox jumps over the lazy dog.'),
    ('Marco', 'Cat', 'The quick brown cat jumps over the lazy dog.'),
    ('James', 'Hamster', 'The quick brown hamster jumps over the lazy dog.')`
  );

  var stmt = conn.createStatement(
    `SELECT sender, title, highlight(email, 2, '<', '>')
     FROM email
     WHERE email MATCH 'ham'
     ORDER BY bm25(email)`
  );
  Assert.ok(stmt.executeStep());
  Assert.equal(stmt.getString(0), "James");
  Assert.equal(stmt.getString(1), "Hamster");
  Assert.equal(
    stmt.getString(2),
    "The quick brown <ham>ster jumps over the lazy dog."
  );
  stmt.reset();
  stmt.finalize();

  cleanup();
});

add_task(async function test_asynchronous() {
  info("Testing asynchronous connection");
  let conn = await openAsyncDatabase(getTestDB());

  await Assert.rejects(
    executeSimpleSQLAsync(
      conn,
      "CREATE VIRTUAL TABLE email USING fts5(sender, title, body);"
    ),
    err => err.message.startsWith("no such module"),
    "Should not be able to use FTS5 without loading the extension"
  );

  await loadFTS5Extension(conn);

  await executeSimpleSQLAsync(
    conn,
    "CREATE VIRTUAL TABLE email USING fts5(sender, title, body);"
  );

  await asyncClose(conn);
  await IOUtils.remove(getTestDB().path, { ignoreAbsent: true });
});

add_task(async function test_clone() {
  info("Testing cloning synchronous connection loads extensions in clone");
  let conn1 = getOpenedUnsharedDatabase();
  await loadFTS5Extension(conn1);

  let conn2 = conn1.clone(false);
  conn2.executeSimpleSQL(
    "CREATE VIRTUAL TABLE email USING fts5(sender, title, body);"
  );

  conn2.close();
  cleanup();
});

add_task(async function test_asyncClone() {
  info("Testing asynchronously cloning connection loads extensions in clone");
  let conn1 = getOpenedUnsharedDatabase();
  await loadFTS5Extension(conn1);

  let conn2 = await asyncClone(conn1, false);
  await executeSimpleSQLAsync(
    conn2,
    "CREATE VIRTUAL TABLE email USING fts5(sender, title, body);"
  );

  await asyncClose(conn2);
  await asyncClose(conn1);
  await IOUtils.remove(getTestDB().path, { ignoreAbsent: true });
});

async function loadFTS5Extension(conn) {
  await new Promise((resolve, reject) => {
    conn.loadExtension("fts5", status => {
      if (Components.isSuccessCode(status)) {
        resolve();
      } else {
        reject(status);
      }
    });
  });
}
