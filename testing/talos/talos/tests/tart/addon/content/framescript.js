/* eslint-env mozilla/frame-script */

(function() {
  const TART_PREFIX = "tart@mozilla.org:";

  addEventListener(
    TART_PREFIX + "chrome-exec-event",
    function(e) {
      if (!content.location.pathname.endsWith("tart.html")) {
        Cu.reportError(
          `Ignore chrome-exec-event on non-tart page ${content.location.href}`
        );
        return;
      }

      function dispatchReply(result) {
        let contentEvent = Cu.cloneInto(
          {
            bubbles: true,
            detail: result,
          },
          content
        );
        content.dispatchEvent(
          new content.CustomEvent(e.detail.replyEvent, contentEvent)
        );
      }

      if (e.detail.command.name == "ping") {
        dispatchReply();
        return;
      }

      var uniqueMessageId =
        TART_PREFIX +
        content.document.documentURI +
        content.window.performance.now() +
        Math.random();

      addMessageListener(TART_PREFIX + "chrome-exec-reply", function done(
        reply
      ) {
        if (reply.data.id == uniqueMessageId) {
          removeMessageListener(TART_PREFIX + "chrome-exec-reply", done);
          dispatchReply(reply.data.result);
        }
      });

      sendAsyncMessage(TART_PREFIX + "chrome-exec-message", {
        command: e.detail.command,
        id: uniqueMessageId,
      });
    },
    false,
    true
  );
})();
