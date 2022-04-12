const { DownloadIntegration } = ChromeUtils.import(
  "resource://gre/modules/DownloadIntegration.jsm"
);

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

// New file is being downloaded and no dialogs are shown in the way.
add_task(async function skipDialogAndDownloadFile() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.useDownloadDir", true],
      ["image.webp.enabled", true],
    ],
  });

  let publicList = await Downloads.getList(Downloads.PUBLIC);
  registerCleanupFunction(async () => {
    await publicList.removeFinished();
  });
  let downloadFinishedPromise = promiseDownloadFinished(publicList);

  let initialTabsCount = gBrowser.tabs.length;

  let loadingTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_PATH + "file_green.webp",
    waitForLoad: false,
    waitForStateStop: true,
  });

  // We just open the file to be downloaded... and wait for it to be downloaded!
  // We see no dialogs to be accepted in the process.
  let download = await downloadFinishedPromise;
  await BrowserTestUtils.waitForCondition(
    () => gBrowser.tabs.length == initialTabsCount + 2
  );

  gBrowser.removeCurrentTab();
  BrowserTestUtils.removeTab(loadingTab);

  Assert.ok(
    await OS.File.exists(download.target.path),
    "The file should have been downloaded."
  );

  try {
    info("removing " + download.target.path);
    if (Services.appinfo.OS === "WINNT") {
      // We need to make the file writable to delete it on Windows.
      await IOUtils.setPermissions(download.target.path, 0o600);
    }
    await IOUtils.remove(download.target.path);
  } catch (ex) {
    info("The file " + download.target.path + " is not removed, " + ex);
  }
});

// Test that pref browser.download.always_ask_before_handling_new_types causes
// the UCT dialog to be opened for unrecognized mime types
add_task(async function skipDialogAndDownloadFile() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.always_ask_before_handling_new_types", true],
      ["browser.download.useDownloadDir", true],
    ],
  });

  let UCTObserver = {
    opened: PromiseUtils.defer(),
    closed: PromiseUtils.defer(),
    observe(aSubject, aTopic, aData) {
      let win = aSubject;
      switch (aTopic) {
        case "domwindowopened":
          win.addEventListener(
            "load",
            function onLoad(event) {
              // Let the dialog initialize
              SimpleTest.executeSoon(function() {
                UCTObserver.opened.resolve(win);
              });
            },
            { once: true }
          );
          break;
        case "domwindowclosed":
          if (
            win.location ==
            "chrome://mozapps/content/downloads/unknownContentType.xhtml"
          ) {
            this.closed.resolve();
          }
          break;
      }
    },
  };
  Services.ww.registerNotification(UCTObserver);

  info("Opening new tab with file of unknown content type");
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url:
        "http://mochi.test:8888/browser/toolkit/mozapps/downloads/tests/browser/unknownContentType_dialog_layout_data.pif",
      waitForLoad: false,
      waitForStateStop: true,
    },
    async function() {
      let uctWindow = await UCTObserver.opened.promise;
      info("Unknown content type dialog opened");
      let focusOnDialog = SimpleTest.promiseFocus(uctWindow);
      uctWindow.focus();
      await focusOnDialog;
      uctWindow.document.getElementById("unknownContentType").cancelDialog();
      info("Unknown content type dialog canceled");
      uctWindow = null;
      Services.ww.unregisterNotification(UCTObserver);
    }
  );
});
