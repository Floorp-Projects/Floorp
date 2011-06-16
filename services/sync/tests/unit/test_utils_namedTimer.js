/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/util.js");

function run_test() {
  run_next_test();
}

add_test(function test_required_args() {
  try {
    Utils.namedTimer(function callback() {
      do_throw("Shouldn't fire.");
    }, 0);
    do_throw("Should have thrown!");
  } catch(ex) {
    run_next_test();
  }
});

add_test(function test_simple() {
  _("Test basic properties of Utils.namedTimer.");

  const delay = 200;
  let that = {};
  let t0 = Date.now();
  Utils.namedTimer(function callback(timer) {
    do_check_eq(this, that);
    do_check_eq(this._zetimer, null);
    do_check_true(timer instanceof Ci.nsITimer);
    // Difference should be ~delay, but hard to predict on all platforms,
    // particularly Windows XP.
    do_check_true(Date.now() > t0);
    run_next_test();
  }, delay, that, "_zetimer");
});

add_test(function test_delay() {
  _("Test delaying a timer that hasn't fired yet.");
  
  const delay = 100;
  let that = {};
  let t0 = Date.now();
  function callback(timer) {
    // Difference should be ~2*delay, but hard to predict on all platforms,
    // particularly Windows XP.
    do_check_true((Date.now() - t0) > delay);
    run_next_test();
  }
  Utils.namedTimer(callback, delay, that, "_zetimer");
  Utils.namedTimer(callback, 2 * delay, that, "_zetimer");
  run_next_test();
});

add_test(function test_clear() {
  _("Test clearing a timer that hasn't fired yet.");

  const delay = 0;
  let that = {};
  Utils.namedTimer(function callback(timer) {
    do_throw("Shouldn't fire!");
  }, delay, that, "_zetimer");

  that._zetimer.clear();
  do_check_eq(that._zetimer, null);
  Utils.nextTick(run_next_test);

  run_next_test();
});
