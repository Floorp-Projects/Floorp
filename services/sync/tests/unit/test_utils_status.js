Cu.import("resource://services-sync/util.js");

// both the checkStatus and ensureStatus functions are tested
// here.

function run_test() {
  _test_checkStatus();
  _test_ensureStatus();
}

function _test_checkStatus() {
  let msg = "msg";

  _("test with default range");
  do_check_true(Utils.checkStatus(200, msg));
  do_check_true(Utils.checkStatus(299, msg));
  do_check_false(Utils.checkStatus(199, msg));
  do_check_false(Utils.checkStatus(300, msg));

  _("test with a number");
  do_check_true(Utils.checkStatus(100, msg, [100]));
  do_check_false(Utils.checkStatus(200, msg, [100]));

  _("test with two numbers");
  do_check_true(Utils.checkStatus(100, msg, [100, 200]));
  do_check_true(Utils.checkStatus(200, msg, [100, 200]));
  do_check_false(Utils.checkStatus(50, msg, [100, 200]));
  do_check_false(Utils.checkStatus(150, msg, [100, 200]));
  do_check_false(Utils.checkStatus(250, msg, [100, 200]));

  _("test with a range and a number");
  do_check_true(Utils.checkStatus(50, msg, [[50, 100], 100]));
  do_check_true(Utils.checkStatus(75, msg, [[50, 100], 100]));
  do_check_true(Utils.checkStatus(100, msg, [[50, 100], 100]));
  do_check_false(Utils.checkStatus(200, msg, [[50, 100], 100]));

  _("test with a number and a range");
  do_check_true(Utils.checkStatus(50, msg, [100, [50, 100]]));
  do_check_true(Utils.checkStatus(75, msg, [100, [50, 100]]));
  do_check_true(Utils.checkStatus(100, msg, [100, [50, 100]]));
  do_check_false(Utils.checkStatus(200, msg, [100, [50, 100]]));
}

function _test_ensureStatus() {
  _("test that ensureStatus throws exception when it should");
  let except;
  try {
    Utils.ensureStatus(400, "msg", [200]);
  } catch(e) {
    except = e;
  }
  do_check_true(!!except);
}
