"use strict";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

add_task(async function testBackgroundWindow() {
  // Background navigates to http:-URL.
  // TODO bug 1286083: Disallow background navigation.
  allow_unsafe_parent_loads_when_extensions_not_remote();

  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.log("background script executed");
      window.location =
        "http://example.com/data/file_privilege_escalation.html";
    },
  });

  let awaitConsole = new Promise(resolve => {
    Services.console.registerListener(function listener(message) {
      if (/WebExt Privilege Escalation/.test(message.message)) {
        Services.console.unregisterListener(listener);
        resolve(message);
      }
    });
  });

  await extension.startup();

  let message = await awaitConsole;
  ok(
    message.message.includes(
      "WebExt Privilege Escalation: typeof(browser) = undefined"
    ),
    "Document does not have `browser` APIs."
  );

  await extension.unload();

  revert_allow_unsafe_parent_loads_when_extensions_not_remote();
});
