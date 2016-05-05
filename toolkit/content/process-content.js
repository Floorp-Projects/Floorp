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

Services.cpmm.addMessageListener("gmp-plugin-crash", msg => {
  let gmpservice = Cc["@mozilla.org/gecko-media-plugin-service;1"]
                     .getService(Ci.mozIGeckoMediaPluginService);

  gmpservice.RunPluginCrashCallbacks(msg.data.pluginID, msg.data.pluginName);
});
