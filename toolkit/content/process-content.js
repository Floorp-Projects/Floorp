/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Creates a new PageListener for this process. This will listen for page loads
// and for those that match URLs provided by the parent process will set up
// a dedicated message port and notify the parent process.
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

Services.cpmm.addMessageListener("gmp-plugin-crash", ({ data }) => {
  Cc["@mozilla.org/gecko-media-plugin-service;1"]
    .getService(Ci.mozIGeckoMediaPluginService)
    .RunPluginCrashCallbacks(data.pluginID, data.pluginName);
});

let ProcessObserver = {
  init() {
    Services.obs.addObserver(this, "chrome-document-global-created");
    Services.obs.addObserver(this, "content-document-global-created");
    if (Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT) {
      Services.obs.addObserver(this, "inner-window-destroyed");
    }
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "chrome-document-global-created":
      case "content-document-global-created": {
        // Strip the hash from the URL, because it's not part of the origin.
        let window = subject;
        let url = window.document.documentURI.replace(/[#?].*$/, "");

        let registeredURLs = Services.cpmm.sharedData.get(
          "RemotePageManager:urls"
        );
        if (registeredURLs && registeredURLs.has(url)) {
          let { ChildMessagePort } = ChromeUtils.import(
            "resource://gre/modules/remotepagemanager/RemotePageManagerChild.jsm"
          );
          // Set up the child side of the message port
          new ChildMessagePort(window);
        }
        break;
      }
      case "inner-window-destroyed":
        // Forward inner-window-destroyed notifications with the
        // inner window ID, so that code in the parent that should
        // do something when content windows go away can do it
        Services.cpmm.sendAsyncMessage(
          "Toolkit:inner-window-destroyed",
          subject.nsISupportsPRUint64.data
        );
        break;
    }
  },
};

ProcessObserver.init();
// Drop the reference so it can be GCed.
ProcessObserver.init = null;
