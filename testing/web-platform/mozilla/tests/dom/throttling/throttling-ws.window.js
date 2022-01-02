// META: script=resources/ws.sub.js
// META: script=resources/throttling.js
let server = "ws://" + __SERVER__NAME + ":" + __PORT + "/" + __PATH;

setup(() => waitForLoad()
  .then(() => "setup done"));

promise_test(t => addWebSocket(t, server)
  .then(() => busy(100))
  .then(() => getThrottlingRate(100))
  .then(rate => {
    assert_less_than(rate, 10, "Timeout was throttled");
  }), "Don't throttle when there are open WebSockets.");

promise_test(t => inFrame(t)
  .then(win => win.addWebSocket(t, server))
  .then(() => busy(100))
  .then(() => getThrottlingRate(100))
  .then(rate => {
    assert_less_than(rate, 10, "Timeout was throttled");
  }), "Don't throttle when there are open WebSockets in iframe.");

promise_test(t => inFrame(t)
  .then(win => addWebSocket(t, server)
    .then(() => win.busy(100))
    .then(() => win.getThrottlingRate(100)))
  .then(rate => {
    assert_less_than(rate, 10, "Timeout was throttled");
  }), "Don't throttle iframe when there are open WebSockets in parent.");

promise_test(t => inFrame(t)
  .then(win => win.addWebSocket(t, server)
                  .then(() => win.busy(100))
                  .then(() => win.getThrottlingRate(100)))
  .then(rate => {
    assert_less_than(rate, 10, "Timeout was throttled");
  }), "Don't throttle iframe when there are open WebSockets in iframe.");
