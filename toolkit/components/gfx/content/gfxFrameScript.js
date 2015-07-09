const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const gfxFrameScript = {
  domUtils: null,

  init: function() {
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

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "MozAfterPaint":
        sendAsyncMessage('gfxSanity:ContentLoaded');
        removeEventListener("MozAfterPaint", this);
        break;
    }
  },

  isSanityTest: function(aUri) {
    if (!aUri) {
      return false;
    }

    return aUri.endsWith("/sanitytest.html");
  },

  onStateChange: function (webProgress, req, flags, status) {
    if (webProgress.isTopLevel &&
        (flags & Ci.nsIWebProgressListener.STATE_STOP) &&
        this.isSanityTest(req.name)) {

      // If no paint is pending, then the test already painted
      if (this.domUtils.isMozAfterPaintPending) {
        addEventListener("MozAfterPaint", this);
      } else {
        sendAsyncMessage('gfxSanity:ContentLoaded');
      }
    }
  },

  // Needed to support web progress listener
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIWebProgressListener,
    Ci.nsISupportsWeakReference,
    Ci.nsIObserver,
  ]),
};

gfxFrameScript.init();
