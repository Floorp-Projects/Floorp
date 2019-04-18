browser.runtime
  .sendNativeMessage("badNativeApi", "errorerrorerror")
  // This message should not be handled
  .catch(runTest);

async function runTest() {
  const response = await browser.runtime
    .sendNativeMessage("browser", "testBackgroundBrowserMessage");

  browser.runtime.sendNativeMessage("browser", `response: ${response}`);

  const port = browser.runtime.connectNative("browser");
  port.onMessage.addListener(response => {
    if (response.action === "disconnect") {
      port.disconnect();
      return;
    }

    port.postMessage(`response: ${response.message}`);
  });

  port.onDisconnect.addListener(() =>
      browser.runtime.sendNativeMessage("browser", {type: "portDisconnected"}));

  port.postMessage("testBackgroundPortMessage");
}
