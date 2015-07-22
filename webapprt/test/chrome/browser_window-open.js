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
      ok(false, "Content redirected")
      finish();
    }
  };

  let winObserver = function(win, topic) {
    if (topic == "domwindowopened") {
      win.addEventListener("load", function onLoadWindow() {
        win.removeEventListener("load", onLoadWindow, false);

        if (win.location == "chrome://webapprt/content/webapp.xul") {
          let winAppBrowser = win.document.getElementById("content");
          winAppBrowser.addEventListener("load", function onLoadBrowser() {
            winAppBrowser.removeEventListener("load", onLoadBrowser, true);

            is(winAppBrowser.getAttribute("src"),
               "http://test/webapprtChrome/webapprt/test/chrome/sample.html",
               "New window browser has correct src");

            is(winAppBrowser.contentDocument.defaultView.document.nodePrincipal.appId,
               appID,
               "New window principal app ID correct");

            win.close();

            finish();
          }, true);
        }
      }, false);
    }
  }

  Services.ww.registerNotification(winObserver);

  loadWebapp("window-open.webapp", undefined, function() {
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
