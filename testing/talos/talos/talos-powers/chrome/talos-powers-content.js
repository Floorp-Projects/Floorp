/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

addEventListener(
  "TalosContentProfilerCommand",
  e => {
    let name = e.detail.name;
    let data = e.detail.data;
    sendAsyncMessage("TalosContentProfiler:Command", { name, data });
  },
  { wantUntrusted: true } // since we're exposing to unprivileged
);

addMessageListener("TalosContentProfiler:Response", msg => {
  let name = msg.data.name;
  let data = msg.data.data;

  let event = Cu.cloneInto(
    {
      bubbles: true,
      detail: {
        name,
        data,
      },
    },
    content
  );
  content.dispatchEvent(
    new content.CustomEvent("TalosContentProfilerResponse", event)
  );
});

addEventListener(
  "TalosPowersContentForceCCAndGC",
  e => {
    Cu.forceGC();
    Cu.forceCC();
    Cu.forceShrinkingGC();
    sendSyncMessage("TalosPowersContent:ForceCCAndGC");
  },
  { wantUntrusted: true } // since we're exposing to unprivileged
);

addEventListener(
  "TalosPowersContentFocus",
  e => {
    if (
      content.location.protocol != "file:" &&
      content.location.hostname != "localhost" &&
      content.location.hostname != "127.0.0.1"
    ) {
      throw new Error(
        "TalosPowersContentFocus may only be used with local content"
      );
    }
    content.focus();
    let contentEvent = Cu.cloneInto(
      {
        bubbles: true,
      },
      content
    );
    content.dispatchEvent(
      new content.CustomEvent("TalosPowersContentFocused", contentEvent)
    );
  },
  { capture: true, wantUntrusted: true } // since we're exposing to unprivileged
);

addEventListener(
  "TalosPowersContentGetStartupInfo",
  e => {
    sendAsyncMessage("TalosPowersContent:GetStartupInfo");
    addMessageListener(
      "TalosPowersContent:GetStartupInfo:Result",
      function onResult(msg) {
        removeMessageListener(
          "TalosPowersContent:GetStartupInfo:Result",
          onResult
        );
        let event = Cu.cloneInto(
          {
            bubbles: true,
            detail: msg.data,
          },
          content
        );

        content.dispatchEvent(
          new content.CustomEvent(
            "TalosPowersContentGetStartupInfoResult",
            event
          )
        );
      }
    );
  },
  { wantUntrusted: true } // since we're exposing to unprivileged
);

addEventListener(
  "TalosPowersContentDumpConsole",
  e => {
    var messages;
    try {
      messages = Services.console.getMessageArray();
    } catch (ex) {
      dump(ex + "\n");
      messages = [];
    }

    for (var i = 0; i < messages.length; i++) {
      dump(messages[i].message + "\n");
    }
  },
  { wantUntrusted: true } // since we're exposing to unprivileged
);

/**
 * Content that wants to quit the whole session should
 * fire the TalosPowersGoQuitApplication custom event. This will
 * attempt to force-quit the browser.
 */
addEventListener(
  "TalosPowersGoQuitApplication",
  e => {
    // If we're loaded in a low-priority background process, like
    // the background page thumbnailer, then we shouldn't be allowed
    // to quit the whole application. This is a workaround until
    // bug 1164459 is fixed.
    let priority = docShell
      .QueryInterface(Ci.nsIDocumentLoader)
      .loadGroup.QueryInterface(Ci.nsISupportsPriority).priority;
    if (priority != Ci.nsISupportsPriority.PRIORITY_LOWEST) {
      sendAsyncMessage("Talos:ForceQuit", e.detail);
    }
  },
  { wantUntrusted: true } // since we're exposing to unprivileged
);

/**
 * Content that wants to trigger a WebRender capture should fire the
 * TalosPowersWebRenderCapture custom event.
 */
addEventListener(
  "TalosPowersWebRenderCapture",
  e => {
    if (content && content.windowUtils) {
      content.windowUtils.wrCapture();
    } else {
      dump("Unable to obtain DOMWindowUtils for TalosPowersWebRenderCapture\n");
    }
  },
  { wantUntrusted: true } // since we're exposing to unprivileged
);

/* *
 * Mediator for the generic ParentExec mechanism.
 * Listens for a query event from the content, forwards it as a query message
 * to the parent, listens to a parent reply message, and forwards it as a reply
 * event for the content to capture.
 * The consumer API for this mechanism is at content/TalosPowersContent.js
 * and the callees are at ParentExecServices at components/TalosPowersService.js
 */
addEventListener(
  "TalosPowers:ParentExec:QueryEvent",
  function(e) {
    if (
      content.location.protocol != "file:" &&
      content.location.hostname != "localhost" &&
      content.location.hostname != "127.0.0.1"
    ) {
      throw new Error(
        "TalosPowers:ParentExec may only be used with local content"
      );
    }
    let uniqueMessageId =
      "TalosPowers:ParentExec:" +
      content.document.documentURI +
      content.window.performance.now() +
      Math.random();

    // Listener for the reply from the parent process
    addMessageListener("TalosPowers:ParentExec:ReplyMsg", function done(reply) {
      if (reply.data.id != uniqueMessageId) {
        return;
      }

      removeMessageListener("TalosPowers:ParentExec:ReplyMsg", done);

      // reply to content via an event
      let contentEvent = Cu.cloneInto(
        {
          bubbles: true,
          detail: reply.data.result,
        },
        content
      );
      content.dispatchEvent(
        new content.CustomEvent(e.detail.listeningTo, contentEvent)
      );
    });

    // Send the query to the parent process
    sendAsyncMessage("TalosPowers:ParentExec:QueryMsg", {
      command: e.detail.command,
      id: uniqueMessageId,
    });
  },
  { wantUntrusted: true } // since we're exposing to unprivileged
);
