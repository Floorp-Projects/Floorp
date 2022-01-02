// This message should not be handled
browser.runtime.sendNativeMessage("badNativeApi", "errorerrorerror");

async function runTest() {
  const response = await browser.runtime.sendNativeMessage(
    "browser",
    "testContentBrowserMessage"
  );

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
    browser.runtime.sendNativeMessage("browser", { type: "portDisconnected" })
  );

  port.postMessage("testContentPortMessage");
}

runTest();
