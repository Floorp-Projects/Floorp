"use strict";

/* eslint no-unused-vars: ["error", {"args": "none", "varsIgnorePattern": "^(FindProxyForURL)$"}] */

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gProxyService",
  "@mozilla.org/network/protocol-proxy-service;1",
  "nsIProtocolProxyService"
);

function getProxyInfo() {
  return new Promise((resolve, reject) => {
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
}
async function testProxyScript(script, expected = {}) {
  let scriptData = String(script).replace(/^.*?\{([^]*)\}$/, "$1");
  let extensionData = {
    manifest: {
      permissions: ["proxy"],
    },
    background() {
      // Some tests generate multiple errors, we'll just rely on the first.
      let seenError = false;
      browser.proxy.onProxyError.addListener(error => {
        if (!seenError) {
          browser.test.sendMessage("proxy-error-received", error);
          seenError = true;
        }
      });

      browser.test.onMessage.addListener(msg => {
        if (msg === "unregister-proxy-script") {
          browser.proxy.unregister().then(() => {
            browser.test.notifyPass("proxy");
          });
        }
      });

      browser.proxy.register("proxy.js").then(() => {
        browser.test.sendMessage("ready");
      });
    },
    files: {
      "proxy.js": scriptData,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
  await extension.awaitMessage("ready");

  let errorWait = extension.awaitMessage("proxy-error-received");

  let proxyInfo = await getProxyInfo();

  let error = await errorWait;
  equal(error.message, expected.message, "Correct error message received");
  if (!expected.proxyInfo) {
    equal(proxyInfo, null, "no proxyInfo received");
  } else {
    let { host, port, type } = expected.proxyInfo;
    equal(proxyInfo.host, host, `Expected proxy host to be ${host}`);
    equal(proxyInfo.port, port, `Expected proxy port to be ${port}`);
    equal(proxyInfo.type, type, `Expected proxy type to be ${type}`);
  }
  if (expected.errorInfo) {
    ok(error.fileName.includes("proxy.js"), "Error should include file name");
    equal(error.lineNumber, 3, "Error should include line number");
    ok(
      error.stack.includes("proxy.js:3:9"),
      "Error should include stack trace"
    );
  }
  extension.sendMessage("unregister-proxy-script");
  await extension.awaitFinish("proxy");
  await extension.unload();
}

add_task(async function test_invalid_FindProxyForURL_function() {
  await testProxyScript(() => {}, {
    message: "The proxy script must define FindProxyForURL as a function",
  });

  await testProxyScript(
    () => {
      var FindProxyForURL = 5; // eslint-disable-line mozilla/var-only-at-top-level
    },
    {
      message: "The proxy script must define FindProxyForURL as a function",
    }
  );

  await testProxyScript(
    () => {
      function FindProxyForURL() {
        return not_defined; // eslint-disable-line no-undef
      }
    },
    {
      message: "not_defined is not defined",
      errorInfo: true,
    }
  );

  // The following tests will produce multiple errors.
  await testProxyScript(
    () => {
      function FindProxyForURL() {
        return ";;;;;PROXY 1.2.3.4:8080";
      }
    },
    {
      message: "ProxyInfoData: Missing Proxy Rule",
    }
  );

  // We take any valid proxy up to the error.
  await testProxyScript(
    () => {
      function FindProxyForURL() {
        return "PROXY 1.2.3.4:8080; UNEXPECTED; SOCKS 1.2.3.4:8080";
      }
    },
    {
      message: 'ProxyInfoData: Unrecognized proxy type: "unexpected"',
      proxyInfo: {
        host: "1.2.3.4",
        port: "8080",
        type: "http",
        failoverProxy: null,
      },
    }
  );
});

async function getExtension(proxyResult) {
  let extensionData = {
    manifest: {
      permissions: ["proxy"],
    },
    background() {
      browser.proxy.register("proxy.js").then(() => {
        browser.test.sendMessage("ready");
      });
    },
    files: {
      "proxy.js": `
        function FindProxyForURL(url, host) {
          return ${proxyResult};
        }`,
    },
  };
  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
  await extension.awaitMessage("ready");
  return extension;
}

add_task(async function test_passthrough() {
  let ext1 = await getExtension(null);
  let ext2 = await getExtension('"PROXY 1.2.3.4:8888"');

  let proxyInfo = await getProxyInfo();

  equal(proxyInfo.host, "1.2.3.4", `second extension won`);
  equal(proxyInfo.port, "8888", `second extension won`);
  equal(proxyInfo.type, "http", `second extension won`);

  await ext2.unload();

  proxyInfo = await getProxyInfo();
  equal(proxyInfo, null, `expected no proxy`);
  await ext1.unload();
});
