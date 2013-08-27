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
          ok(true, "Prompt shown.");
          win.close();
        }
      }, false);
    }
  }

  Services.ww.registerNotification(winObserver);

  let mutObserver = null;

  loadWebapp("geolocation-prompt-perm.webapp", undefined, function onLoad() {
    let principal = document.getElementById("content").contentDocument.defaultView.document.nodePrincipal;
    let permValue = Services.perms.testExactPermissionFromPrincipal(principal, "geolocation");
    is(permValue, Ci.nsIPermissionManager.PROMPT_ACTION, "Geolocation permission: prompt.");

    let msg = gAppBrowser.contentDocument.getElementById("msg");
    mutObserver = new MutationObserver(function(mutations) {
      if (msg.textContent == "Failure.") {
        ok(true, "Permission not granted.");
      } else {
        ok(false, "Permission not granted.");
      }

      if (openedWindows != 2) {
        ok(false, "Prompt not shown.");
      }

      finish();
    });
    mutObserver.observe(msg, { childList: true });
  });

  registerCleanupFunction(function() {
    Services.ww.unregisterNotification(winObserver);
    mutObserver.disconnect();
  });
}
