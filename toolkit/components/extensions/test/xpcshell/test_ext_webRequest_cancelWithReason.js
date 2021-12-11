"use strict";

const server = createHttpServer();
const gServerUrl = `http://localhost:${server.identity.primaryPort}`;

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write("ok");
});

add_task(async function test_cancel_with_reason() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: { gecko: { id: "cancel@test" } },
      permissions: ["webRequest", "webRequestBlocking", "<all_urls>"],
    },

    background() {
      browser.webRequest.onBeforeRequest.addListener(
        () => {
          return { cancel: true };
        },
        { urls: ["*://*/*"] },
        ["blocking"]
      );
    },
  });
  await ext.startup();

  let data = await new Promise(resolve => {
    let ssm = Services.scriptSecurityManager;

    let channel = NetUtil.newChannel({
      uri: `${gServerUrl}/dummy`,
      loadingPrincipal: ssm.createContentPrincipalFromOrigin(
        "http://localhost"
      ),
      contentPolicyType: Ci.nsIContentPolicy.TYPE_XMLHTTPREQUEST,
      securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    });

    channel.asyncOpen({
      QueryInterface: ChromeUtils.generateQI(["nsIStreamListener"]),

      onStartRequest(request) {},

      onStopRequest(request, statusCode) {
        let properties = request.QueryInterface(Ci.nsIPropertyBag);
        let id = properties.getProperty("cancelledByExtension");
        let reason = request.loadInfo.requestBlockingReason;
        resolve({ reason, id });
      },

      onDataAvailable() {},
    });
  });

  Assert.equal(
    Ci.nsILoadInfo.BLOCKING_REASON_EXTENSION_WEBREQUEST,
    data.reason,
    "extension cancelled request"
  );
  Assert.equal(
    ext.id,
    data.id,
    "extension id attached to channel property bag"
  );
  await ext.unload();
});
