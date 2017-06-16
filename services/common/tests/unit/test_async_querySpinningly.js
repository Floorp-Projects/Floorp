/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FormHistory.jsm");
Cu.import("resource://services-common/async.js");
Cu.import("resource://services-common/utils.js");

_("Make sure querySpinningly will synchronously fetch rows for a query asyncly");

const SQLITE_CONSTRAINT_VIOLATION = 19;  // http://www.sqlite.org/c3ref/c_abort.html

// This test is a bit hacky - it was originally written to use the
// formhistory.sqlite database using the nsIFormHistory2 sync APIs. However,
// that's now been deprecated in favour of the async FormHistory.jsm.
// Rather than re-write the test completely, we cheat - we use FormHistory.jsm
// to initialize the database, then we just re-open it for these tests.

// Init the forms database.
FormHistory.schemaVersion;
FormHistory.shutdown();

// and open the database it just created.
let dbFile = Services.dirsvc.get("ProfD", Ci.nsIFile).clone();
dbFile.append("formhistory.sqlite");
let dbConnection = Services.storage.openUnsharedDatabase(dbFile);

do_register_cleanup(() => {
  let cb = Async.makeSpinningCallback();
  dbConnection.asyncClose(cb);
  return cb.wait();
});

function querySpinningly(query, names) {
  let q = dbConnection.createStatement(query);
  let r = Async.querySpinningly(q, names);
  q.finalize();
  return r;
}

function run_test() {
  initTestLogging("Trace");

  _("Make sure the call is async and allows other events to process");
  let isAsync = false;
  CommonUtils.nextTick(function() { isAsync = true; });
  do_check_false(isAsync);

  _("Empty out the formhistory table");
  let r0 = querySpinningly("DELETE FROM moz_formhistory");
  do_check_eq(r0, null);

  _("Make sure there's nothing there");
  let r1 = querySpinningly("SELECT 1 FROM moz_formhistory");
  do_check_eq(r1, null);

  _("Insert a row");
  let r2 = querySpinningly("INSERT INTO moz_formhistory (fieldname, value) VALUES ('foo', 'bar')");
  do_check_eq(r2, null);

  _("Request a known value for the one row");
  let r3 = querySpinningly("SELECT 42 num FROM moz_formhistory", ["num"]);
  do_check_eq(r3.length, 1);
  do_check_eq(r3[0].num, 42);

  _("Get multiple columns");
  let r4 = querySpinningly("SELECT fieldname, value FROM moz_formhistory", ["fieldname", "value"]);
  do_check_eq(r4.length, 1);
  do_check_eq(r4[0].fieldname, "foo");
  do_check_eq(r4[0].value, "bar");

  _("Get multiple columns with a different order");
  let r5 = querySpinningly("SELECT fieldname, value FROM moz_formhistory", ["value", "fieldname"]);
  do_check_eq(r5.length, 1);
  do_check_eq(r5[0].fieldname, "foo");
  do_check_eq(r5[0].value, "bar");

  _("Add multiple entries (sqlite doesn't support multiple VALUES)");
  let r6 = querySpinningly("INSERT INTO moz_formhistory (fieldname, value) SELECT 'foo', 'baz' UNION SELECT 'more', 'values'");
  do_check_eq(r6, null);

  _("Get multiple rows");
  let r7 = querySpinningly("SELECT fieldname, value FROM moz_formhistory WHERE fieldname = 'foo'", ["fieldname", "value"]);
  do_check_eq(r7.length, 2);
  do_check_eq(r7[0].fieldname, "foo");
  do_check_eq(r7[1].fieldname, "foo");

  _("Make sure updates work");
  let r8 = querySpinningly("UPDATE moz_formhistory SET value = 'updated' WHERE fieldname = 'more'");
  do_check_eq(r8, null);

  _("Get the updated");
  let r9 = querySpinningly("SELECT value, fieldname FROM moz_formhistory WHERE fieldname = 'more'", ["fieldname", "value"]);
  do_check_eq(r9.length, 1);
  do_check_eq(r9[0].fieldname, "more");
  do_check_eq(r9[0].value, "updated");

  _("Grabbing fewer fields than queried is fine");
  let r10 = querySpinningly("SELECT value, fieldname FROM moz_formhistory", ["fieldname"]);
  do_check_eq(r10.length, 3);

  _("Generate an execution error");
  let query = "INSERT INTO moz_formhistory (fieldname, value) VALUES ('one', NULL)";
  let stmt = dbConnection.createStatement(query);
  let except;
  try {
    Async.querySpinningly(stmt);
  } catch (e) {
    except = e;
  }
  stmt.finalize()
  do_check_true(!!except);
  do_check_eq(except.result, SQLITE_CONSTRAINT_VIOLATION);

  _("Cleaning up");
  querySpinningly("DELETE FROM moz_formhistory");

  _("Make sure the timeout got to run before this function ends");
  do_check_true(isAsync);
}
