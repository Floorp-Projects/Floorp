// META: script=resources/throttling.js

setup(() => waitForLoad()
  .then(() => "setup done"));

promise_test(t => inFrame(t)
  .then(win => busy(100)
    .then(() => win.getThrottlingRate(100)))
  .then(rate => {
    assert_less_than(rate, 10, "Timeout was throttled");
  }), "Don't throttle iframe when all budget in parent has been used");
