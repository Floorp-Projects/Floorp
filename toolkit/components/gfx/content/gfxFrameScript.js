/* eslint-env mozilla/frame-script */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const gfxFrameScript = {
  domUtils: null,

  init() {
    let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
    let webProgress =  docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_STATE_WINDOW);

    this.domUtils = content.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIDOMWindowUtils);

    webNav.loadURI("chrome://gfxsanity/content/sanitytest.html",
                   Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
                   null, null, null);

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
    if (webProgress.isTopLevel &&
        (flags & Ci.nsIWebProgressListener.STATE_STOP) &&
        this.isSanityTest(req.name)) {

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
    Ci.nsIWebProgressListener,
    Ci.nsISupportsWeakReference,
    Ci.nsIObserver,
  ]),
};

gfxFrameScript.init();
