/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This file tests that dbs are using 32k pagesize

const kExpectedPageSize = 32768; // 32K
const kExpectedCacheSize = -2048; // 2MiB

function check_size(db) {
  var stmt = db.createStatement("PRAGMA page_size");
  stmt.executeStep();
  Assert.equal(stmt.getInt32(0), kExpectedPageSize);
  stmt.finalize();
  stmt = db.createStatement("PRAGMA cache_size");
  stmt.executeStep();
  Assert.equal(stmt.getInt32(0), kExpectedCacheSize);
  stmt.finalize();
}

function new_file(name) {
  var file = Services.dirsvc.get("ProfD", Ci.nsIFile);
  file.append(name + ".sqlite");
  Assert.ok(!file.exists());
  return file;
}

function run_test() {
  check_size(getDatabase(new_file("shared32k")));
  check_size(Services.storage.openUnsharedDatabase(new_file("unshared32k")));
}
