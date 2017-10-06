// META: script=resources/throttling.js

setup(() => waitForLoad()
  .then(() => "setup done"));

promise_test(t => addIndexedDB(t)
  .then(() => busy(100))
  .then(() => getThrottlingRate(100))
  .then(rate => {
    assert_less_than(rate, 10, "Timeout was throttled");
  }), "Don't throttle when there are open IndexedDB transactions.");

promise_test(t => inFrame(t)
  .then(win => win.addIndexedDB(t))
  .then(() => busy(100))
  .then(() => getThrottlingRate(100))
  .then(rate => {
    assert_less_than(rate, 10, "Timeout was throttled");
  }), "Don't throttle when there are open IndexedDB transactions in iframe.");

promise_test(t => inFrame(t)
  .then(win => addIndexedDB(t)
    .then(() => win.busy(100))
    .then(() => win.getThrottlingRate(100)))
  .then(rate => {
    assert_less_than(rate, 10, "Timeout was throttled");
  }), "Don't throttle iframe when there are open IndexedDB transactions in parent.");

promise_test(t => inFrame(t)
  .then(win => win.addIndexedDB(t)
                  .then(() => win.busy(100))
                  .then(() => win.getThrottlingRate(100)))
  .then(rate => {
    assert_less_than(rate, 10, "Timeout was throttled");
  }), "Don't throttle iframe when there are open IndexedDB transactions in iframe.");
