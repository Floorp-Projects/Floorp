const PRINT_DOCUMENT_URI = "chrome://global/content/print.html";
const { MockFilePicker } = SpecialPowers;

let pickerMocked = false;

class PrintHelper {
  static async withTestPage(testFn) {
    await SpecialPowers.pushPrefEnv({
      set: [["print.tab_modal.enabled", true]],
    });

    let taskReturn = await BrowserTestUtils.withNewTab(
      this.defaultTestPageUrl,
      async function(browser) {
        await testFn(new PrintHelper(browser));
      }
    );

    await SpecialPowers.popPrefEnv();

    // Reset all of the other printing prefs to their default.
    for (let name of Services.prefs.getChildList("print.")) {
      Services.prefs.clearUserPref(name);
    }
    Services.prefs.clearUserPref("print_printer");

    return taskReturn;
  }

  static get defaultTestPageUrl() {
    const testPath = getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "http://example.com"
    );
    return testPath + "simplifyArticleSample.html";
  }

  static createMockPaper(paperProperties = {}) {
    return Object.assign(
      {
        id: "regular",
        name: "Regular Size",
        width: 612,
        height: 792,
        unwriteableMargin: {
          marginTop: 0.1,
          marginBottom: 0.1,
          marginLeft: 0.1,
          marginRight: 0.1,
          QueryInterface: ChromeUtils.generateQI([Ci.nsIPaperMargin]),
        },
        QueryInterface: ChromeUtils.generateQI([Ci.nsIPaper]),
      },
      paperProperties
    );
  }

  // This is used only for the old print preview. For tests
  // involving the newer UI, use waitForPreview instead.
  static waitForOldPrintPreview(expectedBrowser) {
    const { PrintingParent } = ChromeUtils.import(
      "resource://gre/actors/PrintingParent.jsm"
    );

    return new Promise(resolve => {
      PrintingParent.setTestListener(browser => {
        if (browser == expectedBrowser) {
          PrintingParent.setTestListener(null);
          resolve();
        }
      });
    });
  }

  constructor(sourceBrowser) {
    this.sourceBrowser = sourceBrowser;
  }

  async startPrint() {
    this.sourceBrowser.ownerGlobal.document
      .getElementById("cmd_print")
      .doCommand();
    let dialog = await TestUtils.waitForCondition(
      () => this.dialog,
      "Wait for dialog"
    );
    await dialog._dialogReady;
    await this.win._initialized;
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
    this.win.PrintEventHandler.settings = this.win.PrintEventHandler.defaultSettings;
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
      () => file.exists(),
      "Wait for printed file",
      50
    );

    ok(file.exists(), "Printed the file");
  }

  setupMockPrint() {
    if (this.resolveShowSystemDialog) {
      throw new Error("Print already mocked");
    }

    // Create some Promises that we can resolve/reject from the test.
    let showSystemDialogPromise = new Promise((resolve, reject) => {
      this.resolveShowSystemDialog = resolve;
      this.rejectShowSystemDialog = () => {
        reject(Components.Exception("", Cr.NS_ERROR_ABORT));
      };
    });
    let printPromise = new Promise((resolve, reject) => {
      this.resolvePrint = resolve;
      this.rejectPrint = reject;
    });

    // Mock PrintEventHandler with our Promises.
    this.win.PrintEventHandler._showPrintDialog = () => showSystemDialogPromise;
    this.win.PrintEventHandler._doPrint = (bc, settings) => {
      this._printedSettings = settings;
      return printPromise;
    };
  }

  addMockPrinter(name = "Mock Printer", paperList = []) {
    let PSSVC = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(
      Ci.nsIPrintSettingsService
    );

    let defaultSettings = PSSVC.newPrintSettings;
    defaultSettings.printerName = name;
    defaultSettings.toFileName = "";
    defaultSettings.outputFormat = Ci.nsIPrintSettings.kOutputFormatNative;
    defaultSettings.printToFile = false;

    let printer = {
      name,
      supportsColor: Promise.resolve(true),
      supportsMonochrome: Promise.resolve(true),
      printerInfo: Promise.resolve({
        paperList,
        defaultSettings,
        QueryInterface: ChromeUtils.generateQI([Ci.nsIPrinterInfo]),
      }),
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
    return this._tabDialogBox.getManager();
  }

  get _dialogs() {
    return this._tabDialogBox._dialogManager._dialogs;
  }

  get dialog() {
    return this._dialogs.find(dlg =>
      dlg._box.querySelector(".printSettingsBrowser")
    );
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

  get sourceURI() {
    return this.win.PrintEventHandler.originalSourceCurrentURI;
  }

  async waitForPreview(changeFn) {
    changeFn();
    await BrowserTestUtils.waitForEvent(this.doc, "preview-updated");
  }

  async waitForSettingsEvent() {
    await BrowserTestUtils.waitForEvent(this.doc, "print-settings");
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

  assertSettingsMatch(expected) {
    let { settings } = this;
    for (let [setting, value] of Object.entries(expected)) {
      is(settings[setting], value, `${setting} matches`);
    }
  }

  assertPrintedWithSettings(expected) {
    ok(this._printedSettings, "Printed settings have been recorded");
    for (let [setting, value] of Object.entries(expected)) {
      is(
        this._printedSettings[setting],
        value,
        `${setting} matches printed setting`
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
