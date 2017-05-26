/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../head.js */

function run_test() {
  do_check_throws_nsIException(function() {
    throw Error("I find your relaxed dishabille unpalatable");
  }, "NS_NOINTERFACE");
}

