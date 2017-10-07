// META: script=resources/throttling.js

setup(() => waitForLoad()
  .then(() => "setup done"));

promise_test(t => inFrame(t)
  .then(win => win.busy(100))
  .then(() => getThrottlingRate(100))
  .then(rate => {
    assert_less_than(rate, 10, "Timeout was throttled");
  }), "Don't throttle parent when all budget in iframe has been used");
