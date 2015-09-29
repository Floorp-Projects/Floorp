(function() {
  const PREFIX = "damp@mozilla.org:";

  addEventListener(PREFIX + "chrome-exec-event", function (e) {
    if (content.document.documentURI.indexOf("chrome://damp/content/damp.html")) {
      // Can have url fragment. Backward compatible version of !str.startsWidth("prefix")
      throw new Error("Cannot be used outside of DAMP's launch page");
    }

    var uniqueMessageId = PREFIX + content.document.documentURI + Date.now() + Math.random();

    addMessageListener(PREFIX + "chrome-exec-reply", function done(reply) {
      if (reply.data.id == uniqueMessageId) {
        removeMessageListener(PREFIX + "chrome-exec-reply", done);
        e.detail.doneCallback(reply.data.result);
      }
    });

    sendAsyncMessage(PREFIX + "chrome-exec-message", {
      command: e.detail.command,
      id: uniqueMessageId
    });
  }, false);
})();
