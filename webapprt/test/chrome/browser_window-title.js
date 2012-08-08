Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function test() {
  waitForExplicitFinish();

  installWebapp("window-title.webapp", undefined,
                function onInstall(appConfig) {
    is(document.documentElement.getAttribute("title"),
       appConfig.app.manifest.name,
       "initial window title should be webapp name");

    let appBrowser = document.getElementById("content");

    // Tests are triples of [URL to load, expected window title, test message].
    let tests = [
      ["http://example.com/webapprtChrome/webapprt/test/chrome/window-title.html",
       "http://example.com" + " - " + appConfig.app.manifest.name,
       "window title should show origin of page at different origin"],
      ["http://mochi.test:8888/webapprtChrome/webapprt/test/chrome/window-title.html",
       appConfig.app.manifest.name,
       "after returning to app origin, window title should no longer show origin"],
    ];

    let title, message;

    let progressListener = {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                             Ci.nsISupportsWeakReference]),
      onLocationChange: function onLocationChange(progress, request, location,
                                                  flags) {
        // Do test in timeout to give runtime time to change title.
        window.setTimeout(function() {
          is(document.documentElement.getAttribute("title"), title, message);
          testNext();
        }, 0);
      }
    };

    appBrowser.webProgress.
      addProgressListener(progressListener, Ci.nsIWebProgress.NOTIFY_LOCATION);

    function testNext() {
      if (!tests.length) {
        appBrowser.removeProgressListener(progressListener);
        appBrowser.stop();
        finish();
        return;
      }

      [appBrowser.contentDocument.location, title, message] = tests.shift();
    }

    testNext();
  });
}
