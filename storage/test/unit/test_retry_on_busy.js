/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable mozilla/no-arbitrary-setTimeout */

const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

function getProfileFile(name) {
  let file = do_get_profile();
  file.append(name);
  return file;
}

function promiseAsyncDatabase(name, options = null) {
  return new Promise((resolve, reject) => {
    let file = getProfileFile(name);
    Services.storage.openAsyncDatabase(file, options, (status, connection) => {
      if (!Components.isSuccessCode(status)) {
        reject(new Error(`Failed to open database: ${status}`));
      } else {
        connection.QueryInterface(Ci.mozIStorageAsyncConnection);
        resolve(connection);
      }
    });
  });
}

function promiseClone(db, readOnly = false) {
  return new Promise((resolve, reject) => {
    db.asyncClone(readOnly, (status, clone) => {
      if (!Components.isSuccessCode(status)) {
        reject(new Error(`Failed to clone connection: ${status}`));
      } else {
        clone.QueryInterface(Ci.mozIStorageAsyncConnection);
        resolve(clone);
      }
    });
  });
}

function promiseClose(db) {
  return new Promise((resolve, reject) => {
    db.asyncClose(status => {
      if (!Components.isSuccessCode(status)) {
        reject(new Error(`Failed to close connection: ${status}`));
      } else {
        resolve();
      }
    });
  });
}

function promiseExecuteStatement(statement) {
  return new Promise((resolve, reject) => {
    let rows = [];
    statement.executeAsync({
      handleResult(resultSet) {
        let row = null;
        do {
          row = resultSet.getNextRow();
          if (row) {
            rows.push(row);
          }
        } while (row);
      },
      handleError(error) {
        reject(new Error(`Failed to execute statement: ${error.message}`));
      },
      handleCompletion(reason) {
        if (reason == Ci.mozIStorageStatementCallback.REASON_FINISHED) {
          resolve(rows);
        } else {
          reject(new Error("Statement failed to execute or was cancelled"));
        }
      },
    });
  });
}

add_task(async function test_retry_on_busy() {
  info("Open first writer in WAL mode and set up schema");
  let db1 = await promiseAsyncDatabase("retry-on-busy.sqlite");

  let walStmt = db1.createAsyncStatement(`PRAGMA journal_mode = WAL`);
  await promiseExecuteStatement(walStmt);
  let createAStmt = db1.createAsyncStatement(`CREATE TABLE a(
    b INTEGER PRIMARY KEY
  )`);
  await promiseExecuteStatement(createAStmt);
  let createPrevAStmt = db1.createAsyncStatement(`CREATE TEMP TABLE prevA(
    curB INTEGER PRIMARY KEY,
    prevB INTEGER NOT NULL
  )`);
  await promiseExecuteStatement(createPrevAStmt);
  let createATriggerStmt = db1.createAsyncStatement(`
    CREATE TEMP TRIGGER a_afterinsert_trigger
    AFTER UPDATE ON a FOR EACH ROW
    BEGIN
      REPLACE INTO prevA(curB, prevB) VALUES(NEW.b, OLD.b);
    END`);
  await promiseExecuteStatement(createATriggerStmt);

  info("Open second writer");
  let db2 = await promiseClone(db1);

  info("Attach second writer to new database");
  let attachStmt = db2.createAsyncStatement(`ATTACH :path AS newDB`);
  attachStmt.bindByName(
    "path",
    getProfileFile("retry-on-busy-attach.sqlite").path
  );
  await promiseExecuteStatement(attachStmt);

  info("Create triggers on second writer");
  let createCStmt = db2.createAsyncStatement(`CREATE TABLE newDB.c(
    d INTEGER PRIMARY KEY
  )`);
  await promiseExecuteStatement(createCStmt);
  let createCTriggerStmt = db2.createAsyncStatement(`
    CREATE TEMP TRIGGER c_afterdelete_trigger
    AFTER DELETE ON c FOR EACH ROW
    BEGIN
      INSERT INTO a(b) VALUES(OLD.d);
    END`);
  await promiseExecuteStatement(createCTriggerStmt);

  info("Begin transaction on second writer");
  let begin2Stmt = db2.createAsyncStatement("BEGIN IMMEDIATE");
  await promiseExecuteStatement(begin2Stmt);

  info(
    "Begin transaction on first writer; should busy-wait until second writer is done"
  );
  let begin1Stmt = db1.createAsyncStatement("BEGIN IMMEDIATE");
  let promise1Began = promiseExecuteStatement(begin1Stmt);
  let update1Stmt = db1.createAsyncStatement(`UPDATE a SET b = 3 WHERE b = 1`);
  let promise1Updated = promiseExecuteStatement(update1Stmt);

  info("Wait 5 seconds");
  await new Promise(resolve => setTimeout(resolve, 5000));

  info("Commit transaction on second writer");
  let insertIntoA2Stmt = db2.createAsyncStatement(`INSERT INTO a(b) VALUES(1)`);
  await promiseExecuteStatement(insertIntoA2Stmt);
  let insertIntoC2Stmt = db2.createAsyncStatement(`INSERT INTO c(d) VALUES(2)`);
  await promiseExecuteStatement(insertIntoC2Stmt);
  let deleteFromC2Stmt = db2.createAsyncStatement(`DELETE FROM c`);
  await promiseExecuteStatement(deleteFromC2Stmt);
  let commit2Stmt = db2.createAsyncStatement("COMMIT");
  await promiseExecuteStatement(commit2Stmt);

  info("Await and commit transaction on first writer");
  await promise1Began;
  await promise1Updated;

  let commit1Stmt = db1.createAsyncStatement("COMMIT");
  await promiseExecuteStatement(commit1Stmt);

  info("Verify our writes succeeded");
  let select1Stmt = db2.createAsyncStatement("SELECT b FROM a");
  let rows = await promiseExecuteStatement(select1Stmt);
  deepEqual(rows.map(row => row.getResultByName("b")), [2, 3]);

  info("Clean up");
  for (let stmt of [
    walStmt,
    createAStmt,
    createPrevAStmt,
    createATriggerStmt,
    attachStmt,
    createCStmt,
    createCTriggerStmt,
    begin2Stmt,
    begin1Stmt,
    insertIntoA2Stmt,
    insertIntoC2Stmt,
    deleteFromC2Stmt,

    commit2Stmt,
    update1Stmt,
    commit1Stmt,
    select1Stmt,
  ]) {
    stmt.finalize();
  }
  await promiseClose(db1);
  await promiseClose(db2);
});
