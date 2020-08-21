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
    let dialog = await TestUtils.waitForCondition(() => this._dialogs[0]);
    await dialog._dialog._dialogReady;
  }

  async withClosingFn(closeFn) {
    await closeFn();
    await this._dialogs[0]._dialog._closingPromise;
  }

  async closeDialog() {
    await this.withClosingFn(() => this._dialogs[0]._dialog.close());
  }

  assertDialogHidden() {
    is(this._dialogs.length, 0, "There are no print dialogs");
  }

  assertDialogVisible() {
    is(this._dialogs.length, 1, "There is one print dialog");
    BrowserTestUtils.is_visible(this._dialogs[0], "The dialog is visible");
  }

  get _container() {
    return this.sourceBrowser.ownerGlobal.gBrowser.getBrowserContainer(
      this.sourceBrowser
    );
  }

  get _dialogs() {
    return this._container.querySelectorAll(".printDialogContainer");
  }

  get _printBrowser() {
    let dialogs = this._dialogs;
    is(dialogs.length, 1, "There's one dialog");
    let frame = dialogs[0].querySelector(".dialogFrame");
    ok(frame, "Found the print dialog frame");
    return frame;
  }

  get doc() {
    return this._printBrowser.contentDocument;
  }

  get win() {
    return this._printBrowser.contentWindow;
  }
}
