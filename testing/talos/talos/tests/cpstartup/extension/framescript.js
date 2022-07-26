/* eslint-env mozilla/frame-script */

(function() {
  sendAsyncMessage("CPStartup:BrowserChildReady", {
    time: Services.telemetry.msSystemNow(),
  });

  addEventListener(
    "CPStartup:Ping",
    e => {
      let evt = new content.CustomEvent("CPStartup:Pong", { bubbles: true });
      content.dispatchEvent(evt);
    },
    false,
    true
  );

  addEventListener(
    "CPStartup:Go",
    e => {
      sendAsyncMessage("CPStartup:Go", e.detail);
    },
    false,
    true
  );

  addMessageListener("CPStartup:FinalResults", msg => {
    let evt = Cu.cloneInto(
      {
        bubbles: true,
        detail: msg.data,
      },
      content
    );

    content.dispatchEvent(
      new content.CustomEvent("CPStartup:FinalResults", evt)
    );
  });
})();
