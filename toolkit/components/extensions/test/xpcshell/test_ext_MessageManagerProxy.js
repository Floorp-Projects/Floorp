/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { MessageManagerProxy } = ChromeUtils.import(
  "resource://gre/modules/MessageManagerProxy.jsm"
);
const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);

class TestMessageManagerProxy extends MessageManagerProxy {
  constructor(contentPage, identifier) {
    super(contentPage.browser);
    this.identifier = identifier;
    this.contentPage = contentPage;
    this.deferred = null;
  }

  // Registers message listeners. Call dispose() once you've finished.
  async setupPingPongListeners() {
    await this.contentPage.loadFrameScript(`() => {
      this.addMessageListener("test:MessageManagerProxy:Ping", ({data}) => {
        this.sendAsyncMessage("test:MessageManagerProxy:Pong", "${this.identifier}:" + data);
      });
    }`);

    // Register the listener here instead of during testPingPong, to make sure
    // that the listener is correctly registered during the whole test.
    this.addMessageListener("test:MessageManagerProxy:Pong", event => {
      ok(
        this.deferred,
        `[${this.identifier}] expected to be waiting for ping-pong`
      );
      this.deferred.resolve(event.data);
      this.deferred = null;
    });
  }

  async testPingPong(description) {
    equal(this.deferred, null, "should not be waiting for a message");
    this.deferred = PromiseUtils.defer();
    this.sendAsyncMessage("test:MessageManagerProxy:Ping", description);
    let result = await this.deferred.promise;
    equal(result, `${this.identifier}:${description}`, "Expected ping-pong");
  }
}

// Tests that MessageManagerProxy continues to proxy messages after docshells
// have been swapped.
add_task(async function test_message_after_swapdocshells() {
  let page1 = await ExtensionTestUtils.loadContentPage("about:blank");
  let page2 = await ExtensionTestUtils.loadContentPage("about:blank");

  let testProxyOne = new TestMessageManagerProxy(page1, "page1");
  let testProxyTwo = new TestMessageManagerProxy(page2, "page2");

  await testProxyOne.setupPingPongListeners();
  await testProxyTwo.setupPingPongListeners();

  await testProxyOne.testPingPong("after setup (to 1)");
  await testProxyTwo.testPingPong("after setup (to 2)");

  page1.browser.swapDocShells(page2.browser);

  await testProxyOne.testPingPong("after docshell swap (to 1)");
  await testProxyTwo.testPingPong("after docshell swap (to 2)");

  // Swap again to verify that listeners are repeatedly moved when needed.
  page1.browser.swapDocShells(page2.browser);

  await testProxyOne.testPingPong("after another docshell swap (to 1)");
  await testProxyTwo.testPingPong("after another docshell swap (to 2)");

  // Verify that dispose() works regardless of the browser's validity.
  await testProxyOne.dispose();
  await page1.close();
  await page2.close();
  await testProxyTwo.dispose();
});
