/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../head.js */

function run_test() {
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  do_check_throws_nsIException(function() {
    env.QueryInterface(Ci.nsIFile);
  }, "NS_NOINTERFACE");
}
