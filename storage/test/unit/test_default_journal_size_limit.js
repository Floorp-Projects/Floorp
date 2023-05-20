/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

// Tests defaul journal_size_limit

async function check_journal_size(db) {
  let stmt = db.createAsyncStatement("PRAGMA journal_size_limit");
  let value = await new Promise((resolve, reject) => {
    stmt.executeAsync({
      handleResult(resultSet) {
        resolve(resultSet.getNextRow().getResultByIndex(0));
      },
      handleError(error) {
        reject();
      },
      handleCompletion() {},
    });
  });
  Assert.greater(value, 0, "There is a positive journal_size_limit");
  stmt.finalize();
  await new Promise(resolve => db.asyncClose(resolve));
}

async function getDbPath(name) {
  let path = PathUtils.join(PathUtils.profileDir, name + ".sqlite");
  Assert.ok(!(await IOUtils.exists(path)));
  return path;
}

add_task(async function () {
  await check_journal_size(
    Services.storage.openDatabase(
      new FileUtils.File(await getDbPath("journal"))
    )
  );
  await check_journal_size(
    Services.storage.openUnsharedDatabase(
      new FileUtils.File(await getDbPath("journalUnshared"))
    )
  );
  await check_journal_size(
    await openAsyncDatabase(new FileUtils.File(await getDbPath("journalAsync")))
  );
});
