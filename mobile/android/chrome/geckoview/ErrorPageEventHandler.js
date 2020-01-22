/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);
const { PrivateBrowsingUtils } = ChromeUtils.import(
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { debug, warn } = GeckoViewUtils.initLogging("ErrorPageEventHandler"); // eslint-disable-line no-unused-vars

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
});

var EXPORTED_SYMBOLS = ["ErrorPageEventHandler"];

var ErrorPageEventHandler = {
  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "click": {
        // Don't trust synthetic events
        if (!aEvent.isTrusted) {
          return;
        }

        let target = aEvent.originalTarget;
        let errorDoc = target.ownerDocument;

        // If the event came from an ssl error page, it is probably either the "Add
        // Exception…" or "Get me out of here!" button
        if (errorDoc.documentURI.startsWith("about:certerror?e=nssBadCert")) {
          let perm = errorDoc.getElementById("permanentExceptionButton");
          let temp = errorDoc.getElementById("temporaryExceptionButton");
          if (target == temp || target == perm) {
            // Handle setting an cert exception and reloading the page
            let uri = Services.io.newURI(errorDoc.location.href);
            let docShell = BrowserApp.selectedBrowser.docShell;
            let securityInfo = docShell.failedChannel.securityInfo;
            securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
            let cert = securityInfo.serverCert;
            let overrideService = Cc[
              "@mozilla.org/security/certoverride;1"
            ].getService(Ci.nsICertOverrideService);
            let flags = 0;
            if (securityInfo.isUntrusted) {
              flags |= overrideService.ERROR_UNTRUSTED;
            }
            if (securityInfo.isDomainMismatch) {
              flags |= overrideService.ERROR_MISMATCH;
            }
            if (securityInfo.isNotValidAtThisTime) {
              flags |= overrideService.ERROR_TIME;
            }
            let temporary =
              target == temp ||
              PrivateBrowsingUtils.isWindowPrivate(errorDoc.defaultView);
            overrideService.rememberValidityOverride(
              uri.asciiHost,
              uri.port,
              cert,
              flags,
              temporary
            );
            errorDoc.location.reload();
          } else if (
            target == errorDoc.getElementById("getMeOutOfHereButton")
          ) {
            errorDoc.location = "about:home";
          }
        }
        break;
      }
    }
  },
};
