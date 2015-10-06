/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { interfaces: Ci } = Components;

/**
 * Content that wants to quit the whole session should
 * fire the TalosQuitApplication custom event. This will
 * attempt to force-quit the browser.
 */
addEventListener("TalosQuitApplication", () => {
  // If we're loaded in a low-priority background process, like
  // the background page thumbnailer, then we shouldn't be allowed
  // to quit the whole application. This is a workaround until
  // bug 1164459 is fixed.
  let priority = docShell.QueryInterface(Ci.nsIDocumentLoader)
                         .loadGroup
                         .QueryInterface(Ci.nsISupportsPriority)
                         .priority;
  if (priority != Ci.nsISupportsPriority.PRIORITY_LOWEST) {
    sendAsyncMessage("Talos:ForceQuit");
  }
});
