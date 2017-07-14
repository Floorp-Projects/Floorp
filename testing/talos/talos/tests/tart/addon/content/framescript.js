(function() {
  const TART_PREFIX = "tart@mozilla.org:";

  addEventListener(TART_PREFIX + "chrome-exec-event", function(e) {
    if (content.document.documentURI.indexOf("chrome://tart/content/tart.html")) {
      // Can have url fragment. Backward compatible version of !str.startsWidth("prefix")
      throw new Error("Cannot be used outside of TART's launch page");
    }

    // eslint-disable-next-line mozilla/avoid-Date-timing
    var uniqueMessageId = TART_PREFIX + content.document.documentURI + Date.now() + Math.random();

    addMessageListener(TART_PREFIX + "chrome-exec-reply", function done(reply) {
      if (reply.data.id == uniqueMessageId) {
        removeMessageListener(TART_PREFIX + "chrome-exec-reply", done);
        e.detail.doneCallback(reply.data.result);
      }
    });

    sendAsyncMessage(TART_PREFIX + "chrome-exec-message", {
      command: e.detail.command,
      id: uniqueMessageId
    });
  }, false);
})()
