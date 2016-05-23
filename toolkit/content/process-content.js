/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

// Creates a new PageListener for this process. This will listen for page loads
// and for those that match URLs provided by the parent process will set up
// a dedicated message port and notify the parent process.
Cu.import("resource://gre/modules/RemotePageManager.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const gInContentProcess = Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT;

Services.cpmm.addMessageListener("gmp-plugin-crash", msg => {
  let gmpservice = Cc["@mozilla.org/gecko-media-plugin-service;1"]
                     .getService(Ci.mozIGeckoMediaPluginService);

  gmpservice.RunPluginCrashCallbacks(msg.data.pluginID, msg.data.pluginName);
});

// Forward inner-window-destroyed notifications with the inner window ID,
// so that code in the parent that should do something when content
// windows go away can do it
if (gInContentProcess) {
  Services.obs.addObserver((subject, topic, data) => {
    let innerWindowID = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
    Services.cpmm.sendAsyncMessage("Toolkit:inner-window-destroyed", innerWindowID);
  }, "inner-window-destroyed", false);
}
