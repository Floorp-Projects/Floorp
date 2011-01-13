/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the async history API exposed by mozIAsyncHistory.
 */

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

function test_interface_exists()
{
  let history = Cc["@mozilla.org/browser/history;1"].getService(Ci.nsISupports);
  do_check_true(history instanceof Ci.mozIAsyncHistory);
  run_next_test();
}

////////////////////////////////////////////////////////////////////////////////
//// Test Runner

let gTests = [
  test_interface_exists,
];

function run_test()
{
  run_next_test();
}
