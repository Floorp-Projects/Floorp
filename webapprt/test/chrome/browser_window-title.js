Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://webapprt/modules/WebappRT.jsm");

function test() {
  waitForExplicitFinish();

  loadWebapp("window-title.webapp", undefined, function onLoad() {
    is(document.documentElement.getAttribute("title"),
       WebappRT.config.app.manifest.name,
       "initial window title should be webapp name");

    // Tests are triples of [URL to load, expected window title, test message].
    let tests = [
      ["http://example.com/webapprtChrome/webapprt/test/chrome/window-title.html",
       "http://example.com" + " - " + WebappRT.config.app.manifest.name,
       "window title should show origin of page at different origin"],
      ["http://test/webapprtChrome/webapprt/test/chrome/window-title.html",
       WebappRT.config.app.manifest.name,
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

    gAppBrowser.addProgressListener(progressListener,
                                    Ci.nsIWebProgress.NOTIFY_LOCATION);

    function testNext() {
      if (!tests.length) {
        gAppBrowser.removeProgressListener(progressListener);
        gAppBrowser.stop();
        finish();
        return;
      }

      [gAppBrowser.contentDocument.location, title, message] = tests.shift();
    }

    testNext();
  });
}
