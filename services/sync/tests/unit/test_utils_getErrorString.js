Cu.import("resource://services-sync/util.js");

function run_test() {
  let str;

  // we just test whether the returned string includes the
  // string "unknown", should be good enough

  str = Utils.getErrorString("error.login.reason.account");
  do_check_true(str.match(/unknown/i) == null);

  str = Utils.getErrorString("foobar");
  do_check_true(str.match(/unknown/i) != null);
}
