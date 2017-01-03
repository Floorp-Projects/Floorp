/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var testnum = 0;
var dbConnection; // used for deleted table tests

Cu.import("resource://gre/modules/Promise.jsm");

function countDeletedEntries(expected)
{
  let deferred = Promise.defer();
  let stmt = dbConnection.createAsyncStatement("SELECT COUNT(*) AS numEntries FROM moz_deleted_formhistory");
  stmt.executeAsync({
    handleResult(resultSet) {
      do_check_eq(expected, resultSet.getNextRow().getResultByName("numEntries"));
      deferred.resolve();
    },
    handleError(error) {
      do_throw("Error occurred counting deleted entries: " + error);
      deferred.reject();
    },
    handleCompletion() {
      stmt.finalize();
    }
  });
  return deferred.promise;
}

function checkTimeDeleted(guid, checkFunction)
{
  let deferred = Promise.defer();
  let stmt = dbConnection.createAsyncStatement("SELECT timeDeleted FROM moz_deleted_formhistory WHERE guid = :guid");
  stmt.params.guid = guid;
  stmt.executeAsync({
    handleResult(resultSet) {
      checkFunction(resultSet.getNextRow().getResultByName("timeDeleted"));
      deferred.resolve();
    },
    handleError(error) {
      do_throw("Error occurred getting deleted entries: " + error);
      deferred.reject();
    },
    handleCompletion() {
      stmt.finalize();
    }
  });
  return deferred.promise;
}

function promiseUpdateEntry(op, name, value)
{
  var change = { op };
  if (name !== null)
    change.fieldname = name;
  if (value !== null)
    change.value = value;
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
      }
    });
  });
}

function promiseSearchEntries(terms, params)
{
  let deferred = Promise.defer();
  let results = [];
  FormHistory.search(terms, params,
                     { handleResult: result => results.push(result),
                       handleError(error) {
                         do_throw("Error occurred searching form history: " + error);
                         deferred.reject(error);
                       },
                       handleCompletion(reason) { if (!reason) deferred.resolve(results); }
                     });
  return deferred.promise;
}

function promiseCountEntries(name, value, checkFn)
{
  let deferred = Promise.defer();
  countEntries(name, value, function(result) { checkFn(result); deferred.resolve(); } );
  return deferred.promise;
}

add_task(function* ()
{
  let oldSupportsDeletedTable = FormHistory._supportsDeletedTable;
  FormHistory._supportsDeletedTable = true;

  try {

  // ===== test init =====
  var testfile = do_get_file("formhistory_apitest.sqlite");
  var profileDir = dirSvc.get("ProfD", Ci.nsIFile);

  // Cleanup from any previous tests or failures.
  var destFile = profileDir.clone();
  destFile.append("formhistory.sqlite");
  if (destFile.exists())
    destFile.remove(false);

  testfile.copyTo(profileDir, "formhistory.sqlite");

  function checkExists(num) { do_check_true(num > 0); }
  function checkNotExists(num) { do_check_true(num == 0); }

  // ===== 1 =====
  // Check initial state is as expected
  testnum++;
  yield promiseCountEntries("name-A", null, checkExists);
  yield promiseCountEntries("name-B", null, checkExists);
  yield promiseCountEntries("name-C", null, checkExists);
  yield promiseCountEntries("name-D", null, checkExists);
  yield promiseCountEntries("name-A", "value-A", checkExists);
  yield promiseCountEntries("name-B", "value-B1", checkExists);
  yield promiseCountEntries("name-B", "value-B2", checkExists);
  yield promiseCountEntries("name-C", "value-C", checkExists);
  yield promiseCountEntries("name-D", "value-D", checkExists);
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
    }
  });
  yield deferred.promise;

  // ===== 2 =====
  // Test looking for nonexistent / bogus data.
  testnum++;
  yield promiseCountEntries("blah", null, checkNotExists);
  yield promiseCountEntries("", null, checkNotExists);
  yield promiseCountEntries("name-A", "blah", checkNotExists);
  yield promiseCountEntries("name-A", "", checkNotExists);
  yield promiseCountEntries("name-A", null, checkExists);
  yield promiseCountEntries("blah", "value-A", checkNotExists);
  yield promiseCountEntries("", "value-A", checkNotExists);
  yield promiseCountEntries(null, "value-A", checkExists);

  // Cannot use promiseCountEntries when name and value are null because it treats null values as not set
  // and here a search should be done explicity for null.
  deferred = Promise.defer();
  yield FormHistory.count({ fieldname: null, value: null },
                          { handleResult: result => checkNotExists(result),
                            handleError(error) {
                              do_throw("Error occurred searching form history: " + error);
                            },
                            handleCompletion(reason) { if (!reason) deferred.resolve() }
                          });
  yield deferred.promise;

  // ===== 3 =====
  // Test removeEntriesForName with a single matching value
  testnum++;
  yield promiseUpdateEntry("remove", "name-A", null);

  yield promiseCountEntries("name-A", "value-A", checkNotExists);
  yield promiseCountEntries("name-B", "value-B1", checkExists);
  yield promiseCountEntries("name-B", "value-B2", checkExists);
  yield promiseCountEntries("name-C", "value-C", checkExists);
  yield promiseCountEntries("name-D", "value-D", checkExists);
  yield countDeletedEntries(1);

  // ===== 4 =====
  // Test removeEntriesForName with multiple matching values
  testnum++;
  yield promiseUpdateEntry("remove", "name-B", null);

  yield promiseCountEntries("name-A", "value-A", checkNotExists);
  yield promiseCountEntries("name-B", "value-B1", checkNotExists);
  yield promiseCountEntries("name-B", "value-B2", checkNotExists);
  yield promiseCountEntries("name-C", "value-C", checkExists);
  yield promiseCountEntries("name-D", "value-D", checkExists);
  yield countDeletedEntries(3);

  // ===== 5 =====
  // Test removing by time range (single entry, not surrounding entries)
  testnum++;
  yield promiseCountEntries("time-A", null, checkExists); // firstUsed=1000, lastUsed=1000
  yield promiseCountEntries("time-B", null, checkExists); // firstUsed=1000, lastUsed=1099
  yield promiseCountEntries("time-C", null, checkExists); // firstUsed=1099, lastUsed=1099
  yield promiseCountEntries("time-D", null, checkExists); // firstUsed=2001, lastUsed=2001
  yield promiseUpdate({ op : "remove", firstUsedStart: 1050, firstUsedEnd: 2000 });

  yield promiseCountEntries("time-A", null, checkExists);
  yield promiseCountEntries("time-B", null, checkExists);
  yield promiseCountEntries("time-C", null, checkNotExists);
  yield promiseCountEntries("time-D", null, checkExists);
  yield countDeletedEntries(4);

  // ===== 6 =====
  // Test removing by time range (multiple entries)
  testnum++;
  yield promiseUpdate({ op : "remove", firstUsedStart: 1000, firstUsedEnd: 2000 });

  yield promiseCountEntries("time-A", null, checkNotExists);
  yield promiseCountEntries("time-B", null, checkNotExists);
  yield promiseCountEntries("time-C", null, checkNotExists);
  yield promiseCountEntries("time-D", null, checkExists);
  yield countDeletedEntries(6);

  // ===== 7 =====
  // test removeAllEntries
  testnum++;
  yield promiseUpdateEntry("remove", null, null);

  yield promiseCountEntries("name-C", null, checkNotExists);
  yield promiseCountEntries("name-D", null, checkNotExists);
  yield promiseCountEntries("name-C", "value-C", checkNotExists);
  yield promiseCountEntries("name-D", "value-D", checkNotExists);

  yield promiseCountEntries(null, null, checkNotExists);
  yield countDeletedEntries(6);

  // ===== 8 =====
  // Add a single entry back
  testnum++;
  yield promiseUpdateEntry("add", "newname-A", "newvalue-A");
  yield promiseCountEntries("newname-A", "newvalue-A", checkExists);

  // ===== 9 =====
  // Remove the single entry
  testnum++;
  yield promiseUpdateEntry("remove", "newname-A", "newvalue-A");
  yield promiseCountEntries("newname-A", "newvalue-A", checkNotExists);

  // ===== 10 =====
  // Add a single entry
  testnum++;
  yield promiseUpdateEntry("add", "field1", "value1");
  yield promiseCountEntries("field1", "value1", checkExists);

  let processFirstResult = function processResults(results)
  {
    // Only handle the first result
    if (results.length > 0) {
      let result = results[0];
      return [result.timesUsed, result.firstUsed, result.lastUsed, result.guid];
    }
    return undefined;
  }

  let results = yield promiseSearchEntries(["timesUsed", "firstUsed", "lastUsed"],
                                           { fieldname: "field1", value: "value1" });
  let [timesUsed, firstUsed, lastUsed] = processFirstResult(results);
  do_check_eq(1, timesUsed);
  do_check_true(firstUsed > 0);
  do_check_true(lastUsed > 0);
  yield promiseCountEntries(null, null, num => do_check_eq(num, 1));

  // ===== 11 =====
  // Add another single entry
  testnum++;
  yield promiseUpdateEntry("add", "field1", "value1b");
  yield promiseCountEntries("field1", "value1", checkExists);
  yield promiseCountEntries("field1", "value1b", checkExists);
  yield promiseCountEntries(null, null, num => do_check_eq(num, 2));

  // ===== 12 =====
  // Update a single entry
  testnum++;

  results = yield promiseSearchEntries(["guid"], { fieldname: "field1", value: "value1" });
  let guid = processFirstResult(results)[3];

  yield promiseUpdate({ op : "update", guid, value: "modifiedValue" });
  yield promiseCountEntries("field1", "modifiedValue", checkExists);
  yield promiseCountEntries("field1", "value1", checkNotExists);
  yield promiseCountEntries("field1", "value1b", checkExists);
  yield promiseCountEntries(null, null, num => do_check_eq(num, 2));

  // ===== 13 =====
  // Add a single entry with times
  testnum++;
  yield promiseUpdate({ op : "add", fieldname: "field2", value: "value2",
                        timesUsed: 20, firstUsed: 100, lastUsed: 500 });

  results = yield promiseSearchEntries(["timesUsed", "firstUsed", "lastUsed"],
                                       { fieldname: "field2", value: "value2" });
  [timesUsed, firstUsed, lastUsed] = processFirstResult(results);

  do_check_eq(20, timesUsed);
  do_check_eq(100, firstUsed);
  do_check_eq(500, lastUsed);
  yield promiseCountEntries(null, null, num => do_check_eq(num, 3));

  // ===== 14 =====
  // Bump an entry, which updates its lastUsed field
  testnum++;
  yield promiseUpdate({ op : "bump", fieldname: "field2", value: "value2",
                        timesUsed: 20, firstUsed: 100, lastUsed: 500 });
  results = yield promiseSearchEntries(["timesUsed", "firstUsed", "lastUsed"],
                                       { fieldname: "field2", value: "value2" });
  [timesUsed, firstUsed, lastUsed] = processFirstResult(results);
  do_check_eq(21, timesUsed);
  do_check_eq(100, firstUsed);
  do_check_true(lastUsed > 500);
  yield promiseCountEntries(null, null, num => do_check_eq(num, 3));

  // ===== 15 =====
  // Bump an entry that does not exist
  testnum++;
  yield promiseUpdate({ op : "bump", fieldname: "field3", value: "value3",
                        timesUsed: 10, firstUsed: 50, lastUsed: 400 });
  results = yield promiseSearchEntries(["timesUsed", "firstUsed", "lastUsed"],
                                       { fieldname: "field3", value: "value3" });
  [timesUsed, firstUsed, lastUsed] = processFirstResult(results);
  do_check_eq(10, timesUsed);
  do_check_eq(50, firstUsed);
  do_check_eq(400, lastUsed);
  yield promiseCountEntries(null, null, num => do_check_eq(num, 4));

  // ===== 16 =====
  // Bump an entry with a guid
  testnum++;
  results = yield promiseSearchEntries(["guid"], { fieldname: "field3", value: "value3" });
  guid = processFirstResult(results)[3];
  yield promiseUpdate({ op : "bump", guid, timesUsed: 20, firstUsed: 55, lastUsed: 400 });
  results = yield promiseSearchEntries(["timesUsed", "firstUsed", "lastUsed"],
                                       { fieldname: "field3", value: "value3" });
  [timesUsed, firstUsed, lastUsed] = processFirstResult(results);
  do_check_eq(11, timesUsed);
  do_check_eq(50, firstUsed);
  do_check_true(lastUsed > 400);
  yield promiseCountEntries(null, null, num => do_check_eq(num, 4));

  // ===== 17 =====
  // Remove an entry
  testnum++;
  yield countDeletedEntries(7);

  results = yield promiseSearchEntries(["guid"], { fieldname: "field1", value: "value1b" });
  guid = processFirstResult(results)[3];

  yield promiseUpdate({ op : "remove", guid});
  yield promiseCountEntries("field1", "modifiedValue", checkExists);
  yield promiseCountEntries("field1", "value1b", checkNotExists);
  yield promiseCountEntries(null, null, num => do_check_eq(num, 3));

  yield countDeletedEntries(8);
  yield checkTimeDeleted(guid, timeDeleted => do_check_true(timeDeleted > 10000));

  // ===== 18 =====
  // Add yet another single entry
  testnum++;
  yield promiseUpdate({ op : "add", fieldname: "field4", value: "value4",
                        timesUsed: 5, firstUsed: 230, lastUsed: 600 });
  yield promiseCountEntries(null, null, num => do_check_eq(num, 4));

  // ===== 19 =====
  // Remove an entry by time
  testnum++;
  yield promiseUpdate({ op : "remove", firstUsedStart: 60, firstUsedEnd: 250 });
  yield promiseCountEntries("field1", "modifiedValue", checkExists);
  yield promiseCountEntries("field2", "value2", checkNotExists);
  yield promiseCountEntries("field3", "value3", checkExists);
  yield promiseCountEntries("field4", "value4", checkNotExists);
  yield promiseCountEntries(null, null, num => do_check_eq(num, 2));
  yield countDeletedEntries(10);

  // ===== 20 =====
  // Bump multiple existing entries at once
  testnum++;

  yield promiseUpdate([{ op : "add", fieldname: "field5", value: "value5",
                         timesUsed: 5, firstUsed: 230, lastUsed: 600 },
                       { op : "add", fieldname: "field6", value: "value6",
                         timesUsed: 12, firstUsed: 430, lastUsed: 700 }]);
  yield promiseCountEntries(null, null, num => do_check_eq(num, 4));

  yield promiseUpdate([
                       { op : "bump", fieldname: "field5", value: "value5" },
                       { op : "bump", fieldname: "field6", value: "value6" }]);
  results = yield promiseSearchEntries(["fieldname", "timesUsed", "firstUsed", "lastUsed"], { });

  do_check_eq(6, results[2].timesUsed);
  do_check_eq(13, results[3].timesUsed);
  do_check_eq(230, results[2].firstUsed);
  do_check_eq(430, results[3].firstUsed);
  do_check_true(results[2].lastUsed > 600);
  do_check_true(results[3].lastUsed > 700);

  yield promiseCountEntries(null, null, num => do_check_eq(num, 4));

  // ===== 21 =====
  // Check update fails if form history is disabled and the operation is not a
  // pure removal.
  testnum++;
  Services.prefs.setBoolPref("browser.formfill.enable", false);

  // Cannot use arrow functions, see bug 1237961.
  Assert.rejects(promiseUpdate(
                   { op : "bump", fieldname: "field5", value: "value5" }),
                 function(err) { return err.result == Ci.mozIStorageError.MISUSE; },
                 "bumping when form history is disabled should fail");
  Assert.rejects(promiseUpdate(
                   { op : "add", fieldname: "field5", value: "value5" }),
                 function(err) { return err.result == Ci.mozIStorageError.MISUSE; },
                 "Adding when form history is disabled should fail");
  Assert.rejects(promiseUpdate([
                     { op : "update", fieldname: "field5", value: "value5" },
                     { op : "remove", fieldname: "field5", value: "value5" }
                   ]),
                 function(err) { return err.result == Ci.mozIStorageError.MISUSE; },
                 "mixed operations when form history is disabled should fail");
  Assert.rejects(promiseUpdate([
                     null, undefined, "", 1, {},
                     { op : "remove", fieldname: "field5", value: "value5" }
                   ]),
                 function(err) { return err.result == Ci.mozIStorageError.MISUSE; },
                 "Invalid entries when form history is disabled should fail");

  // Remove should work though.
  yield promiseUpdate([{ op: "remove", fieldname: "field5", value: null },
                       { op: "remove", fieldname: null, value: null }]);
  Services.prefs.clearUserPref("browser.formfill.enable");

  } catch (e) {
    throw "FAILED in test #" + testnum + " -- " + e;
  }
  finally {
    FormHistory._supportsDeletedTable = oldSupportsDeletedTable;
    dbConnection.asyncClose(do_test_finished);
  }
});

function run_test() {
  return run_next_test();
}
