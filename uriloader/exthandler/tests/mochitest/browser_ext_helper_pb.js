const { Downloads } = Cu.import("resource://gre/modules/Downloads.jsm", {});
const URI = "http://example.com/browser/uriloader/exthandler/tests/mochitest/blob.html";

add_task(async function testExtHelperInPrivateBrowsing() {
  let win = await BrowserTestUtils.openNewBrowserWindow({private: true});
  let browser = win.gBrowser.selectedBrowser;
  browser.loadURI(URI);
  await BrowserTestUtils.browserLoaded(browser);

  let listener = {
    _resolve: null,

    onOpenWindow (aXULWindow) {
      info("Download window shown...");
      var domwindow = aXULWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDOMWindow);
      waitForFocus(function() {
        // When the download dialog is shown, its accept button is only enabled
        // after 1000ms (which is configured in the pref "security.dialog_enable_delay",
        // see SharedPromptUtils.jsm.
        setTimeout(() => {
          // Set the choice to 'Save As File'.
          let mode = domwindow.document.getElementById("mode");
          let save = domwindow.document.getElementById("save");
          let index = mode.getIndexOfItem(save);
          mode.selectedItem = save;
          domwindow.document.documentElement.acceptDialog();
          // acceptDialog will close domwindow itself, so we don't have to close
          // domwindow here.
        }, 1000);
      }, domwindow);
    },

    onCloseWindow: function(aXULWindow) {
      info("onCloseWindow");
      Services.wm.removeListener(listener);
      this._resolve();
    },
    onWindowTitleChange: function(aXULWindow, aNewTitle) {},

    waitForDownload: function() {
      return new Promise(resolve => {
        this._resolve = resolve;
      });
    }
  };

  Services.wm.addListener(listener);

  await ContentTask.spawn(browser, {}, async function() {
    var download = content.document.getElementById("download");
    download.click();
  });

  // Wait until download is finished.
  // However there seems to be no easy way to get notified when the download is
  // completed, so we use onCloseWindow listener here.
  await listener.waitForDownload();

  let allList = await Downloads.getList(Downloads.ALL);
  let allDownloads = await allList.getAll();
  Assert.equal(allDownloads.length, 1, "Should have at least 1 download in ALL mode");

  let publicList = await Downloads.getList(Downloads.PUBLIC);
  let publicDownloads = await publicList.getAll();
  Assert.equal(publicDownloads.length, 0, "Shouldn't have any download in normal mode");

  let privateList = await Downloads.getList(Downloads.PRIVATE);
  let privateDownloads = await privateList.getAll();
  Assert.equal(privateDownloads.length, 1, "Should have 1 download in private mode");

  win.close();
  finish();
});
