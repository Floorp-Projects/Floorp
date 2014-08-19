Cu.import("resource://gre/modules/Services.jsm");

let list = MockDownloadsModule();

let download = new DummyDownload("test");
let removedDownload = new DummyDownload("removed");

let winObserver = function(win, topic) {
  if (topic == "domwindowopened") {
    win.addEventListener("load", function onLoadWindow() {
      win.removeEventListener("load", onLoadWindow, false);

      if (win.document.documentURI ==
          "chrome://webapprt/content/downloads/downloads.xul") {
        ok(true, "Download manager window shown");

        waitDownloadListPopulation(win).then(() => {
          test_downloadList(win, [download, removedDownload]);
          list.removeDownload(removedDownload);
          executeSoon(() => {
            test_downloadList(win, [download]);
            finish();
          });
        });
      }
    }, false);
  }
}

Services.ww.registerNotification(winObserver);

registerCleanupFunction(function() {
  Services.ww.unregisterNotification(winObserver);

  Services.wm.getMostRecentWindow("Download:Manager").close();
});

function test() {
  waitForExplicitFinish();

  loadWebapp("download.webapp", undefined, function onLoad() {
    list.addDownload(download);
    list.addDownload(removedDownload);
  });
}
