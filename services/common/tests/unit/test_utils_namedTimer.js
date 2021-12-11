/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_test(function test_required_args() {
  try {
    CommonUtils.namedTimer(function callback() {
      do_throw("Shouldn't fire.");
    }, 0);
    do_throw("Should have thrown!");
  } catch (ex) {
    run_next_test();
  }
});

add_test(function test_simple() {
  _("Test basic properties of CommonUtils.namedTimer.");

  const delay = 200;
  let that = {};
  let t0 = Date.now();
  CommonUtils.namedTimer(
    function callback(timer) {
      Assert.equal(this, that);
      Assert.equal(this._zetimer, null);
      Assert.ok(timer instanceof Ci.nsITimer);
      // Difference should be ~delay, but hard to predict on all platforms,
      // particularly Windows XP.
      Assert.ok(Date.now() > t0);
      run_next_test();
    },
    delay,
    that,
    "_zetimer"
  );
});

add_test(function test_delay() {
  _("Test delaying a timer that hasn't fired yet.");

  const delay = 100;
  let that = {};
  let t0 = Date.now();
  function callback(timer) {
    // Difference should be ~2*delay, but hard to predict on all platforms,
    // particularly Windows XP.
    Assert.ok(Date.now() - t0 > delay);
    run_next_test();
  }
  CommonUtils.namedTimer(callback, delay, that, "_zetimer");
  CommonUtils.namedTimer(callback, 2 * delay, that, "_zetimer");
  run_next_test();
});

add_test(function test_clear() {
  _("Test clearing a timer that hasn't fired yet.");

  const delay = 0;
  let that = {};
  CommonUtils.namedTimer(
    function callback(timer) {
      do_throw("Shouldn't fire!");
    },
    delay,
    that,
    "_zetimer"
  );

  that._zetimer.clear();
  Assert.equal(that._zetimer, null);
  CommonUtils.nextTick(run_next_test);

  run_next_test();
});
