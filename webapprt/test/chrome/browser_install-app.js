Cu.import("resource://gre/modules/Services.jsm");
let { WebappOSUtils } = Cu.import("resource://gre/modules/WebappOSUtils.jsm", {});

let url = "http://test/webapprtChrome/webapprt/test/chrome/sample.webapp";

function test() {
  waitForExplicitFinish();

  loadWebapp("install-app.webapp", undefined, function onLoad() {

    let dialogShown = false;

    let winObserver = function(win, topic) {
      if (topic == "domwindowopened") {
        win.addEventListener("load", function onLoadWindow() {
          win.removeEventListener("load", onLoadWindow, false);

          if (win.document.documentURI == "chrome://global/content/commonDialog.xul") {
            dialogShown = true;

            executeSoon(() => {
              win.document.documentElement.acceptDialog();
            });
          }
        }, false);
      }
    }

    Services.ww.registerNotification(winObserver);

    registerCleanupFunction(function() {
      Services.ww.unregisterNotification(winObserver);
    });

    let request = navigator.mozApps.install(url);
    request.onsuccess = function() {
      ok(dialogShown, "Install app dialog shown");
      ok(request.result, "App installed");

      navigator.mozApps.mgmt.uninstall(request.result).onsuccess = function() {
        finish();
      }
    }
    request.onerror = function() {
      ok(false, "Not installed: " + request.error.name);
      finish();
    }
  });
}
