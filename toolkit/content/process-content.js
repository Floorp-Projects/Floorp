/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/process-script */

"use strict";

// Creates a new PageListener for this process. This will listen for page loads
// and for those that match URLs provided by the parent process will set up
// a dedicated message port and notify the parent process.

Services.cpmm.addMessageListener("gmp-plugin-crash", ({ data }) => {
  Cc["@mozilla.org/gecko-media-plugin-service;1"]
    .getService(Ci.mozIGeckoMediaPluginService)
    .RunPluginCrashCallbacks(data.pluginID, data.pluginName);
});
