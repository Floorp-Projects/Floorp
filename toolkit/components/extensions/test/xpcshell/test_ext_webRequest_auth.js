"use strict";

const HOSTS = new Set(["example.com"]);

const server = createHttpServer({ hosts: HOSTS });

const BASE_URL = "http://example.com";

// Save seen realms for cache checking.
let realms = new Set([]);

server.registerPathHandler("/authenticate.sjs", (request, response) => {
  let url = new URL(`${BASE_URL}${request.path}?${request.queryString}`);
  let realm = url.searchParams.get("realm") || "mochitest";
  let proxy_realm = url.searchParams.get("proxy_realm");

  function checkAuthorization(authorization) {
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

function getExtension(bgConfig) {
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
      details => {
        browser.test.log(
          `onAuthRequired called with ${details.requestId} ${details.url}`
        );
        browser.test.assertEq(
          config.realm,
          details.realm,
          "providing www authorization"
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
      permissions: ["webRequest", "webRequestBlocking", bgConfig.path],
    },
    background: `(${background})(${JSON.stringify(bgConfig)})`,
  });
}

add_task(async function test_webRequest_auth() {
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

  let extension = getExtension(config);
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
