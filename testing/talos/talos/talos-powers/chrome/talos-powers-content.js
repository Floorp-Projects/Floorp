/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { interfaces: Ci, utils: Cu } = Components;

/**
 * Content that wants to quit the whole session should
 * fire the TalosQuitApplication custom event. This will
 * attempt to force-quit the browser.
 */
addEventListener("TalosQuitApplication", event => {
  // If we're loaded in a low-priority background process, like
  // the background page thumbnailer, then we shouldn't be allowed
  // to quit the whole application. This is a workaround until
  // bug 1164459 is fixed.
  let priority = docShell.QueryInterface(Ci.nsIDocumentLoader)
                         .loadGroup
                         .QueryInterface(Ci.nsISupportsPriority)
                         .priority;
  if (priority != Ci.nsISupportsPriority.PRIORITY_LOWEST) {
    sendAsyncMessage("Talos:ForceQuit", event.detail);
  }
});

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
      name: name,
      data: data,
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
