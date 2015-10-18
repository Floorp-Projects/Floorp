Cu.import("resource://gre/modules/Services.jsm");

var list = MockDownloadsModule();

var downloadsInList = [
  new DummyDownload("test1"),
  new DummyDownload("test2"),
];

var winObserver = function(win, topic) {
  if (topic == "domwindowopened") {
    win.addEventListener("load", function onLoadWindow() {
      win.removeEventListener("load", onLoadWindow, false);

      if (win.document.documentURI ==
          "chrome://webapprt/content/downloads/downloads.xul") {
        ok(true, "Download manager window shown");

        waitDownloadListPopulation(win).then(() => {
          test_downloadList(win, downloadsInList);
          finish();
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
    for (let download of downloadsInList) {
      list.addDownload(download);
    }
  });
}
