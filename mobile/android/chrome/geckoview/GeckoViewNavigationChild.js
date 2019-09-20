/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { GeckoViewChildModule } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewChildModule.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ErrorPageEventHandler: "chrome://geckoview/content/ErrorPageEventHandler.js",
});

// TODO: only needed in Fennec
class GeckoViewNavigationChild extends GeckoViewChildModule {
  onInit() {
    if (Services.androidBridge.isFennec) {
      addEventListener("DOMContentLoaded", this);
    }
  }

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "DOMContentLoaded": {
        // TODO: Remove this when we have a better story re: interactive error pages.
        let target = aEvent.originalTarget;

        // ignore on frames and other documents
        if (target != content.document) {
          return;
        }

        let docURI = target.documentURI;

        if (
          docURI.startsWith("about:certerror") ||
          docURI.startsWith("about:blocked")
        ) {
          addEventListener("click", ErrorPageEventHandler, true);
          let listener = () => {
            removeEventListener("click", ErrorPageEventHandler, true);
            removeEventListener("pagehide", listener, true);
          };

          addEventListener("pagehide", listener, true);
        }

        break;
      }
    }
  }
}

const { debug, warn } = GeckoViewNavigationChild.initLogging(
  "GeckoViewNavigation"
); // eslint-disable-line no-unused-vars
const module = GeckoViewNavigationChild.create(this);
