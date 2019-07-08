"use strict";

const { ExtensionUtils } = ChromeUtils.import(
  "resource://gre/modules/ExtensionUtils.jsm"
);

const proxy = createHttpServer();

add_task(async function test_speculative_connect() {
  function background() {
    // Handle the proxy request.
    browser.proxy.onRequest.addListener(
      details => {
        browser.test.log(`onRequest ${JSON.stringify(details)}`);
        browser.test.assertEq(
          details.type,
          "speculative",
          "Should have seen a speculative proxy request."
        );
        return [{ type: "direct" }];
      },
      { urls: ["<all_urls>"], types: ["speculative"] }
    );
  }

  let handlingExt = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["proxy", "<all_urls>"],
    },
    background: `(${background})()`,
  });

  Services.prefs.setBoolPref("network.http.debug-observations", true);

  await handlingExt.startup();

  let notificationPromise = ExtensionUtils.promiseObserved(
    "speculative-connect-request"
  );

  let uri = Services.io.newURI(
    `http://${proxy.identity.primaryHost}:${proxy.identity.primaryPort}`
  );
  Services.io.speculativeConnect(
    uri,
    Services.scriptSecurityManager.getSystemPrincipal(),
    null
  );
  await notificationPromise;

  await handlingExt.unload();
});
