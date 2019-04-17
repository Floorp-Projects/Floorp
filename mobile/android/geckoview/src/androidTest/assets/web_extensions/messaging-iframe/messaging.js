browser.runtime
  .sendNativeMessage("badNativeApi", "errorerrorerror")
  // This message should not be handled
  .catch(runTest);

async function runTest() {
  await browser.runtime
    .sendNativeMessage("browser", "testContentBrowserMessage");
  browser.runtime.connectNative("browser");
}
