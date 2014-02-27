Cu.import("resource://gre/modules/XPCOMUtils.jsm");

let HandlerService = {
  classID: Components.ID("{b4ed9fab-fd39-435a-8e3e-edc3e689e72e}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFactory,
                                         Ci.nsIExternalProtocolService]),

  createInstance: function(aOuter, aIID) {
    if (aOuter) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }

    return this.QueryInterface(aIID);
  },

  init: function() {
    Components.manager.nsIComponentRegistrar.registerFactory(this.classID,
      "Test Protocol Handler Service",
      "@mozilla.org/uriloader/external-protocol-service;1",
      this);
  },

  getProtocolHandlerInfo: function(aProtocolScheme) {
    let handlerInfoObj = {
      launchWithURI: function(aURI) {
        is(aURI.spec,
           "http://test/webapprtChrome/webapprt/test/chrome/sample.html",
           "The app tried to open the link in the default browser");

        finish();
      }
    };

    return handlerInfoObj;
  }
};

HandlerService.init();

function test() {
  waitForExplicitFinish();

  let progressListener = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                           Ci.nsISupportsWeakReference]),
    onLocationChange: function(progress, request, location, flags) {
      ok(false, "Location changed");
      finish();
    }
  };

  let winObserver = function(win, topic) {
    if (topic == "domwindowopened") {
      win.addEventListener("load", function onLoadWindow() {
        win.removeEventListener("load", onLoadWindow, false);

        if (win.location == "chrome://webapprt/content/webapp.xul") {
          ok(false, "New app window opened");
          finish();
        }
      }, false);
    }
  }

  loadWebapp("window-open-blank.webapp", undefined, function() {
    gAppBrowser.addProgressListener(progressListener,
                                    Ci.nsIWebProgress.NOTIFY_LOCATION);
  });

  registerCleanupFunction(function() {
    Services.ww.unregisterNotification(winObserver);
    gAppBrowser.removeProgressListener(progressListener);
  });
}
