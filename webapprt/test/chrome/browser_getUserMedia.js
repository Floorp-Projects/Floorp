Cu.import("resource://gre/modules/Services.jsm");

function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref("media.navigator.permission.fake", true);

  let getUserMediaDialogOpened = false;

  let winObserver = function(win, topic) {
    if (topic == "domwindowopened") {
      win.addEventListener("load", function onLoadWindow() {
        win.removeEventListener("load", onLoadWindow, false);

        if (win.document.documentURI == "chrome://webapprt/content/getUserMediaDialog.xul") {
          getUserMediaDialogOpened = true;
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
      ok(getUserMediaDialogOpened, "Prompt shown.");
      finish();
    });
    mutObserver.observe(msg, { childList: true });
  });

  registerCleanupFunction(function() {
    Services.ww.unregisterNotification(winObserver);
    mutObserver.disconnect();
    Services.prefs.clearUserPref("media.navigator.permission.fake");
  });
}
