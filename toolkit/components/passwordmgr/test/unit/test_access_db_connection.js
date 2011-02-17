/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const STORAGE_TYPE = "mozStorage";

function run_test()
{
  // Make sure that the storage component exposes its database connection.
  let storage = LoginTest.initStorage(INDIR, "signons-empty.txt", OUTDIR,
                                      "signons-empty.sqlite");
  do_check_true(storage instanceof Ci.nsIInterfaceRequestor);
  let db = storage.getInterface(Ci.mozIStorageConnection);
  do_check_neq(db, null);
  do_check_true(db.connectionReady);

  // Make sure that the login manager exposes its the storage component's
  // database connection.
  let lm = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
  do_check_true(lm instanceof Ci.nsIInterfaceRequestor);
  db = lm.getInterface(Ci.mozIStorageConnection);
  do_check_neq(db, null);
  do_check_true(db.connectionReady);
}
