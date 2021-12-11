browser.runtime.sendNativeMessage("browser1", "HELLO_FROM_PAGE_1");
browser.runtime.sendNativeMessage("browser2", "HELLO_FROM_PAGE_2");

const port = browser.runtime.connectNative("browser1");
port.postMessage("HELLO_FROM_PORT");
