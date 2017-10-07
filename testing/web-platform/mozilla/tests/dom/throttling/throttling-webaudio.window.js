// META: script=resources/throttling.js

setup(() => waitForLoad()
  .then(() => "setup done"));

promise_test(t => addWebAudio(t)
  .then(() => busy(100))
  .then(() => getThrottlingRate(100))
  .then(rate => {
    assert_less_than(rate, 10, "Timeout was throttled");
  }), "Don't throttle when there is active WebAudio.");

promise_test(t => inFrame(t)
  .then(win => win.addWebAudio(t))
  .then(() => busy(100))
  .then(() => getThrottlingRate(100))
  .then(rate => {
    assert_less_than(rate, 10, "Timeout was throttled");
  }), "Don't throttle when there is active WebAudio in iframe.");

promise_test(t => inFrame(t)
  .then(win => addWebAudio(t)
    .then(() => win.busy(100))
    .then(() => win.getThrottlingRate(100)))
  .then(rate => {
    assert_less_than(rate, 10, "Timeout was throttled");
  }), "Don't throttle iframe when there is active WebAudio in parent.");

promise_test(t => inFrame(t)
  .then(win => win.addWebAudio(t)
                  .then(() => win.busy(100))
                  .then(() => win.getThrottlingRate(100)))
  .then(rate => {
    assert_less_than(rate, 10, "Timeout was throttled");
  }), "Don't throttle iframe when there is active WebAudio in iframe.");
