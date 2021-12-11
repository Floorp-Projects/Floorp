// META: script=resources/throttling.js

setup(() => waitForLoad()
  .then(() => "setup done"));

promise_test(t => addRTCPeerConnection(t)
  .then(() => busy(100))
  .then(() => getThrottlingRate(100))
  .then(rate => {
    assert_less_than(rate, 10, "Timeout was throttled");
  }), "Don't throttle when there are open RTCPeerConnections.");

promise_test(t => inFrame(t)
  .then(win => win.addRTCPeerConnection(t))
  .then(() => busy(100))
  .then(() => getThrottlingRate(100))
  .then(rate => {
    assert_less_than(rate, 10, "Timeout was throttled");
  }), "Don't throttle when there are open RTCPeerConnections in iframe.");

promise_test(t => inFrame(t)
  .then(win => addRTCPeerConnection(t)
    .then(() => win.busy(100))
    .then(() => win.getThrottlingRate(100)))
  .then(rate => {
    assert_less_than(rate, 10, "Timeout was throttled");
  }), "Don't throttle iframe when there are open RTCPeerConnections in parent.");

promise_test(t => inFrame(t)
  .then(win => win.addRTCPeerConnection(t)
                  .then(() => win.busy(100))
                  .then(() => win.getThrottlingRate(100)))
  .then(rate => {
    assert_less_than(rate, 10, "Timeout was throttled");
  }), "Don't throttle iframe when there are open RTCPeerConnections in iframe.");
