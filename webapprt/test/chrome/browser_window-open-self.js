Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://webapprt/modules/WebappRT.jsm");
let { DOMApplicationRegistry } =
  Cu.import("resource://gre/modules/Webapps.jsm", {});

function test() {
  waitForExplicitFinish();

  let appID = Ci.nsIScriptSecurityManager.NO_APP_ID;

  let progressListener = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                           Ci.nsISupportsWeakReference]),
    onLocationChange: function(progress, request, location, flags) {
      gAppBrowser.addEventListener("load", function onLoad() {
        gAppBrowser.removeEventListener("load", onLoad, true);

        is(DOMApplicationRegistry.getAppLocalIdByManifestURL(WebappRT.config.app.manifestURL),
           appID,
           "Principal app ID hasn't changed");

        finish();
      }, true);
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

  loadWebapp("window-open-self.webapp", undefined, function() {
    appID = gAppBrowser.contentDocument.defaultView.document.nodePrincipal.appId;

    is(DOMApplicationRegistry.getAppLocalIdByManifestURL(WebappRT.config.app.manifestURL),
       appID,
       "Principal app ID correct");

    gAppBrowser.addProgressListener(progressListener,
                                    Ci.nsIWebProgress.NOTIFY_LOCATION);
  });

  registerCleanupFunction(function() {
    Services.ww.unregisterNotification(winObserver);
    gAppBrowser.removeProgressListener(progressListener);
  });
}
