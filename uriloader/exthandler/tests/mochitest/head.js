var { FileUtils } = ChromeUtils.import("resource://gre/modules/FileUtils.jsm");

var gMimeSvc = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
var gHandlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
  Ci.nsIHandlerService
);

function createMockedHandlerApp() {
  // Mock the executable
  let mockedExecutable = FileUtils.getFile("TmpD", ["mockedExecutable"]);
  if (!mockedExecutable.exists()) {
    mockedExecutable.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o755);
  }

  // Mock the handler app
  let mockedHandlerApp = Cc[
    "@mozilla.org/uriloader/local-handler-app;1"
  ].createInstance(Ci.nsILocalHandlerApp);
  mockedHandlerApp.executable = mockedExecutable;
  mockedHandlerApp.detailedDescription = "Mocked handler app";

  registerCleanupFunction(function() {
    // remove the mocked executable from disk.
    if (mockedExecutable.exists()) {
      mockedExecutable.remove(true);
    }
  });

  return mockedHandlerApp;
}

function createMockedObjects(createHandlerApp) {
  // Mock the mime info
  let internalMockedMIME = gMimeSvc.getFromTypeAndExtension(
    "text/x-test-handler",
    null
  );
  internalMockedMIME.alwaysAskBeforeHandling = true;
  internalMockedMIME.preferredAction = Ci.nsIHandlerInfo.useHelperApp;
  internalMockedMIME.appendExtension("abc");
  if (createHandlerApp) {
    let mockedHandlerApp = createMockedHandlerApp();
    internalMockedMIME.description = mockedHandlerApp.detailedDescription;
    internalMockedMIME.possibleApplicationHandlers.appendElement(
      mockedHandlerApp
    );
    internalMockedMIME.preferredApplicationHandler = mockedHandlerApp;
  }

  // Proxy for the mocked MIME info for faking the read-only attributes
  let mockedMIME = new Proxy(internalMockedMIME, {
    get(target, property) {
      switch (property) {
        case "hasDefaultHandler":
          return true;
        case "defaultDescription":
          return "Default description";
        default:
          return target[property];
      }
    },
  });

  // Mock the launcher:
  let mockedLauncher = {
    MIMEInfo: mockedMIME,
    source: Services.io.newURI("http://www.mozilla.org/"),
    suggestedFileName: "test_download_dialog.abc",
    targetFileIsExecutable: false,
    saveToDisk() {},
    cancel() {},
    launchWithApplication() {},
    setWebProgressListener() {},
    saveDestinationAvailable() {},
    contentLength: 42,
    targetFile: null, // never read
    // PRTime is microseconds since epoch, Date.now() returns milliseconds:
    timeDownloadStarted: Date.now() * 1000,
    QueryInterface: ChromeUtils.generateQI([
      "nsICancelable",
      "nsIHelperAppLauncher",
    ]),
  };

  registerCleanupFunction(function() {
    // remove the mocked mime info from database.
    let mockHandlerInfo = gMimeSvc.getFromTypeAndExtension(
      "text/x-test-handler",
      null
    );
    if (gHandlerSvc.exists(mockHandlerInfo)) {
      gHandlerSvc.remove(mockHandlerInfo);
    }
  });

  return mockedLauncher;
}

async function openHelperAppDialog(launcher) {
  let helperAppDialog = Cc[
    "@mozilla.org/helperapplauncherdialog;1"
  ].createInstance(Ci.nsIHelperAppLauncherDialog);

  let helperAppDialogShownPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
  try {
    helperAppDialog.show(launcher, window, "foopy");
  } catch (ex) {
    ok(
      false,
      "Trying to show unknownContentType.xhtml failed with exception: " + ex
    );
    Cu.reportError(ex);
  }
  let dlg = await helperAppDialogShownPromise;

  is(
    dlg.location.href,
    "chrome://mozapps/content/downloads/unknownContentType.xhtml",
    "Got correct dialog"
  );

  return dlg;
}

/**
 * Wait for protocol ask dialog open/close.
 * @param {*} browser - Browser element the dialog belongs to.
 * @param {Boolean} state - true: dialog open, false: dialog close
 * @returns {Promise<SubDialog>} - Returns a promise which resolves with the
 * SubDialog object of the dialog which closed or opened.
 */
async function waitForProtocolAskDialog(browser, state) {
  const CONTENT_HANDLING_URL = "chrome://mozapps/content/handling/dialog.xhtml";

  let eventStr = state ? "dialogopen" : "dialogclose";

  let tabDialogBox = gBrowser.getTabDialogBox(browser);
  let dialogStack = tabDialogBox._dialogManager._dialogStack;

  let checkFn;

  if (state) {
    checkFn = dialogEvent =>
      dialogEvent.detail.dialog?._openedURL == CONTENT_HANDLING_URL;
  }

  let event = await BrowserTestUtils.waitForEvent(
    dialogStack,
    eventStr,
    true,
    checkFn
  );

  let { dialog } = event.detail;

  // If the dialog is closing wait for it to be fully closed before resolving
  if (!state) {
    await dialog._closingPromise;
  }

  return event.detail.dialog;
}

async function promiseDownloadFinished(list) {
  return new Promise(resolve => {
    list.addView({
      onDownloadChanged(download) {
        info("Download changed!");
        if (download.succeeded || download.error) {
          info("Download succeeded or errored");
          list.removeView(this);
          resolve(download);
        }
      },
    });
  });
}
