/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded as a framescript
/* global docShell */
// eslint-env mozilla/frame-script

ChromeUtils.import("resource://gre/modules/Services.jsm");

addEventListener("TalosContentProfilerCommand", (e) => {
  let name = e.detail.name;
  let data = e.detail.data;
  sendAsyncMessage("TalosContentProfiler:Command", { name, data });
});

addMessageListener("TalosContentProfiler:Response", (msg) => {
  let name = msg.data.name;
  let data = msg.data.data;

  let event = Cu.cloneInto({
    bubbles: true,
    detail: {
      name,
      data,
    },
  }, content);
  content.dispatchEvent(
    new content.CustomEvent("TalosContentProfilerResponse", event));
});

addEventListener("TalosPowersContentForceCCAndGC", (e) => {
  Cu.forceGC();
  Cu.forceCC();
  Cu.forceShrinkingGC();
  sendSyncMessage("TalosPowersContent:ForceCCAndGC");
});

addEventListener("TalosPowersContentFocus", (e) => {
  if (content.location.protocol != "file:" &&
      content.location.hostname != "localhost" &&
      content.location.hostname != "127.0.0.1") {
    throw new Error("TalosPowersContentFocus may only be used with local content");
  }
  content.focus();
  let contentEvent = Cu.cloneInto({
    bubbles: true,
  }, content);
  content.dispatchEvent(new content.CustomEvent("TalosPowersContentFocused", contentEvent));
}, true, true);

addEventListener("TalosPowersContentGetStartupInfo", (e) => {
  sendAsyncMessage("TalosPowersContent:GetStartupInfo");
  addMessageListener("TalosPowersContent:GetStartupInfo:Result",
                     function onResult(msg) {
    removeMessageListener("TalosPowersContent:GetStartupInfo:Result",
                          onResult);
    let event = Cu.cloneInto({
      bubbles: true,
      detail: msg.data,
    }, content);

    content.dispatchEvent(
      new content.CustomEvent("TalosPowersContentGetStartupInfoResult",
                              event));
  });
});

/**
 * Content that wants to quit the whole session should
 * fire the TalosPowersGoQuitApplication custom event. This will
 * attempt to force-quit the browser.
 */
addEventListener("TalosPowersGoQuitApplication", (e) => {
  // If we're loaded in a low-priority background process, like
  // the background page thumbnailer, then we shouldn't be allowed
  // to quit the whole application. This is a workaround until
  // bug 1164459 is fixed.
  let priority = docShell.QueryInterface(Ci.nsIDocumentLoader)
                         .loadGroup
                         .QueryInterface(Ci.nsISupportsPriority)
                         .priority;
  if (priority != Ci.nsISupportsPriority.PRIORITY_LOWEST) {
    sendAsyncMessage("Talos:ForceQuit", e.detail);
  }
});

/* *
 * Mediator for the generic ParentExec mechanism.
 * Listens for a query event from the content, forwards it as a query message
 * to the parent, listens to a parent reply message, and forwards it as a reply
 * event for the content to capture.
 * The consumer API for this mechanism is at content/TalosPowersContent.js
 * and the callees are at ParentExecServices at components/TalosPowersService.js
 */
addEventListener("TalosPowers:ParentExec:QueryEvent", function(e) {
  if (content.location.protocol != "file:" &&
      content.location.hostname != "localhost" &&
      content.location.hostname != "127.0.0.1") {
    throw new Error("TalosPowers:ParentExec may only be used with local content");
  }
  let uniqueMessageId = "TalosPowers:ParentExec:"
                      + content.document.documentURI + Date.now() + Math.random(); // eslint-disable-line mozilla/avoid-Date-timing

  // Listener for the reply from the parent process
  addMessageListener("TalosPowers:ParentExec:ReplyMsg", function done(reply) {
    if (reply.data.id != uniqueMessageId)
      return;

    removeMessageListener("TalosPowers:ParentExec:ReplyMsg", done);

    // reply to content via an event
    let contentEvent = Cu.cloneInto({
      bubbles: true,
      detail: reply.data.result
    }, content);
    content.dispatchEvent(new content.CustomEvent(e.detail.listeningTo, contentEvent));
  });

  // Send the query to the parent process
  sendAsyncMessage("TalosPowers:ParentExec:QueryMsg", {
    command: e.detail.command,
    id: uniqueMessageId
  });
}, false, true); // wantsUntrusted since we're exposing to unprivileged
