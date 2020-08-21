const PRINT_DOCUMENT_URI = "chrome://global/content/print.html";

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
  }

  async withClosingFn(closeFn) {
    let { dialog } = this;
    await closeFn();
    await dialog._closingPromise;
  }

  async closeDialog() {
    await this.withClosingFn(() => this.dialog.close());
  }

  assertDialogHidden() {
    is(this._dialogs.length, 0, "There are no print dialogs");
  }

  assertDialogVisible() {
    is(this._dialogs.length, 1, "There is one print dialog");
    BrowserTestUtils.is_visible(this.dialog._box, "The dialog is visible");
  }

  get _tabDialogBox() {
    return this.sourceBrowser.ownerGlobal.gBrowser.getTabDialogBox(
      this.sourceBrowser
    );
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
    let dialog = this.dialog;
    ok(dialog, "The dialog exists");
    return dialog._frame;
  }

  get doc() {
    return this._printBrowser.contentDocument;
  }

  get win() {
    return this._printBrowser.contentWindow;
  }
}
