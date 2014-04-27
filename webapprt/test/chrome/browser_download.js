Cu.import("resource://gre/modules/Services.jsm");

let winObserver = function(win, topic) {
  if (topic == "domwindowopened") {
    win.addEventListener("load", function onLoadWindow() {
      win.removeEventListener("load", onLoadWindow, false);

      if (win.document.documentURI ==
          "chrome://mozapps/content/downloads/unknownContentType.xul") {
        ok(true, "Download dialog shown");

        setTimeout(() => {
          let button = win.document.documentElement.getButton("accept");
          button.disabled = false;
          win.document.documentElement.acceptDialog();
        }, 0);
      }
    }, false);
  }
}

Services.ww.registerNotification(winObserver);

let MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);
MockFilePicker.useAnyFile();
MockFilePicker.showCallback = function() {
  ok(true, "File picker shown");
  return MockFilePicker.returnOK;
}

let downloadListener = {
  onDownloadStateChange: function(aState, aDownload) {
    if (aDownload.state == Services.downloads.DOWNLOAD_FINISHED) {
      ok(aDownload.targetFile.exists(), "Download completed");
      is(aDownload.targetFile.fileSize, 154, "Downloaded file has correct size");

      finish();
    }
  },
};

Services.downloads.addListener(downloadListener);

registerCleanupFunction(function() {
  Services.wm.getMostRecentWindow("Download:Manager").close();
  Services.ww.unregisterNotification(winObserver);
  MockFilePicker.cleanup();
  Services.downloads.removeListener(downloadListener);
});

function test() {
  waitForExplicitFinish();

  loadWebapp("download.webapp", undefined, function onLoad() {
    gAppBrowser.contentDocument.getElementById("download").click();
  });
}
