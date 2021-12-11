// META: script=resources/throttling.js

setup(() => waitForLoad()
  .then(() => "setup done"));

promise_test(t => inFrame(t)
  .then(win => win.busy(100)
                  .then(() => win.getThrottlingRate(100)))
  .then(rate => {
    assert_greater_than(rate, 10, "Timeout wasn't throttled");
  }), "Throttle iframe when all budget has been used");
