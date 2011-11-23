/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This file tests that dbs of various page sizes are using the right cache
// size (bug 703113).

/**
 * In order to change the cache size, we must open a DB, change the page
 * size, create a table, close the DB, then re-open the DB.  We then check
 * the cache size after reopening.
 *
 * @param dbOpener
 *        function that opens the DB specified in file
 * @param file
 *        file holding the database
 * @param pageSize
 *        the DB's page size
 * @param expectedCacheSize
 *        the expected cache size for the given page size
 */
function check_size(dbOpener, file, pageSize, expectedCacheSize)
{
  // Open the DB, immediately change its page size.
  let db = dbOpener(file);
  db.executeSimpleSQL("PRAGMA page_size = " + pageSize);

  // Check the page size change worked.
  let stmt = db.createStatement("PRAGMA page_size");
  do_check_true(stmt.executeStep());
  do_check_eq(stmt.row.page_size, pageSize);
  stmt.finalize();

  // Create a simple table.
  db.executeSimpleSQL("CREATE TABLE test ( id INTEGER PRIMARY KEY )");

  // Close and re-open the DB.
  db.close();
  db = dbOpener(file);

  // Check cache size is as expected.
  let stmt = db.createStatement("PRAGMA cache_size");
  do_check_true(stmt.executeStep());
  do_check_eq(stmt.row.cache_size, expectedCacheSize);
  stmt.finalize();
}

function new_file(name)
{
  let file = dirSvc.get("ProfD", Ci.nsIFile);
  file.append(name + ".sqlite");
  do_check_false(file.exists());
  return file;
}

function run_test()
{
  // This is copied from the logic in Connection::initialize().
  function cacheSize(pageSize) {
    const DEFAULT_CACHE_SIZE_PAGES = 2000;
    const MAX_CACHE_SIZE_BYTES = 4 * 1024 * 1024;
    return Math.min(DEFAULT_CACHE_SIZE_PAGES, MAX_CACHE_SIZE_BYTES / pageSize);
  }

  let pageSizes = [
    1024,
    4096,
    32768,
  ];

  for (let i = 0; i < pageSizes.length; i++) {
    let pageSize = pageSizes[i];
    let expectedCacheSize = cacheSize(pageSize);
    check_size(getDatabase,
               new_file("shared" + pageSize), pageSize, expectedCacheSize);
    check_size(getService().openUnsharedDatabase,
               new_file("unshared" + pageSize), pageSize, expectedCacheSize);
  }
}

