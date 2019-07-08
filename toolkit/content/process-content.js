/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Creates a new PageListener for this process. This will listen for page loads
// and for those that match URLs provided by the parent process will set up
// a dedicated message port and notify the parent process.
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const gInContentProcess =
  Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT;

Services.cpmm.addMessageListener("gmp-plugin-crash", msg => {
  let gmpservice = Cc["@mozilla.org/gecko-media-plugin-service;1"].getService(
    Ci.mozIGeckoMediaPluginService
  );

  gmpservice.RunPluginCrashCallbacks(msg.data.pluginID, msg.data.pluginName);
});

let TOPICS = [
  "chrome-document-global-created",
  "content-document-global-created",
];

if (gInContentProcess) {
  TOPICS.push("inner-window-destroyed");
  TOPICS.push("xpcom-shutdown");
}

let ProcessObserver = {
  init() {
    for (let topic of TOPICS) {
      Services.obs.addObserver(this, topic);
    }
  },

  uninit() {
    for (let topic of TOPICS) {
      Services.obs.removeObserver(this, topic);
    }
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "chrome-document-global-created":
      case "content-document-global-created": {
        // Strip the hash from the URL, because it's not part of the origin.
        let window = subject;
        let url = window.document.documentURI.replace(/[\#|\?].*$/, "");

        let registeredURLs = Services.cpmm.sharedData.get(
          "RemotePageManager:urls"
        );

        if (!registeredURLs || !registeredURLs.has(url)) {
          return;
        }

        // Get the frame message manager for this window so we can associate this
        // page with a browser element
        let messageManager = window.docShell.messageManager;

        let { ChildMessagePort } = ChromeUtils.import(
          "resource://gre/modules/remotepagemanager/RemotePageManagerChild.jsm"
        );
        // Set up the child side of the message port
        new ChildMessagePort(messageManager, window);
        break;
      }
      case "inner-window-destroyed": {
        // Forward inner-window-destroyed notifications with the
        // inner window ID, so that code in the parent that should
        // do something when content windows go away can do it
        let innerWindowID = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
        Services.cpmm.sendAsyncMessage(
          "Toolkit:inner-window-destroyed",
          innerWindowID
        );
        break;
      }
      case "xpcom-shutdown": {
        this.uninit();
        break;
      }
    }
  },
};

ProcessObserver.init();
