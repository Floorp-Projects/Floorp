Cu.import("resource://gre/modules/Services.jsm");

function test() {
  waitForExplicitFinish();

  let openedWindows = 0;

  let winObserver = function(win, topic) {
    if (topic == "domwindowopened") {
      win.addEventListener("load", function onLoadWindow() {
        win.removeEventListener("load", onLoadWindow, false);
        openedWindows++;
        if (openedWindows == 2) {
          win.close();
        }
      }, false);
    }
  }

  Services.ww.registerNotification(winObserver);

  let mutObserver = null;

  loadWebapp("getUserMedia.webapp", undefined, function onLoad() {
    let msg = gAppBrowser.contentDocument.getElementById("msg");
    mutObserver = new MutationObserver(function(mutations) {
      is(msg.textContent, "PERMISSION_DENIED", "getUserMedia permission denied.");
      is(openedWindows, 2, "Prompt shown.");
      finish();
    });
    mutObserver.observe(msg, { childList: true });
  });

  registerCleanupFunction(function() {
    Services.ww.unregisterNotification(winObserver);
    mutObserver.disconnect();
  });
}
