// Let's test privileged APIs
browser.runtime.sendNativeMessage("browser", "HELLO_FROM_PAGE");
