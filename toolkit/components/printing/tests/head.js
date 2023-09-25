const PRINT_DOCUMENT_URI = "chrome://global/content/print.html";
const DEFAULT_PRINTER_NAME = "Mozilla Save to PDF";
const { MockFilePicker } = SpecialPowers;

let pickerMocked = false;

class PrintHelper {
  static async withTestPage(testFn, pagePathname, useHTTPS = false) {
    let pageUrl = "";
    if (pagePathname) {
      pageUrl = useHTTPS
        ? this.getTestPageUrlHTTPS(pagePathname)
        : this.getTestPageUrl(pagePathname);
    } else {
      pageUrl = useHTTPS
        ? this.defaultTestPageUrlHTTPS
        : this.defaultTestPageUrl;
    }
    info("withTestPage: " + pageUrl);
    let isPdf = pageUrl.endsWith(".pdf");

    let taskReturn = await BrowserTestUtils.withNewTab(
      isPdf ? "about:blank" : pageUrl,
      async function (browser) {
        if (isPdf) {
          let loaded = BrowserTestUtils.waitForContentEvent(
            browser,
            "documentloaded",
            false,
            null,
            true
          );
          BrowserTestUtils.startLoadingURIString(browser, pageUrl);
          await loaded;
        }
        await testFn(new PrintHelper(browser));
      }
    );

    await SpecialPowers.popPrefEnv();
    if (isPdf) {
      await SpecialPowers.popPrefEnv();
    }

    // Reset all of the other printing prefs to their default.
    this.resetPrintPrefs();
    return taskReturn;
  }

  static async withTestPageHTTPS(testFn, pagePathname) {
    return this.withTestPage(testFn, pagePathname, /* useHttps */ true);
  }

  static resetPrintPrefs() {
    for (let name of Services.prefs.getChildList("print.")) {
      Services.prefs.clearUserPref(name);
    }
    Services.prefs.clearUserPref("print_printer");
    Services.prefs.clearUserPref("print.more-settings.open");
  }

  static getTestPageUrl(pathName) {
    const testPath = getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "http://example.com"
    );
    return testPath + pathName;
  }

  static getTestPageUrlHTTPS(pathName) {
    const testPath = getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.com"
    );
    return testPath + pathName;
  }

  static get defaultTestPageUrl() {
    return this.getTestPageUrl("simplifyArticleSample.html");
  }

  static get defaultTestPageUrlHTTPS() {
    return this.getTestPageUrlHTTPS("simplifyArticleSample.html");
  }

  static createMockPaper(paperProperties = {}) {
    return Object.assign(
      {
        id: "regular",
        name: "Regular Size",
        width: 612,
        height: 792,
        unwriteableMargin: Promise.resolve(
          paperProperties.unwriteableMargin || {
            top: 0.1,
            bottom: 0.1,
            left: 0.1,
            right: 0.1,
            QueryInterface: ChromeUtils.generateQI([Ci.nsIPaperMargin]),
          }
        ),
        QueryInterface: ChromeUtils.generateQI([Ci.nsIPaper]),
      },
      paperProperties
    );
  }

  constructor(sourceBrowser) {
    this.sourceBrowser = sourceBrowser;
  }

  async startPrint(condition = {}) {
    this.sourceBrowser.ownerGlobal.document
      .getElementById("cmd_print")
      .doCommand();
    return this.waitForDialog(condition);
  }

  async waitForDialog(condition = {}) {
    let dialog = await TestUtils.waitForCondition(
      () => this.dialog,
      "Wait for dialog"
    );
    await dialog._dialogReady;

    if (Object.keys(condition).length === 0) {
      await this.win._initialized;
      // Wait a frame so the rendering spinner is hidden.
      await new Promise(resolve => requestAnimationFrame(resolve));
    } else if (condition.waitFor == "loadComplete") {
      await BrowserTestUtils.waitForAttributeRemoval("loading", this.doc.body);
    }
  }

  beforeInit(initFn) {
    // Run a function when the print.html document is created,
    // but before its init is called from the domcontentloaded handler
    TestUtils.topicObserved("document-element-inserted", doc => {
      return (
        doc.nodePrincipal.isSystemPrincipal &&
        doc.contentType == "text/html" &&
        doc.URL.startsWith("chrome://global/content/print.html")
      );
    }).then(([doc]) => {
      doc.addEventListener("DOMContentLoaded", () => {
        initFn(doc.ownerGlobal);
      });
    });
  }

  async withClosingFn(closeFn) {
    let { dialog } = this;
    await closeFn();
    if (this.dialog) {
      await TestUtils.waitForCondition(
        () => !this.dialog,
        "Wait for dialog to close"
      );
    }
    await dialog._closingPromise;
  }

  resetSettings() {
    this.win.PrintEventHandler.settings =
      this.win.PrintEventHandler.defaultSettings;
    this.win.PrintEventHandler.saveSettingsToPrefs(
      this.win.PrintEventHandler.kInitSaveAll
    );
  }

  async closeDialog() {
    this.resetSettings();
    await this.withClosingFn(() => this.dialog.close());
  }

  assertDialogClosed() {
    is(this._dialogs.length, 0, "There are no print dialogs");
  }

  assertDialogOpen() {
    is(this._dialogs.length, 1, "There is one print dialog");
    ok(BrowserTestUtils.is_visible(this.dialog._box), "The dialog is visible");
  }

  assertDialogHidden() {
    is(this._dialogs.length, 1, "There is one print dialog");
    ok(BrowserTestUtils.is_hidden(this.dialog._box), "The dialog is hidden");
    ok(
      this.dialog._box.getBoundingClientRect().width > 0,
      "The dialog should still have boxes"
    );
  }

  async assertPrintToFile(file, testFn) {
    ok(!file.exists(), "File does not exist before printing");
    await this.withClosingFn(testFn);
    await TestUtils.waitForCondition(
      () => file.exists() && file.fileSize > 0,
      "Wait for target file to get created",
      50
    );
    ok(file.exists(), "Created target file");

    await TestUtils.waitForCondition(
      () => file.fileSize > 0,
      "Wait for the print progress to run",
      50
    );

    ok(file.fileSize > 0, "Target file not empty");
  }

  setupMockPrint() {
    if (this.resolveShowSystemDialog) {
      throw new Error("Print already mocked");
    }

    // Create some Promises that we can resolve from the test.
    let showSystemDialogPromise = new Promise(resolve => {
      this.resolveShowSystemDialog = result => {
        if (result !== undefined) {
          resolve(result);
        } else {
          resolve(true);
        }
      };
    });
    let printPromise = new Promise((resolve, reject) => {
      this.resolvePrint = resolve;
      this.rejectPrint = reject;
    });

    // Mock PrintEventHandler with our Promises.
    this.win.PrintEventHandler._showPrintDialog = (
      window,
      haveSelection,
      settings
    ) => {
      this.systemDialogOpenedWithSelection = haveSelection;
      return showSystemDialogPromise;
    };
    this.win.PrintEventHandler._doPrint = (bc, settings) => {
      this._printedSettings = settings;
      return printPromise;
    };
  }

  addMockPrinter(opts = {}) {
    if (typeof opts == "string") {
      opts = { name: opts };
    }
    let {
      name = "Mock Printer",
      paperList,
      printerInfoPromise = Promise.resolve(),
      paperSizeUnit = Ci.nsIPrintSettings.kPaperSizeInches,
      paperId,
    } = opts;
    let PSSVC = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(
      Ci.nsIPrintSettingsService
    );
    // Use the fallbackPaperList as the default for mock printers
    if (!paperList) {
      info("addMockPrinter, using the fallbackPaperList");
      paperList = Cc["@mozilla.org/gfx/printerlist;1"].createInstance(
        Ci.nsIPrinterList
      ).fallbackPaperList;
    }

    let defaultSettings = PSSVC.createNewPrintSettings();
    defaultSettings.printerName = name;
    defaultSettings.toFileName = "";
    defaultSettings.outputFormat = Ci.nsIPrintSettings.kOutputFormatNative;
    defaultSettings.outputDestination =
      Ci.nsIPrintSettings.kOutputDestinationPrinter;
    defaultSettings.paperSizeUnit = paperSizeUnit;
    if (paperId) {
      defaultSettings.paperId = paperId;
    }

    if (
      defaultSettings.paperId &&
      Array.from(paperList).find(p => p.id == defaultSettings.paperId)
    ) {
      info(
        `addMockPrinter, using paperId: ${defaultSettings.paperId} from the paperList`
      );
    } else if (paperList.length) {
      defaultSettings.paperId = paperList[0].id;
      info(
        `addMockPrinter, corrected default paperId setting value: ${defaultSettings.paperId}`
      );
    }

    let printer = {
      name,
      supportsColor: Promise.resolve(true),
      supportsMonochrome: Promise.resolve(true),
      printerInfo: printerInfoPromise.then(() => ({
        paperList,
        defaultSettings,
        QueryInterface: ChromeUtils.generateQI([Ci.nsIPrinterInfo]),
      })),
      QueryInterface: ChromeUtils.generateQI([Ci.nsIPrinter]),
    };

    if (!this._mockPrinters) {
      this._mockPrinters = [printer];
      this.beforeInit(win => (win._mockPrinters = this._mockPrinters));
    } else {
      this._mockPrinters.push(printer);
    }
    return printer;
  }

  get _tabDialogBox() {
    return this.sourceBrowser.ownerGlobal.gBrowser.getTabDialogBox(
      this.sourceBrowser
    );
  }

  get _tabDialogBoxManager() {
    return this._tabDialogBox.getTabDialogManager();
  }

  get _dialogs() {
    return this._tabDialogBox.getTabDialogManager()._dialogs;
  }

  get dialog() {
    return this._dialogs.find(dlg =>
      dlg._box.querySelector(".printSettingsBrowser")
    );
  }

  get paginationElem() {
    return this.dialog._box.querySelector(".printPreviewNavigation");
  }

  get paginationSheetIndicator() {
    return this.paginationElem.shadowRoot.querySelector("#sheetIndicator");
  }

  get currentPrintPreviewBrowser() {
    return this.win.PrintEventHandler.printPreviewEl.lastPreviewBrowser;
  }

  get _printBrowser() {
    return this.dialog._frame;
  }

  get doc() {
    return this._printBrowser.contentDocument;
  }

  get win() {
    return this._printBrowser.contentWindow;
  }

  get(id) {
    return this.doc.getElementById(id);
  }

  get sheetCount() {
    return this.doc.l10n.getAttributes(this.get("sheet-count")).args.sheetCount;
  }

  get sourceURI() {
    return this.win.PrintEventHandler.activeCurrentURI;
  }

  async waitForReaderModeReady() {
    if (gBrowser.selectedBrowser.isArticle) {
      return;
    }
    await new Promise(resolve => {
      let onReaderModeChange = {
        receiveMessage(message) {
          if (
            message.data &&
            message.data.isArticle !== undefined &&
            gBrowser.selectedBrowser.isArticle
          ) {
            AboutReaderParent.removeMessageListener(
              "Reader:UpdateReaderButton",
              onReaderModeChange
            );
            resolve();
          }
        },
      };
      AboutReaderParent.addMessageListener(
        "Reader:UpdateReaderButton",
        onReaderModeChange
      );
    });
  }

  async waitForPreview(changeFn) {
    changeFn();
    await BrowserTestUtils.waitForEvent(this.doc, "preview-updated");
  }

  async waitForSettingsEvent(changeFn) {
    let changed = BrowserTestUtils.waitForEvent(this.doc, "print-settings");
    await changeFn?.();
    await BrowserTestUtils.waitForCondition(
      () => !this.win.PrintEventHandler._delayedSettingsChangeTask.isArmed,
      "Wait for all delayed tasks to execute"
    );
    await changed;
  }

  click(el, { scroll = true } = {}) {
    if (scroll) {
      el.scrollIntoView();
    }
    ok(BrowserTestUtils.is_visible(el), "Element must be visible to click");
    EventUtils.synthesizeMouseAtCenter(el, {}, this.win);
  }

  text(el, text) {
    this.click(el);
    el.value = "";
    EventUtils.sendString(text, this.win);
  }

  async openMoreSettings(options) {
    let details = this.get("more-settings");
    if (!details.open) {
      this.click(details.firstElementChild, options);
    }
    await this.awaitAnimationFrame();
  }

  dispatchSettingsChange(settings) {
    this.doc.dispatchEvent(
      new CustomEvent("update-print-settings", {
        detail: settings,
      })
    );
  }

  get settings() {
    return this.win.PrintEventHandler.settings;
  }

  get viewSettings() {
    return this.win.PrintEventHandler.viewSettings;
  }

  _assertMatches(a, b, msg) {
    if (Array.isArray(a)) {
      is(a.length, b.length, msg);
      for (let i = 0; i < a.length; ++i) {
        this._assertMatches(a[i], b[i], msg);
      }
      return;
    }
    is(a, b, msg);
  }

  assertSettingsMatch(expected) {
    let { settings } = this;
    for (let [setting, value] of Object.entries(expected)) {
      this._assertMatches(settings[setting], value, `${setting} matches`);
    }
  }

  assertPrintedWithSettings(expected) {
    ok(this._printedSettings, "Printed settings have been recorded");
    for (let [setting, value] of Object.entries(expected)) {
      this._assertMatches(
        this._printedSettings[setting],
        value,
        `${setting} matches printed setting`
      );
    }
  }

  get _lastPrintPreviewSettings() {
    return this.win.PrintEventHandler._lastPrintPreviewSettings;
  }

  assertPreviewedWithSettings(expected) {
    let settings = this._lastPrintPreviewSettings;
    ok(settings, "Last preview settings are available");
    for (let [setting, value] of Object.entries(expected)) {
      this._assertMatches(
        settings[setting],
        value,
        `${setting} matches previewed setting`
      );
    }
  }

  async assertSettingsChanged(from, to, changeFn) {
    is(
      Object.keys(from).length,
      Object.keys(to).length,
      "Got the same number of settings to check"
    );
    ok(
      Object.keys(from).every(s => s in to),
      "Checking the same setting names"
    );
    this.assertSettingsMatch(from);
    await changeFn();
    this.assertSettingsMatch(to);
  }

  async assertSettingsNotChanged(settings, changeFn) {
    await this.assertSettingsChanged(settings, settings, changeFn);
  }

  awaitAnimationFrame() {
    return new Promise(resolve => this.win.requestAnimationFrame(resolve));
  }

  mockFilePickerCancel() {
    if (!pickerMocked) {
      pickerMocked = true;
      MockFilePicker.init(window);
      registerCleanupFunction(() => MockFilePicker.cleanup());
    }
    MockFilePicker.returnValue = MockFilePicker.returnCancel;
  }

  mockFilePicker(filename) {
    if (!pickerMocked) {
      pickerMocked = true;
      MockFilePicker.init(window);
      registerCleanupFunction(() => MockFilePicker.cleanup());
    }
    MockFilePicker.returnValue = MockFilePicker.returnOK;
    let file = Services.dirsvc.get("TmpD", Ci.nsIFile);
    file.append(filename);
    registerCleanupFunction(() => {
      if (file.exists()) {
        file.remove(false);
      }
    });
    MockFilePicker.setFiles([file]);
    return file;
  }
}

function waitForPreviewVisible() {
  return BrowserTestUtils.waitForCondition(function () {
    let preview = document.querySelector(".printPreviewBrowser");
    return preview && BrowserTestUtils.is_visible(preview);
  });
}
