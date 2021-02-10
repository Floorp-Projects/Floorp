var { FileUtils } = ChromeUtils.import("resource://gre/modules/FileUtils.jsm");
var { HandlerServiceTestUtils } = ChromeUtils.import(
  "resource://testing-common/HandlerServiceTestUtils.jsm"
);

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
 * Wait for a subdialog event indicating a dialog either opened
 * or was closed.
 *
 * First argument is the browser in which to listen. If a tabbrowser,
 * we listen to subdialogs for any tab of that browser.
 */
async function waitForSubDialog(browser, url, state) {
  let eventStr = state ? "dialogopen" : "dialogclose";

  let eventTarget;

  // Tabbrowser?
  if (browser.tabContainer) {
    eventTarget = browser.tabContainer.ownerDocument.documentElement;
  } else {
    // Individual browser. Get its box:
    let tabDialogBox = browser.ownerGlobal.gBrowser.getTabDialogBox(browser);
    eventTarget = tabDialogBox.getTabDialogManager()._dialogStack;
  }

  let checkFn;

  if (state) {
    checkFn = dialogEvent => dialogEvent.detail.dialog?._openedURL == url;
  }

  let event = await BrowserTestUtils.waitForEvent(
    eventTarget,
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

/**
 * Wait for protocol permission dialog open/close.
 * @param {MozBrowser} browser - Browser element the dialog belongs to.
 * @param {boolean} state - true: dialog open, false: dialog close
 * @returns {Promise<SubDialog>} - Returns a promise which resolves with the
 * SubDialog object of the dialog which closed or opened.
 */
async function waitForProtocolPermissionDialog(browser, state) {
  return waitForSubDialog(
    browser,
    "chrome://mozapps/content/handling/permissionDialog.xhtml",
    state
  );
}

/**
 * Wait for protocol app chooser dialog open/close.
 * @param {MozBrowser} browser - Browser element the dialog belongs to.
 * @param {boolean} state - true: dialog open, false: dialog close
 * @returns {Promise<SubDialog>} - Returns a promise which resolves with the
 * SubDialog object of the dialog which closed or opened.
 */
async function waitForProtocolAppChooserDialog(browser, state) {
  return waitForSubDialog(
    browser,
    "chrome://mozapps/content/handling/appChooser.xhtml",
    state
  );
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

function setupMailHandler() {
  let mailHandlerInfo = HandlerServiceTestUtils.getHandlerInfo("mailto");
  let gOldMailHandlers = [];

  // Remove extant web handlers because they have icons that
  // we fetch from the web, which isn't allowed in tests.
  let handlers = mailHandlerInfo.possibleApplicationHandlers;
  for (let i = handlers.Count() - 1; i >= 0; i--) {
    try {
      let handler = handlers.queryElementAt(i, Ci.nsIWebHandlerApp);
      gOldMailHandlers.push(handler);
      // If we get here, this is a web handler app. Remove it:
      handlers.removeElementAt(i);
    } catch (ex) {}
  }

  let previousHandling = mailHandlerInfo.alwaysAskBeforeHandling;
  mailHandlerInfo.alwaysAskBeforeHandling = true;

  // Create a dummy web mail handler so we always know the mailto: protocol.
  // Without this, the test fails on VMs without a default mailto: handler,
  // because no dialog is ever shown, as we ignore subframe navigations to
  // protocols that cannot be handled.
  let dummy = Cc["@mozilla.org/uriloader/web-handler-app;1"].createInstance(
    Ci.nsIWebHandlerApp
  );
  dummy.name = "Handler 1";
  dummy.uriTemplate = "https://example.com/first/%s";
  mailHandlerInfo.possibleApplicationHandlers.appendElement(dummy);

  gHandlerSvc.store(mailHandlerInfo);
  registerCleanupFunction(() => {
    // Re-add the original protocol handlers:
    let mailHandlers = mailHandlerInfo.possibleApplicationHandlers;
    for (let i = handlers.Count() - 1; i >= 0; i--) {
      try {
        // See if this is a web handler. If it is, it'll throw, otherwise,
        // we will remove it.
        mailHandlers.queryElementAt(i, Ci.nsIWebHandlerApp);
        mailHandlers.removeElementAt(i);
      } catch (ex) {}
    }
    for (let h of gOldMailHandlers) {
      mailHandlers.appendElement(h);
    }
    mailHandlerInfo.alwaysAskBeforeHandling = previousHandling;
    gHandlerSvc.store(mailHandlerInfo);
  });
}

let gDownloadDir;

async function setDownloadDir() {
  let tmpDir = await PathUtils.getTempDir();
  tmpDir = PathUtils.join(
    tmpDir,
    "testsavedir" + Math.floor(Math.random() * 2 ** 32)
  );
  // Create this dir if it doesn't exist (ignores existing dirs)
  await IOUtils.makeDirectory(tmpDir);
  registerCleanupFunction(async function() {
    try {
      await IOUtils.remove(tmpDir, { recursive: true });
    } catch (e) {
      Cu.reportError(e);
    }
  });
  Services.prefs.setIntPref("browser.download.folderList", 2);
  Services.prefs.setCharPref("browser.download.dir", tmpDir);
  return tmpDir;
}

add_task(async function test_common_initialize() {
  gDownloadDir = await setDownloadDir();
  Services.prefs.setCharPref("browser.download.loglevel", "Debug");
});
