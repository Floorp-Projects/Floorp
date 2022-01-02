/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const gfxFrameScript = {
  domUtils: null,

  init() {
    let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
    let webProgress = docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(
      this,
      Ci.nsIWebProgress.NOTIFY_STATE_WINDOW
    );

    this.domUtils = content.windowUtils;

    let loadURIOptions = {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    };
    webNav.loadURI(
      "chrome://gfxsanity/content/sanitytest.html",
      loadURIOptions
    );
  },

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "MozAfterPaint":
        sendAsyncMessage("gfxSanity:ContentLoaded");
        removeEventListener("MozAfterPaint", this);
        break;
    }
  },

  isSanityTest(aUri) {
    if (!aUri) {
      return false;
    }

    return aUri.endsWith("/sanitytest.html");
  },

  onStateChange(webProgress, req, flags, status) {
    if (
      webProgress.isTopLevel &&
      flags & Ci.nsIWebProgressListener.STATE_STOP &&
      this.isSanityTest(req.name)
    ) {
      webProgress.removeProgressListener(this);

      // If no paint is pending, then the test already painted
      if (this.domUtils.isMozAfterPaintPending) {
        addEventListener("MozAfterPaint", this);
      } else {
        sendAsyncMessage("gfxSanity:ContentLoaded");
      }
    }
  },

  // Needed to support web progress listener
  QueryInterface: ChromeUtils.generateQI([
    "nsIWebProgressListener",
    "nsISupportsWeakReference",
    "nsIObserver",
  ]),
};

gfxFrameScript.init();
