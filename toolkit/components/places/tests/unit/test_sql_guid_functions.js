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
function check_invariants(aGuid) {
  do_print("Checking guid '" + aGuid + "'");

  do_check_valid_places_guid(aGuid);
}

// Test Functions

function test_guid_invariants() {
  const kExpectedChars = 64;
  const kAllowedChars =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";
  do_check_eq(kAllowedChars.length, kExpectedChars);
  const kGuidLength = 12;

  let checkedChars = [];
  for (let i = 0; i < kGuidLength; i++) {
    checkedChars[i] = {};
    for (let j = 0; j < kAllowedChars; j++) {
      checkedChars[i][kAllowedChars[j]] = false;
    }
  }

  // We run this until we've seen every character that we expect to see in every
  // position.
  let seenChars = 0;
  let stmt = DBConn().createStatement("SELECT GENERATE_GUID()");
  while (seenChars != (kExpectedChars * kGuidLength)) {
    do_check_true(stmt.executeStep());
    let guid = stmt.getString(0);
    check_invariants(guid);

    for (let i = 0; i < guid.length; i++) {
      let character = guid[i];
      if (!checkedChars[i][character]) {
        checkedChars[i][character] = true;
        seenChars++;
      }
    }
    stmt.reset();
  }
  stmt.finalize();

  // One last reality check - make sure all of our characters were seen.
  for (let i = 0; i < kGuidLength; i++) {
    for (let j = 0; j < kAllowedChars; j++) {
      do_check_true(checkedChars[i][kAllowedChars[j]]);
    }
  }

  run_next_test();
}

function test_guid_on_background() {
  // We should not assert if we execute this asynchronously.
  let stmt = DBConn().createAsyncStatement("SELECT GENERATE_GUID()");
  let checked = false;
  stmt.executeAsync({
    handleResult(aResult) {
      try {
        let row = aResult.getNextRow();
        check_invariants(row.getResultByIndex(0));
        do_check_eq(aResult.getNextRow(), null);
        checked = true;
      } catch (e) {
        do_throw(e);
      }
    },
    handleCompletion(aReason) {
      do_check_eq(aReason, Ci.mozIStorageStatementCallback.REASON_FINISHED);
      do_check_true(checked);
      run_next_test();
    }
  });
  stmt.finalize();
}

// Test Runner

[
  test_guid_invariants,
  test_guid_on_background,
].forEach(add_test);
