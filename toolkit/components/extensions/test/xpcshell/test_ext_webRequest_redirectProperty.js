"use strict";

const server = createHttpServer();
const gServerUrl = `http://localhost:${server.identity.primaryPort}`;

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write("ok");
});

add_task(async function test_redirect_property() {
  function background(serverUrl) {
    browser.webRequest.onBeforeRequest.addListener(
      () => {
        return { redirectUrl: `${serverUrl}/dummy` };
      },
      { urls: ["*://localhost/*"] },
      ["blocking"]
    );
  }

  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: { gecko: { id: "redirect@test" } },
      permissions: ["webRequest", "webRequestBlocking", "<all_urls>"],
    },
    background: `(${background})("${gServerUrl}")`,
  });
  await ext.startup();

  let data = await new Promise(resolve => {
    let ssm = Services.scriptSecurityManager;

    let channel = NetUtil.newChannel({
      uri: `${gServerUrl}/redirect`,
      loadingPrincipal:
        ssm.createContentPrincipalFromOrigin("http://localhost"),
      contentPolicyType: Ci.nsIContentPolicy.TYPE_XMLHTTPREQUEST,
      securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    });

    channel.asyncOpen({
      QueryInterface: ChromeUtils.generateQI(["nsIStreamListener"]),

      onStartRequest() {},

      onStopRequest(request) {
        let properties = request.QueryInterface(Ci.nsIPropertyBag);
        let id = properties.getProperty("redirectedByExtension");
        resolve({ id, url: request.QueryInterface(Ci.nsIChannel).URI.spec });
      },

      onDataAvailable() {},
    });
  });

  Assert.equal(`${gServerUrl}/dummy`, data.url, "request redirected");
  Assert.equal(
    ext.id,
    data.id,
    "extension id attached to channel property bag"
  );
  await ext.unload();
});
