// META: script=resources/throttling.js

setup(() => waitForLoad()
  .then(() => "setup done"));

promise_test(t => busy(100)
  .then(() => getThrottlingRate(100))
  .then(rate => {
    assert_greater_than(rate, 10, "Timeout wasn't throttled");
  }), "Throttle when all budget has been used.");
