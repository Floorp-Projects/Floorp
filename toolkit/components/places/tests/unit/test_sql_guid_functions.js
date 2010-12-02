/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests that the guid function generates a guid of the proper length,
 * with no invalid characters.
 */

/**
 * Checks all our invariants about our guids for a given result.
 *
 * @param aGuid
 *        The guid to check.
 */
function check_invariants(aGuid)
{
  print("TEST-INFO | " + tests[index - 1].name + " | Checking guid '" +
        aGuid + "'");

  do_check_valid_places_guid(aGuid);
}

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

function test_guid_invariants()
{
  const kExpectedChars = 64;
  const kAllowedChars =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_"
  do_check_eq(kAllowedChars.length, kExpectedChars);
  let checkedChars = {};
  for (let i = 0; i < kAllowedChars; i++) {
    checkedChars[kAllowedChars[i]] = false;
  }

  // We run this until we've seen every character that we expect to see.
  let seenChars = 0;
  let stmt = DBConn().createStatement("SELECT GENERATE_GUID()");
  while (seenChars != kExpectedChars) {
    do_check_true(stmt.executeStep());
    let guid = stmt.getString(0);
    check_invariants(guid);

    for (let i = 0; i < guid.length; i++) {
      if (!checkedChars[guid[i]]) {
        checkedChars[guid[i]] = true;
        seenChars++;
      }
    }
    stmt.reset();
  }
  stmt.finalize();

  // One last reality check - make sure all of our characters were seen.
  for (let i = 0; i < kAllowedChars; i++) {
    do_check_true(checkedChars[kAllowedChars[i]]);
  }

  run_next_test();
}

function test_guid_on_background()
{
  // We should not assert if we execute this asynchronously.
  let stmt = DBConn().createAsyncStatement("SELECT GENERATE_GUID()");
  let checked = false;
  stmt.executeAsync({
    handleResult: function(aResult) {
      try {
        let row = aResult.getNextRow();
        check_invariants(row.getResultByIndex(0));
        do_check_eq(aResult.getNextRow(), null);
        checked = true;
      }
      catch (e) {
        do_throw(e);
      }
    },
    handleCompletion: function(aReason) {
      do_check_eq(aReason, Ci.mozIStorageStatementCallback.REASON_FINISHED);
      do_check_true(checked);
      run_next_test();
    }
  });
  stmt.finalize();
}

////////////////////////////////////////////////////////////////////////////////
//// Test Runner

let tests = [
  test_guid_invariants,
  test_guid_on_background,
];
let index = 0;

function run_next_test()
{
  function _run_next_test() {
    if (index < tests.length) {
      do_test_pending();
      print("TEST-INFO | " + _TEST_FILE + " | Starting " + tests[index].name);

      // Exceptions do not kill asynchronous tests, so they'll time out.
      try {
        tests[index++]();
      }
      catch (e) {
        do_throw(e);
      }
    }

    do_test_finished();
  }

  // For sane stacks during failures, we execute this code soon, but not now.
  do_execute_soon(_run_next_test);
}

function run_test()
{
  do_test_pending();
  run_next_test();
}
