/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var testnum = 0;
var dbConnection; // used for deleted table tests

Cu.import("resource://gre/modules/Promise.jsm");

function countDeletedEntries(expected) {
  return new Promise((resolve, reject) => {
    let stmt = dbConnection
               .createAsyncStatement("SELECT COUNT(*) AS numEntries FROM moz_deleted_formhistory");
    stmt.executeAsync({
      handleResult(resultSet) {
        Assert.equal(expected, resultSet.getNextRow().getResultByName("numEntries"));
        resolve();
      },
      handleError(error) {
        do_throw("Error occurred counting deleted entries: " + error);
        reject();
      },
      handleCompletion() {
        stmt.finalize();
      },
    });
  });
}

function checkTimeDeleted(guid, checkFunction) {
  return new Promise((resolve, reject) => {
    let stmt = dbConnection
               .createAsyncStatement("SELECT timeDeleted FROM moz_deleted_formhistory " +
                                     "WHERE guid = :guid");
    stmt.params.guid = guid;
    stmt.executeAsync({
      handleResult(resultSet) {
        checkFunction(resultSet.getNextRow().getResultByName("timeDeleted"));
        resolve();
      },
      handleError(error) {
        do_throw("Error occurred getting deleted entries: " + error);
        reject();
      },
      handleCompletion() {
        stmt.finalize();
      },
    });
  });
}

function promiseUpdateEntry(op, name, value) {
  let change = { op };
  if (name !== null) {
    change.fieldname = name;
  }
  if (value !== null) {
    change.value = value;
  }
  return promiseUpdate(change);
}

function promiseUpdate(change) {
  return new Promise((resolve, reject) => {
    FormHistory.update(change, {
      handleError(error) {
        this._error = error;
      },
      handleCompletion(reason) {
        if (reason) {
          reject(this._error);
        } else {
          resolve();
        }
      },
    });
  });
}

function promiseSearchEntries(terms, params) {
  return new Promise((resolve, reject) => {
    let results = [];
    FormHistory.search(terms, params,
                       { handleResult: result => results.push(result),
                         handleError(error) {
                           do_throw("Error occurred searching form history: " + error);
                           reject(error);
                         },
                         handleCompletion(reason) {
                           if (!reason) {
                             resolve(results);
                           }
                         },
                       });
  });
}

function promiseCountEntries(name, value, checkFn) {
  return new Promise(resolve => {
    countEntries(name, value, function(result) { checkFn(result); resolve(); });
  });
}

add_task(async function() {
  let oldSupportsDeletedTable = FormHistory._supportsDeletedTable;
  FormHistory._supportsDeletedTable = true;

  try {
  // ===== test init =====
    let testfile = do_get_file("formhistory_apitest.sqlite");
    let profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);

    // Cleanup from any previous tests or failures.
    let destFile = profileDir.clone();
    destFile.append("formhistory.sqlite");
    if (destFile.exists()) {
      destFile.remove(false);
    }

    testfile.copyTo(profileDir, "formhistory.sqlite");

    function checkExists(num) { Assert.ok(num > 0); }
    function checkNotExists(num) { Assert.ok(num == 0); }

    // ===== 1 =====
    // Check initial state is as expected
    testnum++;
    await promiseCountEntries("name-A", null, checkExists);
    await promiseCountEntries("name-B", null, checkExists);
    await promiseCountEntries("name-C", null, checkExists);
    await promiseCountEntries("name-D", null, checkExists);
    await promiseCountEntries("name-A", "value-A", checkExists);
    await promiseCountEntries("name-B", "value-B1", checkExists);
    await promiseCountEntries("name-B", "value-B2", checkExists);
    await promiseCountEntries("name-C", "value-C", checkExists);
    await promiseCountEntries("name-D", "value-D", checkExists);
    // time-A/B/C/D checked below.

    // Delete anything from the deleted table
    let dbFile = Services.dirsvc.get("ProfD", Ci.nsIFile).clone();
    dbFile.append("formhistory.sqlite");
    dbConnection = Services.storage.openUnsharedDatabase(dbFile);

    let deferred = Promise.defer();

    let stmt = dbConnection.createAsyncStatement("DELETE FROM moz_deleted_formhistory");
    stmt.executeAsync({
      handleResult(resultSet) { },
      handleError(error) {
        do_throw("Error occurred counting deleted all entries: " + error);
      },
      handleCompletion() {
        stmt.finalize();
        deferred.resolve();
      },
    });
    await deferred.promise;

    // ===== 2 =====
    // Test looking for nonexistent / bogus data.
    testnum++;
    await promiseCountEntries("blah", null, checkNotExists);
    await promiseCountEntries("", null, checkNotExists);
    await promiseCountEntries("name-A", "blah", checkNotExists);
    await promiseCountEntries("name-A", "", checkNotExists);
    await promiseCountEntries("name-A", null, checkExists);
    await promiseCountEntries("blah", "value-A", checkNotExists);
    await promiseCountEntries("", "value-A", checkNotExists);
    await promiseCountEntries(null, "value-A", checkExists);

    // Cannot use promiseCountEntries when name and value are null
    // because it treats null values as not set
    // and here a search should be done explicity for null.
    deferred = Promise.defer();
    await FormHistory.count({ fieldname: null, value: null },
                            { handleResult: result => checkNotExists(result),
                              handleError(error) {
                                do_throw("Error occurred searching form history: " + error);
                              },
                              handleCompletion(reason) {
                                if (!reason) {
                                  deferred.resolve();
                                }
                              },
                            });
    await deferred.promise;

    // ===== 3 =====
    // Test removeEntriesForName with a single matching value
    testnum++;
    await promiseUpdateEntry("remove", "name-A", null);

    await promiseCountEntries("name-A", "value-A", checkNotExists);
    await promiseCountEntries("name-B", "value-B1", checkExists);
    await promiseCountEntries("name-B", "value-B2", checkExists);
    await promiseCountEntries("name-C", "value-C", checkExists);
    await promiseCountEntries("name-D", "value-D", checkExists);
    await countDeletedEntries(1);

    // ===== 4 =====
    // Test removeEntriesForName with multiple matching values
    testnum++;
    await promiseUpdateEntry("remove", "name-B", null);

    await promiseCountEntries("name-A", "value-A", checkNotExists);
    await promiseCountEntries("name-B", "value-B1", checkNotExists);
    await promiseCountEntries("name-B", "value-B2", checkNotExists);
    await promiseCountEntries("name-C", "value-C", checkExists);
    await promiseCountEntries("name-D", "value-D", checkExists);
    await countDeletedEntries(3);

    // ===== 5 =====
    // Test removing by time range (single entry, not surrounding entries)
    testnum++;
    await promiseCountEntries("time-A", null, checkExists); // firstUsed=1000, lastUsed=1000
    await promiseCountEntries("time-B", null, checkExists); // firstUsed=1000, lastUsed=1099
    await promiseCountEntries("time-C", null, checkExists); // firstUsed=1099, lastUsed=1099
    await promiseCountEntries("time-D", null, checkExists); // firstUsed=2001, lastUsed=2001
    await promiseUpdate({ op: "remove", firstUsedStart: 1050, firstUsedEnd: 2000 });

    await promiseCountEntries("time-A", null, checkExists);
    await promiseCountEntries("time-B", null, checkExists);
    await promiseCountEntries("time-C", null, checkNotExists);
    await promiseCountEntries("time-D", null, checkExists);
    await countDeletedEntries(4);

    // ===== 6 =====
    // Test removing by time range (multiple entries)
    testnum++;
    await promiseUpdate({ op: "remove", firstUsedStart: 1000, firstUsedEnd: 2000 });

    await promiseCountEntries("time-A", null, checkNotExists);
    await promiseCountEntries("time-B", null, checkNotExists);
    await promiseCountEntries("time-C", null, checkNotExists);
    await promiseCountEntries("time-D", null, checkExists);
    await countDeletedEntries(6);

    // ===== 7 =====
    // test removeAllEntries
    testnum++;
    await promiseUpdateEntry("remove", null, null);

    await promiseCountEntries("name-C", null, checkNotExists);
    await promiseCountEntries("name-D", null, checkNotExists);
    await promiseCountEntries("name-C", "value-C", checkNotExists);
    await promiseCountEntries("name-D", "value-D", checkNotExists);

    await promiseCountEntries(null, null, checkNotExists);
    await countDeletedEntries(6);

    // ===== 8 =====
    // Add a single entry back
    testnum++;
    await promiseUpdateEntry("add", "newname-A", "newvalue-A");
    await promiseCountEntries("newname-A", "newvalue-A", checkExists);

    // ===== 9 =====
    // Remove the single entry
    testnum++;
    await promiseUpdateEntry("remove", "newname-A", "newvalue-A");
    await promiseCountEntries("newname-A", "newvalue-A", checkNotExists);

    // ===== 10 =====
    // Add a single entry
    testnum++;
    await promiseUpdateEntry("add", "field1", "value1");
    await promiseCountEntries("field1", "value1", checkExists);

    let processFirstResult = function processResults(results) {
      // Only handle the first result
      if (results.length > 0) {
        let result = results[0];
        return [result.timesUsed, result.firstUsed, result.lastUsed, result.guid];
      }
      return undefined;
    };

    let results = await promiseSearchEntries(["timesUsed", "firstUsed", "lastUsed"],
                                             { fieldname: "field1", value: "value1" });
    let [timesUsed, firstUsed, lastUsed] = processFirstResult(results);
    Assert.equal(1, timesUsed);
    Assert.ok(firstUsed > 0);
    Assert.ok(lastUsed > 0);
    await promiseCountEntries(null, null, num => Assert.equal(num, 1));

    // ===== 11 =====
    // Add another single entry
    testnum++;
    await promiseUpdateEntry("add", "field1", "value1b");
    await promiseCountEntries("field1", "value1", checkExists);
    await promiseCountEntries("field1", "value1b", checkExists);
    await promiseCountEntries(null, null, num => Assert.equal(num, 2));

    // ===== 12 =====
    // Update a single entry
    testnum++;

    results = await promiseSearchEntries(["guid"], { fieldname: "field1", value: "value1" });
    let guid = processFirstResult(results)[3];

    await promiseUpdate({ op: "update", guid, value: "modifiedValue" });
    await promiseCountEntries("field1", "modifiedValue", checkExists);
    await promiseCountEntries("field1", "value1", checkNotExists);
    await promiseCountEntries("field1", "value1b", checkExists);
    await promiseCountEntries(null, null, num => Assert.equal(num, 2));

    // ===== 13 =====
    // Add a single entry with times
    testnum++;
    await promiseUpdate({ op: "add", fieldname: "field2", value: "value2",
      timesUsed: 20, firstUsed: 100, lastUsed: 500 });

    results = await promiseSearchEntries(["timesUsed", "firstUsed", "lastUsed"],
                                         { fieldname: "field2", value: "value2" });
    [timesUsed, firstUsed, lastUsed] = processFirstResult(results);

    Assert.equal(20, timesUsed);
    Assert.equal(100, firstUsed);
    Assert.equal(500, lastUsed);
    await promiseCountEntries(null, null, num => Assert.equal(num, 3));

    // ===== 14 =====
    // Bump an entry, which updates its lastUsed field
    testnum++;
    await promiseUpdate({ op: "bump", fieldname: "field2", value: "value2",
      timesUsed: 20, firstUsed: 100, lastUsed: 500 });
    results = await promiseSearchEntries(["timesUsed", "firstUsed", "lastUsed"],
                                         { fieldname: "field2", value: "value2" });
    [timesUsed, firstUsed, lastUsed] = processFirstResult(results);
    Assert.equal(21, timesUsed);
    Assert.equal(100, firstUsed);
    Assert.ok(lastUsed > 500);
    await promiseCountEntries(null, null, num => Assert.equal(num, 3));

    // ===== 15 =====
    // Bump an entry that does not exist
    testnum++;
    await promiseUpdate({ op: "bump", fieldname: "field3", value: "value3",
      timesUsed: 10, firstUsed: 50, lastUsed: 400 });
    results = await promiseSearchEntries(["timesUsed", "firstUsed", "lastUsed"],
                                         { fieldname: "field3", value: "value3" });
    [timesUsed, firstUsed, lastUsed] = processFirstResult(results);
    Assert.equal(10, timesUsed);
    Assert.equal(50, firstUsed);
    Assert.equal(400, lastUsed);
    await promiseCountEntries(null, null, num => Assert.equal(num, 4));

    // ===== 16 =====
    // Bump an entry with a guid
    testnum++;
    results = await promiseSearchEntries(["guid"], { fieldname: "field3", value: "value3" });
    guid = processFirstResult(results)[3];
    await promiseUpdate({ op: "bump", guid, timesUsed: 20, firstUsed: 55, lastUsed: 400 });
    results = await promiseSearchEntries(["timesUsed", "firstUsed", "lastUsed"],
                                         { fieldname: "field3", value: "value3" });
    [timesUsed, firstUsed, lastUsed] = processFirstResult(results);
    Assert.equal(11, timesUsed);
    Assert.equal(50, firstUsed);
    Assert.ok(lastUsed > 400);
    await promiseCountEntries(null, null, num => Assert.equal(num, 4));

    // ===== 17 =====
    // Remove an entry
    testnum++;
    await countDeletedEntries(7);

    results = await promiseSearchEntries(["guid"], { fieldname: "field1", value: "value1b" });
    guid = processFirstResult(results)[3];

    await promiseUpdate({ op: "remove", guid});
    await promiseCountEntries("field1", "modifiedValue", checkExists);
    await promiseCountEntries("field1", "value1b", checkNotExists);
    await promiseCountEntries(null, null, num => Assert.equal(num, 3));

    await countDeletedEntries(8);
    await checkTimeDeleted(guid, timeDeleted => Assert.ok(timeDeleted > 10000));

    // ===== 18 =====
    // Add yet another single entry
    testnum++;
    await promiseUpdate({ op: "add", fieldname: "field4", value: "value4",
      timesUsed: 5, firstUsed: 230, lastUsed: 600 });
    await promiseCountEntries(null, null, num => Assert.equal(num, 4));

    // ===== 19 =====
    // Remove an entry by time
    testnum++;
    await promiseUpdate({ op: "remove", firstUsedStart: 60, firstUsedEnd: 250 });
    await promiseCountEntries("field1", "modifiedValue", checkExists);
    await promiseCountEntries("field2", "value2", checkNotExists);
    await promiseCountEntries("field3", "value3", checkExists);
    await promiseCountEntries("field4", "value4", checkNotExists);
    await promiseCountEntries(null, null, num => Assert.equal(num, 2));
    await countDeletedEntries(10);

    // ===== 20 =====
    // Bump multiple existing entries at once
    testnum++;

    await promiseUpdate([{ op: "add", fieldname: "field5", value: "value5",
      timesUsed: 5, firstUsed: 230, lastUsed: 600 },
    { op: "add", fieldname: "field6", value: "value6",
      timesUsed: 12, firstUsed: 430, lastUsed: 700 }]);
    await promiseCountEntries(null, null, num => Assert.equal(num, 4));

    await promiseUpdate([
      { op: "bump", fieldname: "field5", value: "value5" },
      { op: "bump", fieldname: "field6", value: "value6" }]);
    results = await promiseSearchEntries(["fieldname", "timesUsed", "firstUsed", "lastUsed"], { });

    Assert.equal(6, results[2].timesUsed);
    Assert.equal(13, results[3].timesUsed);
    Assert.equal(230, results[2].firstUsed);
    Assert.equal(430, results[3].firstUsed);
    Assert.ok(results[2].lastUsed > 600);
    Assert.ok(results[3].lastUsed > 700);

    await promiseCountEntries(null, null, num => Assert.equal(num, 4));

    // ===== 21 =====
    // Check update fails if form history is disabled and the operation is not a
    // pure removal.
    testnum++;
    Services.prefs.setBoolPref("browser.formfill.enable", false);

    // Cannot use arrow functions, see bug 1237961.
    Assert.rejects(promiseUpdate(
      { op: "bump", fieldname: "field5", value: "value5" }),
                   function(err) { return err.result == Ci.mozIStorageError.MISUSE; },
                   "bumping when form history is disabled should fail");
    Assert.rejects(promiseUpdate(
      { op: "add", fieldname: "field5", value: "value5" }),
                   function(err) { return err.result == Ci.mozIStorageError.MISUSE; },
                   "Adding when form history is disabled should fail");
    Assert.rejects(promiseUpdate([
      { op: "update", fieldname: "field5", value: "value5" },
      { op: "remove", fieldname: "field5", value: "value5" },
    ]),
                   function(err) { return err.result == Ci.mozIStorageError.MISUSE; },
                   "mixed operations when form history is disabled should fail");
    Assert.rejects(promiseUpdate([
      null, undefined, "", 1, {},
      { op: "remove", fieldname: "field5", value: "value5" },
    ]),
                   function(err) { return err.result == Ci.mozIStorageError.MISUSE; },
                   "Invalid entries when form history is disabled should fail");

    // Remove should work though.
    await promiseUpdate([{ op: "remove", fieldname: "field5", value: null },
      { op: "remove", fieldname: null, value: null }]);
    Services.prefs.clearUserPref("browser.formfill.enable");
  } catch (e) {
    throw new Error(`FAILED in test #${testnum} -- ${e}`);
  } finally {
    FormHistory._supportsDeletedTable = oldSupportsDeletedTable;
    dbConnection.asyncClose(do_test_finished);
  }
});

function run_test() {
  return run_next_test();
}
