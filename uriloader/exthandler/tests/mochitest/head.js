var { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs"
);
var { HandlerServiceTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/HandlerServiceTestUtils.sys.mjs"
);

var gMimeSvc = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
var gHandlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
  Ci.nsIHandlerService
);

function createMockedHandlerApp() {
  // Mock the executable
  let mockedExecutable = new FileUtils.File(
    PathUtils.join(PathUtils.tempDir, "mockedExecutable")
  );
  if (!mockedExecutable.exists()) {
    mockedExecutable.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o755);
  }

  // Mock the handler app
  let mockedHandlerApp = Cc[
    "@mozilla.org/uriloader/local-handler-app;1"
  ].createInstance(Ci.nsILocalHandlerApp);
  mockedHandlerApp.executable = mockedExecutable;
  mockedHandlerApp.detailedDescription = "Mocked handler app";

  registerCleanupFunction(function () {
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
    setDownloadToLaunch() {},
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

  registerCleanupFunction(function () {
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

function createTemporarySaveDirectory() {
  var saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists()) {
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  }
  return saveDir;
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
    console.error(ex);
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

async function promiseDownloadFinished(list, stopFromOpening) {
  return new Promise(resolve => {
    list.addView({
      onDownloadChanged(download) {
        if (stopFromOpening) {
          download.launchWhenSucceeded = false;
        }
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
  let tmpDir = PathUtils.join(
    PathUtils.tempDir,
    "testsavedir" + Math.floor(Math.random() * 2 ** 32)
  );
  // Create this dir if it doesn't exist (ignores existing dirs)
  await IOUtils.makeDirectory(tmpDir);
  registerCleanupFunction(async function () {
    Services.prefs.clearUserPref("browser.download.folderList");
    Services.prefs.clearUserPref("browser.download.dir");
    try {
      await IOUtils.remove(tmpDir, { recursive: true });
    } catch (e) {
      console.error(e);
    }
  });
  Services.prefs.setIntPref("browser.download.folderList", 2);
  Services.prefs.setCharPref("browser.download.dir", tmpDir);
  return tmpDir;
}

add_setup(async function test_common_initialize() {
  gDownloadDir = await setDownloadDir();
  Services.prefs.setCharPref("browser.download.loglevel", "Debug");
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("browser.download.loglevel");
  });
});

async function removeAllDownloads() {
  let publicList = await Downloads.getList(Downloads.PUBLIC);
  let downloads = await publicList.getAll();
  for (let download of downloads) {
    await publicList.remove(download);
    if (await IOUtils.exists(download.target.path)) {
      await download.finalize(true);
    }
  }
}

// Helpers for external protocol sandbox tests.
const EXT_PROTO_URI_MAILTO = "mailto:test@example.com";

/**
 * Creates and iframe and navigate to an external protocol from the iframe.
 * @param {MozBrowser} browser - Browser to spawn iframe in.
 * @param {string} sandboxAttr - Sandbox attribute value for the iframe.
 * @param {'trustedClick'|'untrustedClick'|'trustedLocationAPI'|'untrustedLocationAPI'|'frameSrc'|'frameSrcRedirect'} triggerMethod
 *  - How to trigger the navigation to the external protocol.
 */
async function navigateExternalProtoFromIframe(
  browser,
  sandboxAttr,
  useCSPSandbox = false,
  triggerMethod = "trustedClick"
) {
  if (
    ![
      "trustedClick",
      "untrustedClick",
      "trustedLocationAPI",
      "untrustedLocationAPI",
      "frameSrc",
      "frameSrcRedirect",
    ].includes(triggerMethod)
  ) {
    throw new Error("Invalid trigger method " + triggerMethod);
  }

  // Construct the url to use as iframe src.
  let testPath = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  );
  let frameSrc = testPath + "/protocol_custom_sandbox_helper.sjs";

  // Load the external protocol directly via the frame src field.
  if (triggerMethod == "frameSrc") {
    frameSrc = EXT_PROTO_URI_MAILTO;
  } else if (triggerMethod == "frameSrcRedirect") {
    let url = new URL(frameSrc);
    url.searchParams.set("redirectCustomProtocol", "true");
    frameSrc = url.href;
  }

  // If enabled set the sandbox attributes via CSP header instead. To do
  // this we need to pass the sandbox flags to the test server via query
  // params.
  if (useCSPSandbox) {
    let url = new URL(frameSrc);
    url.searchParams.set("cspSandbox", sandboxAttr);
    frameSrc = url.href;

    // If we use CSP sandbox attributes we shouldn't set any via iframe attribute.
    sandboxAttr = null;
  }

  // Create a sandboxed iframe and navigate to the external protocol.
  await SpecialPowers.spawn(
    browser,
    [sandboxAttr, frameSrc, EXT_PROTO_URI_MAILTO, triggerMethod],
    async (sandbox, src, extProtoURI, trigger) => {
      let frame = content.document.createElement("iframe");

      if (sandbox != null) {
        frame.sandbox = sandbox;
      }

      frame.src = src;

      let useFrameSrc = trigger == "frameSrc" || trigger == "frameSrcRedirect";

      // Create frame load promise.
      let frameLoadPromise;
      // We won't get a load event if we directly put the external protocol in
      // the frame src.
      if (!useFrameSrc) {
        frameLoadPromise = ContentTaskUtils.waitForEvent(frame, "load", false);
      }

      content.document.body.appendChild(frame);
      await frameLoadPromise;

      if (!useFrameSrc) {
        // Trigger the external protocol navigation in the iframe. We test
        // navigation by clicking links and navigation via the history API.
        await SpecialPowers.spawn(
          frame,
          [extProtoURI, trigger],
          async (uri, trigger2) => {
            let link = content.document.createElement("a");
            link.innerText = "CLICK ME";
            link.id = "extProtoLink";
            content.document.body.appendChild(link);

            if (trigger2 == "trustedClick" || trigger2 == "untrustedClick") {
              link.href = uri;
            } else if (
              trigger2 == "trustedLocationAPI" ||
              trigger2 == "untrustedLocationAPI"
            ) {
              link.setAttribute("onclick", `location.href = '${uri}'`);
            }

            if (
              trigger2 == "untrustedClick" ||
              trigger2 == "untrustedLocationAPI"
            ) {
              link.click();
            } else if (
              trigger2 == "trustedClick" ||
              trigger2 == "trustedLocationAPI"
            ) {
              await ContentTaskUtils.waitForCondition(
                () => link,
                "wait for link to be present"
              );
              await EventUtils.synthesizeMouseAtCenter(link, {}, content);
            }
          }
        );
      }
    }
  );
}

/**
 * Wait for the sandbox error message which is shown in the web console when an
 * external protocol navigation from a sandboxed context is blocked.
 * @returns {Promise} - Promise which resolves once message has been logged.
 */
function waitForExtProtocolSandboxError() {
  return new Promise(resolve => {
    Services.console.registerListener(function onMessage(msg) {
      let { message, logLevel } = msg;
      if (logLevel != Ci.nsIConsoleMessage.error) {
        return;
      }
      if (
        !message.includes(
          `Blocked navigation to custom protocol “${EXT_PROTO_URI_MAILTO}” from a sandboxed context.`
        )
      ) {
        return;
      }
      Services.console.unregisterListener(onMessage);
      resolve();
    });
  });
}

/**
 * Run the external protocol sandbox test using iframes.
 * @param {Object} options
 * @param {boolean} options.blocked - Whether the navigation should be blocked.
 * @param {string} options.sandbox -   See {@link navigateExternalProtoFromIframe}.
 * @param {string} options.useCSPSandbox -  See {@link navigateExternalProtoFromIframe}.
 * @param {string} options.triggerMethod - See {@link navigateExternalProtoFromIframe}.
 * @returns {Promise} - Promise which resolves once the test has finished.
 */
function runExtProtocolSandboxTest(options) {
  let { blocked, sandbox, useCSPSandbox = false, triggerMethod } = options;

  let testPath = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  );

  info("runSandboxTest options: " + JSON.stringify(options));
  return BrowserTestUtils.withNewTab(
    testPath + "/protocol_custom_sandbox_helper.sjs",
    async browser => {
      if (blocked) {
        let errorPromise = waitForExtProtocolSandboxError();
        await navigateExternalProtoFromIframe(
          browser,
          sandbox,
          useCSPSandbox,
          triggerMethod
        );
        await errorPromise;

        ok(
          errorPromise,
          "Should not show the dialog for iframe with sandbox " + sandbox
        );
      } else {
        let dialogWindowOpenPromise = waitForProtocolAppChooserDialog(
          browser,
          true
        );
        await navigateExternalProtoFromIframe(
          browser,
          sandbox,
          useCSPSandbox,
          triggerMethod
        );
        let dialog = await dialogWindowOpenPromise;

        ok(dialog, "Should show the dialog for sandbox " + sandbox);

        // Close dialog before closing the tab to avoid intermittent failures.
        let dialogWindowClosePromise = waitForProtocolAppChooserDialog(
          browser,
          false
        );

        dialog.close();
        await dialogWindowClosePromise;
      }
    }
  );
}
