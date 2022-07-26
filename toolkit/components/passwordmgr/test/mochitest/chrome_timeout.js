/* eslint-env mozilla/chrome-script */

const timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
addMessageListener("setTimeout", msg => {
  timer.init(
    _ => {
      sendAsyncMessage("timeout");
    },
    msg.delay,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
});

sendAsyncMessage("ready");
