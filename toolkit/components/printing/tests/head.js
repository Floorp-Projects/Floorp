class PrintHelper {
  static withTestPage(testFn) {
    SpecialPowers.pushPrefEnv({
      set: [["print.tab_modal.enabled", true]],
    });

    return BrowserTestUtils.withNewTab(
      this.defaultTestPageUrl,
      async browser => {
        await testFn(new PrintHelper(browser));
        SpecialPowers.popPrefEnv();
      }
    );
  }

  static get defaultTestPageUrl() {
    const testPath = getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "http://example.com"
    );
    return testPath + "simplifyArticleSample.html";
  }

  constructor(browser) {
    this.browser = browser;
  }

  async startPrint() {
    document.getElementById("cmd_print").doCommand();
    let dialog = await TestUtils.waitForCondition(() => this._dialogs[0]);
    await dialog._ready;
  }

  async withClosingFn(closeFn) {
    let closed = this._dialogs[0]._closed;
    await closeFn();
    await closed;
  }

  assertDialogHidden() {
    is(this._dialogs.length, 0, "There are no print dialogs");
  }

  assertDialogVisible() {
    is(this._dialogs.length, 1, "There is one print dialog");
    BrowserTestUtils.is_visible(this._dialogs[0], "The dialog is visible");
  }

  get _container() {
    return this.browser.ownerGlobal.gBrowser.getBrowserContainer(this.browser);
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
