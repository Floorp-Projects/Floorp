(function() {
  const TRESIZE_PREFIX = "tresize@mozilla.org:";

  addEventListener(TRESIZE_PREFIX + "chrome-run-event", function (e) {
    var uniqueMessageId = TRESIZE_PREFIX + content.document.documentURI + Date.now() + Math.random();

    addMessageListener(TRESIZE_PREFIX + "chrome-run-reply", function done(reply) {
      if (reply.data.id == uniqueMessageId) {
        removeMessageListener(TRESIZE_PREFIX + "chrome-run-reply", done);
        content.wrappedJSObject.logResults(reply.data.result);
      }
    });

    sendAsyncMessage(TRESIZE_PREFIX + "chrome-run-message", {
      id: uniqueMessageId,
      locationSearch: e.detail.locationSearch
    });
  }, false);
})()
