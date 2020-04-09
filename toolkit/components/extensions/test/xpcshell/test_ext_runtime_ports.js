"use strict";

add_task(async function test_port_disconnected_from_wrong_window() {
  let extensionData = {
    background() {
      let num = 0;
      let ports = {};
      let done = false;

      browser.runtime.onConnect.addListener(port => {
        num++;
        ports[num] = port;

        port.onMessage.addListener(msg => {
          browser.test.assertEq(msg, "port-2-response", "Got port 2 response");
          browser.test.sendMessage(msg + "-received");
          done = true;
        });

        port.onDisconnect.addListener(err => {
          if (port === ports[1]) {
            browser.test.log("Port 1 disconnected, sending message via port 2");
            ports[2].postMessage("port-2-msg");
          } else {
            browser.test.assertTrue(
              done,
              "Port 2 disconnected only after a full roundtrip received"
            );
          }
        });

        browser.test.sendMessage("port-connect-" + num);
      });
    },
    files: {
      "page.html": `
        <!DOCTYPE html><meta charset="utf8">
        <script src="script.js"></script>
      `,
      "script.js"() {
        let port = browser.runtime.connect();
        port.onMessage.addListener(msg => {
          browser.test.assertEq(msg, "port-2-msg", "Got message via port 2");
          port.postMessage("port-2-response");
        });
      },
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  let url = `moz-extension://${extension.uuid}/page.html`;
  await extension.startup();

  let page1 = await ExtensionTestUtils.loadContentPage(url, { extension });
  await extension.awaitMessage("port-connect-1");
  info("First page opened port 1");

  let page2 = await ExtensionTestUtils.loadContentPage(url, { extension });
  await extension.awaitMessage("port-connect-2");
  info("Second page opened port 2");

  info("Closing the first page should not close port 2");
  await page1.close();
  await extension.awaitMessage("port-2-response-received");
  info("Roundtrip message through port 2 received");

  await page2.close();
  await extension.unload();
});
