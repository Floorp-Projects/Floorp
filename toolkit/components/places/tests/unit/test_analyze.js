/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests sqlite_sta1 table exists, it should be created by analyze.
// Since the bookmark roots are created when the DB is created (bug 704855),
// the table will contain data.

function run_test() {
  do_test_pending();

  let stmt = DBConn().createAsyncStatement(
    "SELECT ROWID FROM sqlite_stat1"
  );
  stmt.executeAsync({
    _gotResult: false,
    handleResult(aResultSet) {
      this._gotResult = true;
    },
    handleError(aError) {
      do_throw("Unexpected error (" + aError.result + "): " + aError.message);
    },
    handleCompletion(aReason) {
      do_check_true(this._gotResult);
       do_test_finished();
    }
  });
  stmt.finalize();
}
