/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var cps = new ContentPrefInstance(null);

function run_test() {
  testCacheWorks("test1.example.com", "test-pref1");
  testHasCachedPrefFunction("test2.example.com", "test-pref2");
  testSetCaches("test3.example.com", "test-pref3");
  testGetCaches("test4.example.com", "test-pref4");
  testRemovePrefs("test5.example.com", "test-pref5");
  testTypeConversions("test6.example.com", "test-pref6");
  testNonExistingPrefCachesAsUndefined("test7.example.com", "test-pref7");
  testCacheEviction("test8.example.com", "test-pref8");
}

function testCacheWorks(uri, prefName) {
  const CACHED_VALUE = 3;
  const NEW_VALUE = 5;

  cps.setPref(uri, prefName, CACHED_VALUE);
  do_check_eq(cps.getPref(uri, prefName), CACHED_VALUE);

  // Now change the value directly through the DB and check
  // that the cached value is different

  let groupId = selectValue("SELECT id FROM groups WHERE name = :param1", "id", uri);
  let settingId = selectValue("SELECT id FROM settings WHERE name = :param1", "id", prefName);
  let prefId = selectValue("SELECT id FROM prefs WHERE groupID = :param1 AND settingID = :param2",
                           "id", groupId, settingId);

  let stmt = cps.DBConnection.createStatement("UPDATE prefs SET value = :value WHERE id = :id");
  stmt.params.value = NEW_VALUE;
  stmt.params.id = prefId;
  stmt.execute();

  let dbValue = selectValue("SELECT value FROM prefs WHERE id = :param1", "value", prefId);
  let cacheValue = cps.getPref(uri, prefName);

  do_check_eq(dbValue, NEW_VALUE);
  do_check_eq(cacheValue, CACHED_VALUE);
  do_check_neq(cacheValue, dbValue);

  do_test_pending();
  cps.getPref(uri, prefName, function(value) {
    do_check_eq(dbValue, NEW_VALUE);
    do_check_eq(value, CACHED_VALUE);
    do_check_neq(value, dbValue);
    do_test_finished();
  });
}

function testHasCachedPrefFunction(uri, prefName) {
  const STARTING_VALUE = 3;
  const NEW_VALUE = 5;

  do_check_false(isCached(uri, prefName));

  cps.setPref(uri, prefName, STARTING_VALUE);

  let groupId = selectValue("SELECT id FROM groups WHERE name = :param1", "id", uri);
  let settingId = selectValue("SELECT id FROM settings WHERE name = :param1", "id", prefName);
  let prefId = selectValue("SELECT id FROM prefs WHERE groupID = :param1 AND settingID = :param2",
                       "id", groupId, settingId);

  do_check_neq(prefId, undefined);

  let originalValue = selectValue("SELECT value FROM prefs WHERE id = :param1", "value", prefId);
  do_check_eq(originalValue, STARTING_VALUE);

  let stmt = cps.DBConnection.createStatement("UPDATE prefs SET value = :value WHERE id = :id");
  stmt.params.value = NEW_VALUE;
  stmt.params.id = prefId;
  stmt.execute();

  let newValue = selectValue("SELECT value FROM prefs WHERE id = :param1", "value", prefId);
  do_check_eq(newValue, NEW_VALUE);

  let cachedValue = cps.getPref(uri, prefName);
  do_check_eq(cachedValue, STARTING_VALUE);
  do_check_true(isCached(uri, prefName));
}

function testSetCaches(uri, prefName) {
  cps.setPref(uri, prefName, 0);
  do_check_true(isCached(uri, prefName));
}

function testRemovePrefs(uri, prefName) {

  /* removePref */
  cps.setPref("www1." + uri, prefName, 1);

  do_check_eq(cps.getPref("www1." + uri, prefName), 1);

  cps.removePref("www1." + uri, prefName);

  do_check_false(isCached("www1." + uri, prefName));
  do_check_false(cps.hasPref("www1." + uri, prefName));
  do_check_neq(cps.getPref("www1." + uri, prefName), 1);

  /* removeGroupedPrefs */
  cps.setPref("www2." + uri, prefName, 2);
  cps.setPref("www3." + uri, prefName, 3);

  do_check_eq(cps.getPref("www2." + uri, prefName), 2);
  do_check_eq(cps.getPref("www3." + uri, prefName), 3);

  cps.removeGroupedPrefs();

  do_check_false(isCached("www2." + uri, prefName));
  do_check_false(isCached("www3." + uri, prefName));
  do_check_false(cps.hasPref("www2." + uri, prefName));
  do_check_false(cps.hasPref("www3." + uri, prefName));
  do_check_neq(cps.getPref("www2." + uri, prefName), 2);
  do_check_neq(cps.getPref("www3." + uri, prefName), 3);

  /* removePrefsByName */
  cps.setPref("www4." + uri, prefName, 4);
  cps.setPref("www5." + uri, prefName, 5);

  do_check_eq(cps.getPref("www4." + uri, prefName), 4);
  do_check_eq(cps.getPref("www5." + uri, prefName), 5);

  cps.removePrefsByName(prefName);

  do_check_false(isCached("www4." + uri, prefName));
  do_check_false(isCached("www5." + uri, prefName));
  do_check_false(cps.hasPref("www4." + uri, prefName));
  do_check_false(cps.hasPref("www5." + uri, prefName));
  do_check_neq(cps.getPref("www4." + uri, prefName), 4);
  do_check_neq(cps.getPref("www5." + uri, prefName), 5);
}

function testGetCaches(uri, prefName) {
  const VALUE = 4;

  let insertGroup = cps.DBConnection.createStatement("INSERT INTO groups (name) VALUES (:name)");
  insertGroup.params.name = uri;
  insertGroup.execute();
  let groupId = cps.DBConnection.lastInsertRowID;

  let insertSetting = cps.DBConnection.createStatement("INSERT INTO settings (name) VALUES (:name)");
  insertSetting.params.name = prefName;
  insertSetting.execute();
  let settingId = cps.DBConnection.lastInsertRowID;

  let insertPref = cps.DBConnection.createStatement(`
    INSERT INTO prefs (groupID, settingID, value)
    VALUES (:groupId, :settingId, :value)
  `);
  insertPref.params.groupId = groupId;
  insertPref.params.settingId = settingId;
  insertPref.params.value = VALUE;
  insertPref.execute();
  let prefId = cps.DBConnection.lastInsertRowID;

  let dbValue = selectValue("SELECT value FROM prefs WHERE id = :param1", "value", prefId);

  // First access from service should hit the DB
  let svcValue = cps.getPref(uri, prefName);

  // Second time should get the value from cache
  let cacheValue = cps.getPref(uri, prefName);

  do_check_eq(VALUE, dbValue);
  do_check_eq(VALUE, svcValue);
  do_check_eq(VALUE, cacheValue);

  do_check_true(isCached(uri, prefName));
}

function testTypeConversions(uri, prefName) {
  let value;

  cps.setPref(uri, prefName, true);
  value = cps.getPref(uri, prefName);
  do_check_true(value === 1);

  cps.setPref(uri, prefName, false);
  value = cps.getPref(uri, prefName);
  do_check_true(value === 0);

  cps.setPref(uri, prefName, null);
  value = cps.getPref(uri, prefName);
  do_check_true(value === null);

  cps.setPref(uri, prefName, undefined);
  value = cps.getPref(uri, prefName);
  do_check_true(value === null);
}

function testNonExistingPrefCachesAsUndefined(uri, prefName) {

  do_check_false(isCached(uri, prefName));

  // Cache the pref
  let value = cps.getPref(uri, prefName);
  do_check_true(value === undefined);

  do_check_true(isCached(uri, prefName));

  // Cached pref
  value = cps.getPref(uri, prefName);
  do_check_true(value === undefined);
}

function testCacheEviction(uri, prefName) {

  cps.setPref(uri, prefName, 5);
  do_check_eq(cps.getPref(uri, prefName), 5);
  do_check_true(isCached(uri, prefName));

  // try to evict value from cache by adding various other entries
  const ENTRIES_TO_ADD = 200;
  for (let i = 0; i < ENTRIES_TO_ADD; i++) {
    let uriToAdd = "www" + i + uri;
    cps.setPref(uriToAdd, prefName, 0);
  }

  do_check_false(isCached(uri, prefName));

}

function selectValue(stmt, columnName, param1, param2) {
  stmt = cps.DBConnection.createStatement(stmt);
  if (param1)
    stmt.params.param1 = param1;

  if (param2)
    stmt.params.param2 = param2;

  stmt.executeStep();
  let val = stmt.row[columnName];
  stmt.reset();
  stmt.finalize();
  return val;
}

function isCached(uri, prefName) {
  return cps.hasCachedPref(uri, prefName);
}
