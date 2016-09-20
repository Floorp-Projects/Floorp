/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");

let observer = {
  observe(aWindow) {
    let utils = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);
    let outerID = utils.outerWindowID;
    let innerID = utils.currentInnerWindowID;

    // NB: We don't have to distinguish between whether this is for a chrome
    // or content document.
    sendAsyncMessage("browser-test:documentCreated",
                     { location: aWindow.location.href,
                       outerID, innerID });
  },
};

addMessageListener("browser-test:removeObservers", (msg) => {
  Services.obs.removeObserver(observer, "chrome-document-global-created");
  Services.obs.removeObserver(observer, "content-document-global-created");
});

Services.obs.addObserver(observer, "chrome-document-global-created", false);
Services.obs.addObserver(observer, "content-document-global-created", false);
