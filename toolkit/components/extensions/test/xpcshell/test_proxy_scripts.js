"use strict";

/* eslint no-unused-vars: ["error", {"args": "none", "varsIgnorePattern": "^(FindProxyForURL)$"}] */

Cu.import("resource://gre/modules/Extension.jsm");
Cu.import("resource://gre/modules/ProxyScriptContext.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gProxyService",
                                   "@mozilla.org/network/protocol-proxy-service;1",
                                   "nsIProtocolProxyService");

async function testProxyScript(options, expected = {}) {
  let scriptData = String(options.scriptData).replace(/^.*?\{([^]*)\}$/, "$1");
  let extensionData = {
    background() {
      browser.test.onMessage.addListener((message, data) => {
        if (message === "runtime-message") {
          browser.runtime.onMessage.addListener((msg, sender, respond) => {
            if (msg === "finish-from-pac-script") {
              browser.test.notifyPass("proxy");
              return Promise.resolve(msg);
            }
          });
          browser.runtime.sendMessage(data, {toProxyScript: true}).then(response => {
            browser.test.sendMessage("runtime-message-sent");
          });
        } else if (message === "finish-from-xpcshell-test") {
          browser.test.notifyPass("proxy");
        }
      });
    },
    files: {
      "proxy.js": scriptData,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  let extension_internal = extension.extension;

  await extension.startup();

  let script = new ProxyScriptContext(extension_internal, extension_internal.getURL("proxy.js"));

  try {
    await script.load();
  } catch (error) {
    equal(error, expected.error, "Expected error received");
    script.unload();
    await extension.unload();
    return;
  }

  if (options.runtimeMessage) {
    extension.sendMessage("runtime-message", options.runtimeMessage);
    await extension.awaitMessage("runtime-message-sent");
  } else {
    extension.sendMessage("finish-from-xpcshell-test");
  }

  await extension.awaitFinish("proxy");

  let proxyInfo = await new Promise((resolve, reject) => {
    let channel = NetUtil.newChannel({
      uri: "http://www.mozilla.org/",
      loadUsingSystemPrincipal: true,
    });

    gProxyService.asyncResolve(channel, 0, {
      onProxyAvailable(req, uri, pi, status) {
        resolve(pi);
      },
    });
  });

  if (!proxyInfo) {
    equal(proxyInfo, expected.proxyInfo, "Expected proxyInfo to be null");
  } else {
    let expectedProxyInfo = expected.proxyInfo;
    for (let proxy = proxyInfo; proxy; proxy = proxy.failoverProxy) {
      equal(proxy.host, expectedProxyInfo.host, `Expected proxy host to be ${expectedProxyInfo.host}`);
      equal(proxy.port, expectedProxyInfo.port, `Expected proxy port to be ${expectedProxyInfo.port}`);
      equal(proxy.type, expectedProxyInfo.type, `Expected proxy type to be ${expectedProxyInfo.type}`);
      expectedProxyInfo = expectedProxyInfo.failoverProxy;
    }
  }

  await extension.unload();
  script.unload();
}

add_task(async function testUndefinedFindProxyForURL() {
  await testProxyScript({
    scriptData() { },
  }, {
    proxyInfo: null,
  });
});

add_task(async function testWrongTypeForFindProxyForURL() {
  await testProxyScript({
    scriptData() {
      let FindProxyForURL = "foo";
    },
  }, {
    proxyInfo: null,
  });
});

add_task(async function testInvalidReturnTypeForFindProxyForURL() {
  await testProxyScript({
    scriptData() {
      function FindProxyForURL(url, host) {
        return -1;
      }
    },
  }, {
    proxyInfo: null,
  });
});

add_task(async function testSimpleProxyScript() {
  await testProxyScript({
    scriptData() {
      function FindProxyForURL(url, host) {
        if (host === "www.mozilla.org") {
          return "DIRECT";
        }
      }
    },
  }, {
    proxyInfo: null,
  });
});

add_task(async function testRuntimeErrorInProxyScript() {
  await testProxyScript({
    scriptData() {
      function FindProxyForURL(url, host) {
        return RUNTIME_ERROR; // eslint-disable-line no-undef
      }
    },
  }, {
    proxyInfo: null,
  });
});

add_task(async function testProxyScriptWithUnexpectedReturnType() {
  await testProxyScript({
    scriptData() {
      function FindProxyForURL(url, host) {
        return "UNEXPECTED 1.2.3.4:8080";
      }
    },
  }, {
    proxyInfo: null,
  });
});

add_task(async function testSocksReturnType() {
  await testProxyScript({
    scriptData() {
      function FindProxyForURL(url, host) {
        return "SOCKS foo.bar:1080";
      }
    },
  }, {
    proxyInfo: {
      host: "foo.bar",
      port: "1080",
      type: "socks",
      failoverProxy: null,
    },
  });
});

add_task(async function testSocks4ReturnType() {
  await testProxyScript({
    scriptData() {
      function FindProxyForURL(url, host) {
        return "SOCKS4 1.2.3.4:1080";
      }
    },
  }, {
    proxyInfo: {
      host: "1.2.3.4",
      port: "1080",
      type: "socks4",
      failoverProxy: null,
    },
  });
});

add_task(async function testSocksReturnTypeWithHostCheck() {
  await testProxyScript({
    scriptData() {
      function FindProxyForURL(url, host) {
        if (host === "www.mozilla.org") {
          return "SOCKS 4.4.4.4:9002";
        }
      }
    },
  }, {
    proxyInfo: {
      host: "4.4.4.4",
      port: "9002",
      type: "socks",
      failoverProxy: null,
    },
  });
});

add_task(async function testProxyReturnType() {
  await testProxyScript({
    scriptData() {
      function FindProxyForURL(url, host) {
        return "PROXY 1.2.3.4:8080";
      }
    },
  }, {
    proxyInfo: {
      host: "1.2.3.4",
      port: "8080",
      type: "http",
      failoverProxy: null,
    },
  });
});

add_task(async function testUnusualWhitespaceForFindProxyForURL() {
  await testProxyScript({
    scriptData() {
      function FindProxyForURL(url, host) {
        return "   PROXY    1.2.3.4:8080      ";
      }
    },
  }, {
    proxyInfo: {
      host: "1.2.3.4",
      port: "8080",
      type: "http",
      failoverProxy: null,
    },
  });
});

add_task(async function testInvalidProxyScriptIgnoresFailover() {
  await testProxyScript({
    scriptData() {
      function FindProxyForURL(url, host) {
        return "PROXY 1.2.3.4:8080; UNEXPECTED; SOCKS 1.2.3.4:8080";
      }
    },
  }, {
    proxyInfo: {
      host: "1.2.3.4",
      port: "8080",
      type: "http",
      failoverProxy: null,
    },
  });
});

add_task(async function testProxyScriptWithValidFailovers() {
  await testProxyScript({
    scriptData() {
      function FindProxyForURL(url, host) {
        return "PROXY 1.2.3.4:8080; SOCKS 4.4.4.4:9000; DIRECT";
      }
    },
  }, {
    proxyInfo: {
      host: "1.2.3.4",
      port: "8080",
      type: "http",
      failoverProxy: {
        host: "4.4.4.4",
        port: "9000",
        type: "socks",
        failoverProxy: null,
      },
    },
  });
});

add_task(async function testProxyScriptWithAnInvalidFailover() {
  await testProxyScript({
    scriptData() {
      function FindProxyForURL(url, host) {
        return "PROXY 1.2.3.4:8080; INVALID 1.2.3.4:9090; SOCKS 4.4.4.4:9000; DIRECT";
      }
    },
  }, {
    proxyInfo: {
      host: "1.2.3.4",
      port: "8080",
      type: "http",
      failoverProxy: null,
    },
  });
});

add_task(async function testProxyScriptWithEmptyFailovers() {
  await testProxyScript({
    scriptData() {
      function FindProxyForURL(url, host) {
        return ";;;;;PROXY 1.2.3.4:8080";
      }
    },
  }, {
    proxyInfo: null,
  });
});

add_task(async function testProxyScriptWithInvalidReturn() {
  await testProxyScript({
    scriptData() {
      function FindProxyForURL(url, host) {
        return "SOCKS :8080;";
      }
    },
  }, {
    proxyInfo: null,
  });
});

add_task(async function testProxyScriptWithRuntimeUpdate() {
  await testProxyScript({
    scriptData() {
      let settings = {};
      function FindProxyForURL(url, host) {
        if (settings.host === "www.mozilla.org") {
          return "PROXY 1.2.3.4:8080;";
        }
        return "DIRECT";
      }
      browser.runtime.onMessage.addListener((msg, sender, respond) => {
        if (msg.host) {
          settings.host = msg.host;
          browser.runtime.sendMessage("finish-from-pac-script");
          return Promise.resolve(msg);
        }
      });
    },
    runtimeMessage: {
      host: "www.mozilla.org",
    },
  }, {
    proxyInfo: {
      host: "1.2.3.4",
      port: "8080",
      type: "http",
      failoverProxy: null,
    },
  });
});
