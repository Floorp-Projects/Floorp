/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This file tests that dbs are using 32k pagesize

function check_size(db)
{
  var stmt = db.createStatement("PRAGMA page_size");
  stmt.executeStep();
  const expected_block_size = 32768; // 32K
  do_check_eq(stmt.getInt32(0), expected_block_size);
  stmt.finalize();
}

function new_file(name)
{
  var file = dirSvc.get("ProfD", Ci.nsIFile);
  file.append(name + ".sqlite");
  do_check_false(file.exists());
  return file;
}

function run_test()
{
  check_size(getDatabase(new_file("shared32k.sqlite")));
  check_size(getService().openUnsharedDatabase(new_file("unshared32k.sqlite")));
}

