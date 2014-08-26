Cu.import("resource://gre/modules/Services.jsm");
let { Downloads } = Cu.import("resource://gre/modules/Downloads.jsm", {});
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

function getFile(aFilename) {
  // The download database may contain targets stored as file URLs or native
  // paths.  This can still be true for previously stored items, even if new
  // items are stored using their file URL.  See also bug 239948 comment 12.
  if (aFilename.startsWith("file:")) {
    // Assume the file URL we obtained from the downloads database or from the
    // "spec" property of the target has the UTF-8 charset.
    let fileUrl = NetUtil.newURI(aFilename).QueryInterface(Ci.nsIFileURL);
    return fileUrl.file.clone();
  }

  // The downloads database contains a native path.  Try to create a local
  // file, though this may throw an exception if the path is invalid.
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(aFilename);
  return file;
}

let fileDownloaded = false;

let download = new DummyDownload("download.test");
download.state = 1;
download.percentComplete = 100;

let winObserver = function(win, topic) {
  if (topic == "domwindowopened") {
    win.addEventListener("load", function onLoadWindow() {
      win.removeEventListener("load", onLoadWindow, false);

      if (win.document.documentURI ==
          "chrome://mozapps/content/downloads/unknownContentType.xul") {
        ok(true, "Download dialog shown");

        executeSoon(() => {
          let button = win.document.documentElement.getButton("accept");
          button.disabled = false;
          win.document.documentElement.acceptDialog();
        });
      } else if (win.document.documentURI ==
          "chrome://webapprt/content/downloads/downloads.xul") {
        ok(true, "Download manager window shown");
        ok(fileDownloaded, "File downloaded");

        waitDownloadListPopulation(win).then(() => {
          test_downloadList(win, [download]);
          finish();
        });
      }
    }, false);
  }
}

Services.ww.registerNotification(winObserver);

let MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);
MockFilePicker.returnFiles = [download.file];
MockFilePicker.showCallback = function() {
  ok(true, "File picker shown");
  return MockFilePicker.returnOK;
}

let downloadListener = {
  onDownloadAdded: function(aDownload) {
    if (aDownload.succeeded) {
      let downloadedFile = getFile(aDownload.target.path);
      ok(downloadedFile.exists(), "Download completed");
      is(downloadedFile.fileSize, 10, "Downloaded file has correct size");
      fileDownloaded = true;
      try {
        downloadedFile.remove(true);
      } catch (ex) {
      }
    }
  },

  onDownloadChanged: function(aDownload) {
    if (aDownload.succeeded) {
      let downloadedFile = getFile(aDownload.target.path);
      ok(downloadedFile.exists(), "Download completed");
      is(downloadedFile.fileSize, 10, "Downloaded file has correct size");
      fileDownloaded = true;
      try {
        downloadedFile.remove(true);
      } catch (ex) {
      }
    }
  },
};

let downloadList;

registerCleanupFunction(function() {
  Services.wm.getMostRecentWindow("Download:Manager").close();

  Services.ww.unregisterNotification(winObserver);

  MockFilePicker.cleanup();

  if (downloadList) {
    return downloadList.removeView(downloadListener);
  }
});

function test() {
  waitForExplicitFinish();

  loadWebapp("download.webapp", undefined, function onLoad() {
    Task.spawn(function*() {
      downloadList = yield Downloads.getList(Downloads.ALL);

      yield downloadList.addView(downloadListener);

      gAppBrowser.contentDocument.getElementById("download").click();
    }).catch(function(e) {
      ok(false, "Error during test: " + e);
      SimpleTest.finish();
    });
  });
}
