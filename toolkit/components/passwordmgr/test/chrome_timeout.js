const Cc = Components.classes;
const Ci = Components.interfaces;

addMessageListener('setTimeout', msg => {
  let timer = Cc['@mozilla.org/timer;1'].createInstance(Ci.nsITimer);
  timer.init(_ => {
    sendAsyncMessage('timeout');
  }, msg.delay, Ci.nsITimer.TYPE_ONE_SHOT);
});

sendAsyncMessage('ready');
