add_task(function*() {
  // Get a helper app dialog instance:
  let helperAppDialog = Cc["@mozilla.org/helperapplauncherdialog;1"].
                        createInstance(Ci.nsIHelperAppLauncherDialog);
  // Mock the mime info:
  let mockedMIME = {
    _launched: 0,

    // nsIHandlerInfo
    type: "text/magic-automated-test",
    description: "My magic test mime type",
    defaultDescription: "Use the default app, luke!",
    hasDefaultHandler: true,
    possibleApplicationHandlers: {
      appendElement() {},
      removeElementAt() {},
      insertElementAt() {},
      replaceElementAt() {},
      clear() {},
      queryElementAt() {},
      indexOf() { return -1 },
      enumerate() { return null },
      length: 0,
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIMutableArray, Ci.nsIArray])
    },
    preferredApplicationHandler: null,
    launchWithURI() {
      this._launched++;
    },
    alwaysAskBeforeHandling: true,
    preferredAction: 2, // useHelperApp

    // nsIMIMEInfo
    getFileExtensions() { throw Cr.NS_ERROR_NOT_IMPLEMENTED; },
    setFileExtensions() {},
    extensionExists(ext) { return ext == this.primaryExtension; },
    appendExtension() {},
    primaryExtension: "something",
    MIMEType: "text/magic-automated-test",

    equals() { return false },

    possibleLocalHandlers: {
      appendElement() {},
      removeElementAt() {},
      insertElementAt() {},
      replaceElementAt() {},
      clear() {},
      queryElementAt() {},
      indexOf() { return -1 },
      enumerate() { return null },
      length: 0,
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIArray])
    },

    launchWithFile() {
      this._launched++;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIMIMEInfo, Ci.nsIHandlerInfo])
  };
  // Mock the launcher:
  let launcher = {
    _saveCount: 0,
    _cancelCount: 0,
    _launched: 0,

    MIMEInfo: mockedMIME,
    source: Services.io.newURI("http://www.mozilla.org/", null, null),
    suggestedFileName: "test_always_ask_preferred_app.something",
    targetFileIsExecutable: false,
    saveToDisk() {
      this._saveCount++;
    },
    cancel() {
      this._cancelCount++;
    },
    launchWithApplication() {
      this._launched++;
    },
    setWebProgressListener() {
    },
    saveDestinationAvailable() {
    },
    contentLength: 42,
    targetFile: null, // never read
    // PRTime is microseconds since epoch, Date.now() returns milliseconds:
    timeDownloadStarted: Date.now() * 1000,
    QueryInterface: XPCOMUtils.generateQI([Ci.nsICancelable, Ci.nsIHelperAppLauncher])
  };

  let helperAppDialogShownPromise = BrowserTestUtils.domWindowOpened();
  try {
    helperAppDialog.show(launcher, window, "foopy");
  } catch (ex) {
    ok(false, "Trying to show unknownContentType.xul failed with exception: " + ex);
    Cu.reportError(ex);
  }
  let dlg = yield helperAppDialogShownPromise;
  yield BrowserTestUtils.waitForEvent(dlg, "load", false);
  is(dlg.location.href, "chrome://mozapps/content/downloads/unknownContentType.xul",
     "Got correct dialog");
  let doc = dlg.document;
  let location = doc.getElementById("source");
  let expectedValue = launcher.source.prePath;
  if (location.value != expectedValue) {
    info("Waiting for dialog to be populated.");
    yield BrowserTestUtils.waitForAttribute("value", location, expectedValue);
  }
  is(doc.getElementById("mode").selectedItem.id, "open", "Should be opening the file.");
  ok(!dlg.document.getElementById("openHandler").selectedItem.hidden,
     "Should not have selected a hidden item.");
  let helperAppDialogHiddenPromise = BrowserTestUtils.windowClosed(dlg);
  doc.documentElement.cancelDialog();
  yield helperAppDialogHiddenPromise;
});

