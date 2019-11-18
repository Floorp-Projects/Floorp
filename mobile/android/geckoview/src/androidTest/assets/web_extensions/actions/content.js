const port = browser.runtime.connectNative("browser");
port.onMessage.addListener(message => {
  browser.runtime.sendMessage(message);
});
