const port = browser.runtime.connectNative("browser");

async function apiCall(message) {
  const { type, since, removalOptions, dataTypes } = message;
  switch (type) {
    case "clear-downloads":
      await browser.browsingData.removeDownloads({ since });
      break;
    case "clear-form-data":
      await browser.browsingData.removeFormData({ since });
      break;
    case "clear-history":
      await browser.browsingData.removeHistory({ since });
      break;
    case "clear-passwords":
      await browser.browsingData.removePasswords({ since });
      break;
    case "clear":
      await browser.browsingData.remove(removalOptions, dataTypes);
      break;
    case "get-settings":
      return browser.browsingData.settings();
  }
  return null;
}

port.onMessage.addListener(async message => {
  const { uuid } = message;
  try {
    const result = await apiCall(message);
    port.postMessage({
      type: "response",
      result,
      uuid,
    });
  } catch (exception) {
    const { message } = exception;
    port.postMessage({
      type: "error",
      error: message,
      uuid,
    });
  }
});
