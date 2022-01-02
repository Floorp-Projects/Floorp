browser.runtime.sendNativeMessage("badNativeApi", "errorerrorerror");

async function runTest() {
  await browser.runtime.sendNativeMessage(
    "browser",
    "testContentBrowserMessage"
  );
  browser.runtime.connectNative("browser");
}

runTest();
