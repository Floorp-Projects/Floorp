"use strict";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gProxyService",
  "@mozilla.org/network/protocol-proxy-service;1",
  "nsIProtocolProxyService"
);

const TRANSPARENT_PROXY_RESOLVES_HOST =
  Ci.nsIProxyInfo.TRANSPARENT_PROXY_RESOLVES_HOST;

let extension;
add_task(async function setup() {
  let extensionData = {
    manifest: {
      permissions: ["proxy", "<all_urls>"],
    },
    background() {
      let settings = { proxy: null };

      browser.proxy.onError.addListener(error => {
        browser.test.log(`error received ${error.message}`);
        browser.test.sendMessage("proxy-error-received", error);
      });
      browser.test.onMessage.addListener((message, data) => {
        if (message === "set-proxy") {
          settings.proxy = data.proxy;
          browser.test.sendMessage("proxy-set", settings.proxy);
        }
      });
      browser.proxy.onRequest.addListener(
        () => {
          return settings.proxy;
        },
        { urls: ["<all_urls>"] }
      );
    },
  };
  extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
});

async function setupProxyResult(proxy) {
  extension.sendMessage("set-proxy", { proxy });
  let proxyInfoSent = await extension.awaitMessage("proxy-set");
  deepEqual(
    proxyInfoSent,
    proxy,
    "got back proxy data from the proxy listener"
  );
}

async function testProxyResolution(test) {
  let { uri, proxy, expected } = test;
  let errorMsg;
  if (expected.error) {
    errorMsg = extension.awaitMessage("proxy-error-received");
  }
  let proxyInfo = await new Promise((resolve, reject) => {
    let channel = NetUtil.newChannel({
      uri,
      loadUsingSystemPrincipal: true,
    });

    gProxyService.asyncResolve(channel, 0, {
      onProxyAvailable(req, uri, pi, status) {
        resolve(pi && pi.QueryInterface(Ci.nsIProxyInfo));
      },
    });
  });

  let expectedProxyInfo = expected.proxyInfo;
  if (expected.error) {
    equal(proxyInfo, null, "Expected proxyInfo to be null");
    equal((await errorMsg).message, expected.error, "error received");
  } else if (proxy == null) {
    equal(proxyInfo, expectedProxyInfo, "proxy is direct");
  } else {
    for (
      let proxyUsed = proxyInfo;
      proxyUsed;
      proxyUsed = proxyUsed.failoverProxy
    ) {
      let {
        type,
        host,
        port,
        username,
        password,
        proxyDNS,
        failoverTimeout,
      } = expectedProxyInfo;
      equal(proxyUsed.host, host, `Expected proxy host to be ${host}`);
      equal(proxyUsed.port, port, `Expected proxy port to be ${port}`);
      equal(proxyUsed.type, type, `Expected proxy type to be ${type}`);
      // May be null or undefined depending on use of newProxyInfoWithAuth or newProxyInfo
      equal(
        proxyUsed.username || "",
        username || "",
        `Expected proxy username to be ${username}`
      );
      equal(
        proxyUsed.password || "",
        password || "",
        `Expected proxy password to be ${password}`
      );
      equal(
        proxyUsed.flags,
        proxyDNS == undefined ? 0 : proxyDNS,
        `Expected proxyDNS to be ${proxyDNS}`
      );
      // Default timeout is 10
      equal(
        proxyUsed.failoverTimeout,
        failoverTimeout || 10,
        `Expected failoverTimeout to be ${failoverTimeout}`
      );
      expectedProxyInfo = expectedProxyInfo.failoverProxy;
    }
  }
}

add_task(async function test_proxyInfo_results() {
  let tests = [
    {
      proxy: 5,
      expected: {
        error: "ProxyInfoData: proxyData must be an object or array of objects",
      },
    },
    {
      proxy: "INVALID",
      expected: {
        error: "ProxyInfoData: proxyData must be an object or array of objects",
      },
    },
    {
      proxy: {
        type: "socks",
      },
      expected: {
        error: 'ProxyInfoData: Invalid proxy server host: "undefined"',
      },
    },
    {
      proxy: [
        {
          type: "pptp",
          host: "foo.bar",
          port: 1080,
          username: "mungosantamaria",
          password: "pass123",
          proxyDNS: true,
          failoverTimeout: 3,
        },
        {
          type: "http",
          host: "192.168.1.1",
          port: 1128,
          username: "mungosantamaria",
          password: "word321",
        },
      ],
      expected: {
        error: 'ProxyInfoData: Invalid proxy server type: "pptp"',
      },
    },
    {
      proxy: [
        {
          type: "http",
          host: "foo.bar",
          port: 65536,
          username: "mungosantamaria",
          password: "pass123",
          proxyDNS: true,
          failoverTimeout: 3,
        },
        {
          type: "http",
          host: "192.168.1.1",
          port: 3128,
          username: "mungosantamaria",
          password: "word321",
        },
      ],
      expected: {
        error:
          "ProxyInfoData: Proxy server port 65536 outside range 1 to 65535",
      },
    },
    {
      proxy: [
        {
          type: "http",
          host: "foo.bar",
          port: 3128,
          proxyAuthorizationHeader: "test",
        },
      ],
      expected: {
        error: 'ProxyInfoData: ProxyAuthorizationHeader requires type "https"',
      },
    },
    {
      proxy: [
        {
          type: "http",
          host: "foo.bar",
          port: 3128,
          connectionIsolationKey: 1234,
        },
      ],
      expected: {
        error: 'ProxyInfoData: Invalid proxy connection isolation key: "1234"',
      },
    },
    {
      proxy: [{ type: "direct" }],
      expected: {
        proxyInfo: null,
      },
    },
    {
      proxy: {
        host: "1.2.3.4",
        port: "8080",
        type: "http",
        failoverProxy: null,
      },
      expected: {
        proxyInfo: {
          host: "1.2.3.4",
          port: "8080",
          type: "http",
          failoverProxy: null,
        },
      },
    },
    {
      uri: "ftp://mozilla.org",
      proxy: {
        host: "1.2.3.4",
        port: "8180",
        type: "http",
        failoverProxy: null,
      },
      expected: {
        proxyInfo: {
          host: "1.2.3.4",
          port: "8180",
          type: "http",
          failoverProxy: null,
        },
      },
    },
    {
      proxy: {
        host: "2.3.4.5",
        port: "8181",
        type: "http",
        failoverProxy: null,
      },
      expected: {
        proxyInfo: {
          host: "2.3.4.5",
          port: "8181",
          type: "http",
          failoverProxy: null,
        },
      },
    },
    {
      proxy: {
        host: "1.2.3.4",
        port: "8080",
        type: "http",
        failoverProxy: {
          host: "4.4.4.4",
          port: "9000",
          type: "socks",
          failoverProxy: {
            type: "direct",
            host: null,
            port: -1,
          },
        },
      },
      expected: {
        proxyInfo: {
          host: "1.2.3.4",
          port: "8080",
          type: "http",
          failoverProxy: {
            host: "4.4.4.4",
            port: "9000",
            type: "socks",
            failoverProxy: {
              type: "direct",
              host: null,
              port: -1,
            },
          },
        },
      },
    },
    {
      proxy: [{ type: "http", host: "foo.bar", port: 3128 }],
      expected: {
        proxyInfo: {
          host: "foo.bar",
          port: "3128",
          type: "http",
        },
      },
    },
    {
      proxy: {
        host: "foo.bar",
        port: "1080",
        type: "socks",
      },
      expected: {
        proxyInfo: {
          host: "foo.bar",
          port: "1080",
          type: "socks",
        },
      },
    },
    {
      proxy: {
        host: "foo.bar",
        port: "1080",
        type: "socks4",
      },
      expected: {
        proxyInfo: {
          host: "foo.bar",
          port: "1080",
          type: "socks4",
        },
      },
    },
    {
      proxy: [{ type: "https", host: "foo.bar", port: 3128 }],
      expected: {
        proxyInfo: {
          host: "foo.bar",
          port: "3128",
          type: "https",
        },
      },
    },
    {
      proxy: [
        {
          type: "socks",
          host: "foo.bar",
          port: 1080,
          username: "mungo",
          password: "santamaria123",
          proxyDNS: true,
          failoverTimeout: 5,
        },
      ],
      expected: {
        proxyInfo: {
          type: "socks",
          host: "foo.bar",
          port: 1080,
          username: "mungo",
          password: "santamaria123",
          failoverTimeout: 5,
          failoverProxy: null,
          proxyDNS: TRANSPARENT_PROXY_RESOLVES_HOST,
        },
      },
    },
    {
      proxy: [
        {
          type: "socks",
          host: "foo.bar",
          port: 1080,
          username: "johnsmith",
          password: "pass123",
          proxyDNS: true,
          failoverTimeout: 3,
        },
        { type: "http", host: "192.168.1.1", port: 3128 },
        { type: "https", host: "192.168.1.2", port: 1121, failoverTimeout: 1 },
        {
          type: "socks",
          host: "192.168.1.3",
          port: 1999,
          proxyDNS: true,
          username: "mungosantamaria",
          password: "foobar",
        },
      ],
      expected: {
        proxyInfo: {
          type: "socks",
          host: "foo.bar",
          port: 1080,
          proxyDNS: true,
          username: "johnsmith",
          password: "pass123",
          failoverTimeout: 3,
          failoverProxy: {
            host: "192.168.1.1",
            port: 3128,
            type: "http",
            failoverProxy: {
              host: "192.168.1.2",
              port: 1121,
              type: "https",
              failoverTimeout: 1,
              failoverProxy: {
                host: "192.168.1.3",
                port: 1999,
                type: "socks",
                proxyDNS: TRANSPARENT_PROXY_RESOLVES_HOST,
                username: "mungosantamaria",
                password: "foobar",
                failoverProxy: {
                  type: "direct",
                },
              },
            },
          },
        },
      },
    },
    {
      proxy: [
        {
          type: "https",
          host: "foo.bar",
          port: 3128,
          proxyAuthorizationHeader: "test",
          connectionIsolationKey: "key",
        },
      ],
      expected: {
        proxyInfo: {
          host: "foo.bar",
          port: "3128",
          type: "https",
          proxyAuthorizationHeader: "test",
          connectionIsolationKey: "key",
        },
      },
    },
  ];
  for (let test of tests) {
    await setupProxyResult(test.proxy);
    if (!test.uri) {
      test.uri = "http://www.mozilla.org/";
    }
    await testProxyResolution(test);
  }
});

add_task(async function shutdown() {
  await extension.unload();
});
