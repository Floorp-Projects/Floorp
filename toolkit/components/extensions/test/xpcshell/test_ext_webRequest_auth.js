"use strict";

AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);
AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

const HOSTS = new Set(["example.com"]);

const server = createHttpServer({ hosts: HOSTS });

const BASE_URL = "http://example.com";

// Save seen realms for cache checking.
let realms = new Set([]);

server.registerPathHandler("/authenticate.sjs", (request, response) => {
  let url = new URL(`${BASE_URL}${request.path}?${request.queryString}`);
  let realm = url.searchParams.get("realm") || "mochitest";
  let proxy_realm = url.searchParams.get("proxy_realm");

  function checkAuthorization() {
    let expected_user = url.searchParams.get("user");
    if (!expected_user) {
      return true;
    }
    let expected_pass = url.searchParams.get("pass");
    let actual_user, actual_pass;
    let authHeader = request.getHeader("Authorization");
    let match = /Basic (.+)/.exec(authHeader);
    if (match.length != 2) {
      throw new Error("Couldn't parse auth header: " + authHeader);
    }
    let userpass = atob(match[1]); // no atob() :-(
    match = /(.*):(.*)/.exec(userpass);
    if (match.length != 3) {
      throw new Error("Couldn't decode auth header: " + userpass);
    }
    actual_user = match[1];
    actual_pass = match[2];
    return expected_user === actual_user && expected_pass === actual_pass;
  }

  response.setHeader("Content-Type", "text/plain; charset=UTF-8", false);
  if (proxy_realm && !request.hasHeader("Proxy-Authorization")) {
    // We're not testing anything that requires checking the proxy auth user/password.
    response.setStatusLine("1.0", 407, "Proxy authentication required");
    response.setHeader(
      "Proxy-Authenticate",
      `basic realm="${proxy_realm}"`,
      true
    );
    response.write("proxy auth required");
  } else if (
    !(
      realms.has(realm) &&
      request.hasHeader("Authorization") &&
      checkAuthorization()
    )
  ) {
    realms.add(realm);
    response.setStatusLine(request.httpVersion, 401, "Authentication required");
    response.setHeader("WWW-Authenticate", `basic realm="${realm}"`, true);
    response.write("auth required");
  } else {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write("ok, got authorization");
  }
});

function getExtension(bgConfig, { permissions = ["webRequestBlocking"] } = {}) {
  function background(config) {
    let path = config.path;
    browser.webRequest.onBeforeRequest.addListener(
      details => {
        browser.test.log(
          `onBeforeRequest called with ${details.requestId} ${details.url}`
        );
        browser.test.sendMessage("onBeforeRequest");
        return (
          config.onBeforeRequest.hasOwnProperty("result") &&
          config.onBeforeRequest.result
        );
      },
      { urls: [path] },
      config.onBeforeRequest.hasOwnProperty("extra")
        ? config.onBeforeRequest.extra
        : []
    );
    browser.webRequest.onAuthRequired.addListener(
      (details, asyncBlockingCb) => {
        browser.test.log(
          `onAuthRequired called with ${details.requestId} ${details.url}`
        );
        browser.test.assertEq(
          config.realm,
          details.realm,
          "providing www authorization"
        );

        if (config.asyncBlocking) {
          browser.test.assertEq(
            "function",
            typeof asyncBlockingCb,
            "expect onAuthRequired to receive a callback parameter with asyncBlocking extra"
          );
          // Delay the asyncBlockingCb call.
          // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
          setTimeout(() => {
            browser.test.sendMessage("onAuthRequired");
            asyncBlockingCb(
              config.onAuthRequired.hasOwnProperty("result")
                ? config.onAuthRequired.result
                : null
            );
          }, 100);
          // NOTE: Return values from an asyncBlocking listeners are expected to be ignored.
          // If this return value isn't ignored then the test case is expected to get stuck
          // and fail with a timeout, and test logs to show that the request was cancelled.
          return { cancel: true };
        }

        browser.test.assertEq(
          undefined,
          asyncBlockingCb,
          "expect no onAuthRequired callback parameter without asyncBlocking extra"
        );
        browser.test.sendMessage("onAuthRequired");
        return (
          config.onAuthRequired.hasOwnProperty("result") &&
          config.onAuthRequired.result
        );
      },
      { urls: [path] },
      config.onAuthRequired.hasOwnProperty("extra")
        ? config.onAuthRequired.extra
        : []
    );
    browser.webRequest.onCompleted.addListener(
      details => {
        browser.test.log(
          `onCompleted called with ${details.requestId} ${details.url}`
        );
        browser.test.sendMessage("onCompleted");
      },
      { urls: [path] }
    );
    browser.webRequest.onErrorOccurred.addListener(
      details => {
        browser.test.log(
          `onErrorOccurred called with ${JSON.stringify(details)}`
        );
        browser.test.sendMessage("onErrorOccurred");
      },
      { urls: [path] }
    );
  }

  return ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: [bgConfig.path, "webRequest", ...permissions],
    },
    background: `(${background})(${JSON.stringify(bgConfig)})`,
  });
}

async function test_webRequest_auth(
  permissions,
  { asyncBlocking = false } = {}
) {
  let config = {
    asyncBlocking,
    path: `${BASE_URL}/*`,
    realm: `webRequest_auth${Math.random()}`,
    onBeforeRequest: {
      extra: permissions.includes("webRequestBlocking") ? ["blocking"] : [],
    },
    onAuthRequired: {
      extra: asyncBlocking ? ["asyncBlocking"] : ["blocking"],
      result: {
        authCredentials: {
          username: "testuser",
          password: "testpass",
        },
      },
    },
  };

  let extension = getExtension(config, { permissions });
  await extension.startup();

  let requestUrl = `${BASE_URL}/authenticate.sjs?realm=${config.realm}`;
  let contentPage = await ExtensionTestUtils.loadContentPage(requestUrl);
  await Promise.all([
    extension.awaitMessage("onBeforeRequest"),
    extension.awaitMessage("onAuthRequired").then(() => {
      return Promise.all([
        extension.awaitMessage("onBeforeRequest"),
        extension.awaitMessage("onCompleted"),
      ]);
    }),
  ]);
  await contentPage.close();

  // Second time around to test cached credentials
  contentPage = await ExtensionTestUtils.loadContentPage(requestUrl);
  await Promise.all([
    extension.awaitMessage("onBeforeRequest"),
    extension.awaitMessage("onCompleted"),
  ]);

  await contentPage.close();
  await extension.unload();
}

add_task(async function test_webRequest_auth_with_webRequestBlocking() {
  await test_webRequest_auth(["webRequestBlocking"]);
});

add_task(async function test_webRequest_auth_with_webRequestAuthProvider() {
  await test_webRequest_auth(["webRequestAuthProvider"]);
});

add_task(async function test_webRequest_auth_with_asyncBlocking() {
  info("Test asyncBlocking with webRequestBlocking permission");
  await test_webRequest_auth(["webRequestBlocking"], { asyncBlocking: true });
  info("Test asyncBlocking with webRequestAuthProvider permission");
  await test_webRequest_auth(["webRequestAuthProvider"], {
    asyncBlocking: true,
  });
});

add_task(async function test_mutually_exclusive_asyncBlocking_and_blocking() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: [
        "webRequest",
        "webRequestAuthProvider",
        "https://example.org/*",
      ],
    },
    background() {
      try {
        browser.webRequest.onAuthRequired.addListener(
          () => {},
          { urls: ["https://example.org/*"] },
          ["blocking", "asyncBlocking"]
        );
        browser.test.fail(
          "onAuthRequired.addListener should have raised an exception"
        );
        browser.test.notifyFail("add-listener-error");
      } catch (err) {
        browser.test.assertEq(
          "'blocking' and 'asyncBlocking' are mutually exclusive",
          err.message,
          "Got the expected error when blocking and asyncBlocking are requested for the same listener"
        );
        browser.test.notifyPass("add-listener-error");
      }
    },
  });

  await extension.startup();
  await extension.awaitFinish("add-listener-error");
  await extension.unload();
});

add_task(async function test_webRequest_auth_cancelled() {
  // Test that any auth listener can cancel.
  let config = {
    path: `${BASE_URL}/*`,
    realm: `webRequest_auth${Math.random()}`,
    onBeforeRequest: {
      extra: ["blocking"],
    },
    onAuthRequired: {
      extra: ["blocking"],
      result: {
        authCredentials: {
          username: "testuser",
          password: "testpass",
        },
      },
    },
  };

  let ex1 = getExtension(config);
  config.onAuthRequired.result = { cancel: true };
  let ex2 = getExtension(config);
  await ex1.startup();
  await ex2.startup();

  let requestUrl = `${BASE_URL}/authenticate.sjs?realm=${config.realm}`;
  let contentPage = await ExtensionTestUtils.loadContentPage(requestUrl);
  await Promise.all([
    ex1.awaitMessage("onBeforeRequest"),
    ex1.awaitMessage("onAuthRequired"),
    ex1.awaitMessage("onErrorOccurred"),
    ex2.awaitMessage("onBeforeRequest"),
    ex2.awaitMessage("onAuthRequired"),
    ex2.awaitMessage("onErrorOccurred"),
  ]);

  await contentPage.close();
  await ex1.unload();
  await ex2.unload();
});

add_task(async function test_webRequest_auth_nonblocking() {
  let config = {
    path: `${BASE_URL}/*`,
    realm: `webRequest_auth${Math.random()}`,
    onBeforeRequest: {
      extra: ["blocking"],
    },
    onAuthRequired: {
      extra: ["blocking"],
      result: {
        authCredentials: {
          username: "testuser",
          password: "testpass",
        },
      },
    },
  };

  let ex1 = getExtension(config);
  // non-blocking ext tries to cancel but cannot.
  delete config.onBeforeRequest.extra;
  delete config.onAuthRequired.extra;
  config.onAuthRequired.result = { cancel: true };
  let ex2 = getExtension(config);
  await ex1.startup();
  await ex2.startup();

  let requestUrl = `${BASE_URL}/authenticate.sjs?realm=${config.realm}`;
  let contentPage = await ExtensionTestUtils.loadContentPage(requestUrl);
  await Promise.all([
    ex1.awaitMessage("onBeforeRequest"),
    ex1.awaitMessage("onAuthRequired").then(() => {
      return Promise.all([
        ex1.awaitMessage("onBeforeRequest"),
        ex1.awaitMessage("onCompleted"),
      ]);
    }),
    ex2.awaitMessage("onBeforeRequest"),
    ex2.awaitMessage("onAuthRequired").then(() => {
      return Promise.all([
        ex2.awaitMessage("onBeforeRequest"),
        ex2.awaitMessage("onCompleted"),
      ]);
    }),
  ]);

  await contentPage.close();
  Services.obs.notifyObservers(null, "net:clear-active-logins");
  await ex1.unload();
  await ex2.unload();
});

add_task(async function test_webRequest_auth_blocking_noreturn() {
  // The first listener is blocking but doesn't return anything.  The second
  // listener cancels the request.
  let config = {
    path: `${BASE_URL}/*`,
    realm: `webRequest_auth${Math.random()}`,
    onBeforeRequest: {
      extra: ["blocking"],
    },
    onAuthRequired: {
      extra: ["blocking"],
    },
  };

  let ex1 = getExtension(config);
  config.onAuthRequired.result = { cancel: true };
  let ex2 = getExtension(config);
  await ex1.startup();
  await ex2.startup();

  let requestUrl = `${BASE_URL}/authenticate.sjs?realm=${config.realm}`;
  let contentPage = await ExtensionTestUtils.loadContentPage(requestUrl);
  await Promise.all([
    ex1.awaitMessage("onBeforeRequest"),
    ex1.awaitMessage("onAuthRequired"),
    ex1.awaitMessage("onErrorOccurred"),
    ex2.awaitMessage("onBeforeRequest"),
    ex2.awaitMessage("onAuthRequired"),
    ex2.awaitMessage("onErrorOccurred"),
  ]);

  await contentPage.close();
  await ex1.unload();
  await ex2.unload();
});

add_task(async function test_webRequest_duelingAuth() {
  let config = {
    path: `${BASE_URL}/*`,
    realm: `webRequest_auth${Math.random()}`,
    onBeforeRequest: {
      extra: ["blocking"],
    },
    onAuthRequired: {
      extra: ["blocking"],
    },
  };
  let exNone = getExtension(config);
  await exNone.startup();

  let authCredentials = {
    username: `testuser_da1${Math.random()}`,
    password: `testpass_da1${Math.random()}`,
  };
  config.onAuthRequired.result = { authCredentials };
  let ex1 = getExtension(config);
  await ex1.startup();

  config.onAuthRequired.result = {};
  let exEmpty = getExtension(config);
  await exEmpty.startup();

  config.onAuthRequired.result = {
    authCredentials: {
      username: `testuser_da2${Math.random()}`,
      password: `testpass_da2${Math.random()}`,
    },
  };
  let ex2 = getExtension(config);
  await ex2.startup();

  let requestUrl = `${BASE_URL}/authenticate.sjs?realm=${config.realm}&user=${authCredentials.username}&pass=${authCredentials.password}`;
  let contentPage = await ExtensionTestUtils.loadContentPage(requestUrl);
  await Promise.all([
    exNone.awaitMessage("onBeforeRequest"),
    exNone.awaitMessage("onAuthRequired").then(() => {
      return Promise.all([
        exNone.awaitMessage("onBeforeRequest"),
        exNone.awaitMessage("onCompleted"),
      ]);
    }),
    exEmpty.awaitMessage("onBeforeRequest"),
    exEmpty.awaitMessage("onAuthRequired").then(() => {
      return Promise.all([
        exEmpty.awaitMessage("onBeforeRequest"),
        exEmpty.awaitMessage("onCompleted"),
      ]);
    }),
    ex1.awaitMessage("onBeforeRequest"),
    ex1.awaitMessage("onAuthRequired").then(() => {
      return Promise.all([
        ex1.awaitMessage("onBeforeRequest"),
        ex1.awaitMessage("onCompleted"),
      ]);
    }),
    ex2.awaitMessage("onBeforeRequest"),
    ex2.awaitMessage("onAuthRequired").then(() => {
      return Promise.all([
        ex2.awaitMessage("onBeforeRequest"),
        ex2.awaitMessage("onCompleted"),
      ]);
    }),
  ]);

  await Promise.all([
    await contentPage.close(),
    exNone.unload(),
    exEmpty.unload(),
    ex1.unload(),
    ex2.unload(),
  ]);
});

add_task(async function test_webRequest_auth_proxy() {
  function background(permissionPath) {
    let proxyOk = false;
    browser.webRequest.onAuthRequired.addListener(
      details => {
        browser.test.log(
          `handlingExt onAuthRequired called with ${details.requestId} ${details.url}`
        );
        if (details.isProxy) {
          browser.test.succeed("providing proxy authorization");
          proxyOk = true;
          return { authCredentials: { username: "puser", password: "ppass" } };
        }
        browser.test.assertTrue(
          proxyOk,
          "providing www authorization after proxy auth"
        );
        browser.test.sendMessage("done");
        return { authCredentials: { username: "auser", password: "apass" } };
      },
      { urls: [permissionPath] },
      ["blocking"]
    );
  }

  let handlingExt = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", `${BASE_URL}/*`],
    },
    background: `(${background})("${BASE_URL}/*")`,
  });

  await handlingExt.startup();

  let requestUrl = `${BASE_URL}/authenticate.sjs?realm=webRequest_auth${Math.random()}&proxy_realm=proxy_auth${Math.random()}`;
  let contentPage = await ExtensionTestUtils.loadContentPage(requestUrl);

  await handlingExt.awaitMessage("done");
  await contentPage.close();
  await handlingExt.unload();
});

add_task(async function test_asyncBlocking_onAuthRequired_startup_primed() {
  function background() {
    browser.webRequest.onAuthRequired.addListener(
      (detais, asyncBlockingCb) => {
        asyncBlockingCb({ cancel: true });
        browser.test.sendMessage("onAuthRequired");
      },
      { urls: ["http://example.com/*"] },
      ["asyncBlocking"]
    );
    browser.test.sendMessage("listener-added");
  }

  await AddonTestUtils.promiseStartupManager();

  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      background: { persistent: true },
      permissions: [
        "webRequest",
        "webRequestAuthProvider",
        "http://example.com/*",
      ],
    },
    background,
  });

  async function testTriggerPrimedListener() {
    let requestUrl = `${BASE_URL}/authenticate.sjs?realm=webRequest${Math.random()}`;
    let contentPagePromise = ExtensionTestUtils.loadContentPage(requestUrl);
    info("Wait for the background script to be respawned");
    await extension.awaitMessage("listener-added");
    info("Wait for the onAuthRequired listener to have been called");
    await extension.awaitMessage("onAuthRequired");
    const contentPage = await contentPagePromise;
    await contentPage.close();
  }

  await extension.startup();
  await extension.awaitMessage("listener-added");

  assertPersistentListeners(extension, "webRequest", "onAuthRequired", {
    primed: false,
  });

  await AddonTestUtils.promiseRestartManager({ earlyStartup: false });
  await extension.awaitStartup();

  assertPersistentListeners(extension, "webRequest", "onAuthRequired", {
    primed: true,
  });
  AddonTestUtils.notifyEarlyStartup();

  await testTriggerPrimedListener();

  Services.obs.notifyObservers(null, "net:clear-active-logins");
  await extension.unload();
});
