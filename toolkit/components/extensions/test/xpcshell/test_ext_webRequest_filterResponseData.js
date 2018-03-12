"use strict";

XPCOMUtils.defineLazyServiceGetter(this, "proxyService",
                                   "@mozilla.org/network/protocol-proxy-service;1",
                                   "nsIProtocolProxyService");

const server = createHttpServer();
const gHost = "localhost";
const gPort = server.identity.primaryPort;

const HOSTS = new Set([
  "example.com",
  "example.org",
  "example.net",
]);

for (let host of HOSTS) {
  server.identity.add("http", host, 80);
}

const proxyFilter = {
  proxyInfo: proxyService.newProxyInfo("http", gHost, gPort, 0, 4096, null),

  applyFilter(service, channel, defaultProxyInfo, callback) {
    if (HOSTS.has(channel.URI.host)) {
      callback.onProxyFilterResult(this.proxyInfo);
    } else {
      callback.onProxyFilterResult(defaultProxyInfo);
    }
  },
};

proxyService.registerChannelFilter(proxyFilter, 0);
registerCleanupFunction(() => {
  proxyService.unregisterChannelFilter(proxyFilter);
});

server.registerPathHandler("/redirect", (request, response) => {
  let params = new URLSearchParams(request.queryString);
  response.setStatusLine(request.httpVersion, 302, "Moved Temporarily");
  response.setHeader("Location", params.get("redirect_uri"));
  response.setHeader("Access-Control-Allow-Origin", "*");
});

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Access-Control-Allow-Origin", "*");
  response.write("ok");
});

Cu.importGlobalProperties(["fetch"]);

add_task(async function() {
  const {fetch} = Cu.Sandbox("http://example.com/", {wantGlobalProperties: ["fetch"]});

  let extension = ExtensionTestUtils.loadExtension({
    background() {
      let pending = [];

      browser.webRequest.onBeforeRequest.addListener(
        data => {
          let filter = browser.webRequest.filterResponseData(data.requestId);

          let url = new URL(data.url);

          if (url.searchParams.get("redirect_uri")) {
            pending.push(
              new Promise(resolve => { filter.onerror = resolve; }).then(() => {
                browser.test.assertEq("Channel redirected", filter.error,
                                      "Got correct error for redirected filter");
              }));
          }

          filter.onstart = () => {
            filter.write(new TextEncoder().encode(data.url));
          };
          filter.ondata = event => {
            let str = new TextDecoder().decode(event.data);
            browser.test.assertEq("ok", str, `Got unfiltered data for ${data.url}`);
          };
          filter.onstop = () => {
            filter.close();
          };
        }, {
          urls: ["<all_urls>"],
        },
        ["blocking"]);

      browser.test.onMessage.addListener(async msg => {
        if (msg === "done") {
          await Promise.all(pending);
          browser.test.notifyPass("stream-filter");
        }
      });
    },

    manifest: {
      permissions: [
        "webRequest",
        "webRequestBlocking",
        "http://example.com/",
        "http://example.org/",
      ],
    },
  });

  await extension.startup();

  let results = [
    ["http://example.com/dummy", "http://example.com/dummy"],
    ["http://example.org/dummy", "http://example.org/dummy"],
    ["http://example.net/dummy", "ok"],
    ["http://example.com/redirect?redirect_uri=http://example.com/dummy", "http://example.com/dummy"],
    ["http://example.com/redirect?redirect_uri=http://example.org/dummy", "http://example.org/dummy"],
    ["http://example.com/redirect?redirect_uri=http://example.net/dummy", "ok"],
    ["http://example.net/redirect?redirect_uri=http://example.com/dummy", "http://example.com/dummy"],
  ].map(async ([url, expectedResponse]) => {
    let resp = await fetch(url);
    let text = await resp.text();
    equal(text, expectedResponse, `Expected response for ${url}`);
  });

  await Promise.all(results);

  extension.sendMessage("done");
  await extension.awaitFinish("stream-filter");
  await extension.unload();
});
