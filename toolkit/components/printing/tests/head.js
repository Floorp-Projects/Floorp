const PRINT_DOCUMENT_URI = "chrome://global/content/print.html";
const { MockFilePicker } = SpecialPowers;

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

    return taskReturn;
  }

  static get defaultTestPageUrl() {
    const testPath = getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "http://example.com"
    );
    return testPath + "simplifyArticleSample.html";
  }

  constructor(sourceBrowser) {
    this.sourceBrowser = sourceBrowser;
  }

  async startPrint() {
    document.getElementById("cmd_print").doCommand();
    let dialog = await TestUtils.waitForCondition(
      () => this.dialog,
      "Wait for dialog"
    );
    await dialog._dialogReady;
    await this.win._initialized;
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

  async closeDialog() {
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
  }

  async setupMockPrint() {
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
    this.win.PrintEventHandler._doPrint = () => printPromise;
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

  click(el) {
    EventUtils.synthesizeMouseAtCenter(el, {}, this.win);
  }

  text(el, text) {
    this.click(el);
    el.value = "";
    EventUtils.sendString(text, this.win);
  }

  async openMoreSettings() {
    this.click(this.get("more-settings").firstElementChild);
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

  awaitAnimationFrame() {
    return new Promise(resolve => this.win.requestAnimationFrame(resolve));
  }

  mockFilePicker(file) {
    MockFilePicker.init(window);
    MockFilePicker.setFiles([file]);
    registerCleanupFunction(() => MockFilePicker.cleanup());
  }
}
