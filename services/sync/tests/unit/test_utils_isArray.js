Cu.import("resource://services-sync/util.js");

let undef;

function run_test() {
  do_check_true(Utils.isArray([]));
  do_check_true(Utils.isArray([1, 2]));
  do_check_false(Utils.isArray({}));
  do_check_false(Utils.isArray(1.0));
  do_check_false(Utils.isArray("string"));
  do_check_false(Utils.isArray(null));
  do_check_false(Utils.isArray(undef));
  do_check_false(Utils.isArray(new Date()));
}
