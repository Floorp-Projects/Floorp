/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the journal persists on close.

async function check_journal_persists(db, journal) {
  let path = db.databaseFile.path;
  info(`testing ${path}`);
  await new Promise(resolve => {
    db.executeSimpleSQLAsync(`PRAGMA journal_mode = ${journal}`, {
      handleCompletion: resolve,
    });
  });

  await new Promise(resolve => {
    db.executeSimpleSQLAsync("CREATE TABLE test (id INTEGER PRIMARY KEY)", {
      handleCompletion: resolve,
    });
  });

  if (journal == "wal") {
    Assert.ok(await IOUtils.exists(path + "-wal"), "-wal exists before close");
    Assert.greater(
      (await IOUtils.stat(path + "-wal")).size,
      0,
      "-wal size is non-zero"
    );
  } else {
    Assert.ok(
      await IOUtils.exists(path + "-journal"),
      "-journal exists before close"
    );
    Assert.equal(
      (await IOUtils.stat(path + "-journal")).size,
      0,
      "-journal is truncated after every transaction"
    );
  }

  await new Promise(resolve => db.asyncClose(resolve));

  if (journal == "wal") {
    Assert.ok(await IOUtils.exists(path + "-wal"), "-wal persists after close");
    Assert.equal(
      (await IOUtils.stat(path + "-wal")).size,
      0,
      "-wal has been truncated"
    );
  } else {
    Assert.ok(
      await IOUtils.exists(path + "-journal"),
      "-journal persists after close"
    );
    Assert.equal(
      (await IOUtils.stat(path + "-journal")).size,
      0,
      "-journal has been truncated"
    );
  }
}

async function getDbPath(name) {
  let path = PathUtils.join(PathUtils.profileDir, name + ".sqlite");
  Assert.ok(!(await IOUtils.exists(path)), "database should not exist");
  return path;
}

add_task(async function () {
  for (let journal of ["truncate", "wal"]) {
    await check_journal_persists(
      Services.storage.openDatabase(
        new FileUtils.File(await getDbPath(`shared-${journal}`))
      ),
      journal
    );
    await check_journal_persists(
      Services.storage.openUnsharedDatabase(
        new FileUtils.File(await getDbPath(`unshared-${journal}`))
      ),
      journal
    );
    await check_journal_persists(
      await openAsyncDatabase(
        new FileUtils.File(await getDbPath(`async-${journal}`))
      ),
      journal
    );
  }
});
