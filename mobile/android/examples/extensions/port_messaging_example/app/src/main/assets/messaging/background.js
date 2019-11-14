// Establish connection with app
let port = browser.runtime.connectNative("browser");
port.onMessage.addListener(response => {
    // Let's just echo the message back
    port.postMessage(`Received: ${JSON.stringify(response)}`);
});
port.postMessage("Hello from WebExtension!");
