"use strict";

// Tests whether we can redirect to a moz-extension: url.
ChromeUtils.defineModuleGetter(
  this,
  "TestUtils",
  "resource://testing-common/TestUtils.jsm"
);

const server = createHttpServer();
const gServerUrl = `http://localhost:${server.identity.primaryPort}`;

server.registerPathHandler("/redirect", (request, response) => {
  let params = new URLSearchParams(request.queryString);
  response.setStatusLine(request.httpVersion, 302, "Moved Temporarily");
  response.setHeader("Location", params.get("redirect_uri"));
  response.write("redirecting");
});

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write("ok");
});

function onStopListener(channel) {
  return new Promise(resolve => {
    let orig = channel.QueryInterface(Ci.nsITraceableChannel).setNewListener({
      QueryInterface: ChromeUtils.generateQI([
        "nsIRequestObserver",
        "nsIStreamListener",
      ]),
      getFinalURI(request) {
        let { loadInfo } = request;
        return (loadInfo && loadInfo.resultPrincipalURI) || request.originalURI;
      },
      onDataAvailable(...args) {
        orig.onDataAvailable(...args);
      },
      onStartRequest(request) {
        orig.onStartRequest(request);
      },
      onStopRequest(request, statusCode) {
        orig.onStopRequest(request, statusCode);
        let URI = this.getFinalURI(request.QueryInterface(Ci.nsIChannel));
        resolve(URI && URI.spec);
      },
    });
  });
}

async function onModifyListener(originUrl, redirectToUrl) {
  return TestUtils.topicObserved("http-on-modify-request", (subject, data) => {
    let channel = subject.QueryInterface(Ci.nsIHttpChannel);
    return channel.URI && channel.URI.spec == originUrl;
  }).then(([subject, data]) => {
    let channel = subject.QueryInterface(Ci.nsIHttpChannel);
    if (redirectToUrl) {
      channel.redirectTo(Services.io.newURI(redirectToUrl));
    }
    return channel;
  });
}

function getExtension(
  accessible = false,
  background = undefined,
  blocking = true
) {
  let manifest = {
    permissions: ["webRequest", "<all_urls>"],
  };
  if (blocking) {
    manifest.permissions.push("webRequestBlocking");
  }
  if (accessible) {
    manifest.web_accessible_resources = ["finished.html"];
  }
  if (!background) {
    background = () => {
      // send the extensions public uri to the test.
      let exturi = browser.runtime.getURL("finished.html");
      browser.test.sendMessage("redirectURI", exturi);
    };
  }
  return ExtensionTestUtils.loadExtension({
    manifest,
    files: {
      "finished.html": `
        <!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
          </head>
          <body>
            <h1>redirected!</h1>
          </body>
        </html>
      `.trim(),
    },
    background,
  });
}

async function redirection_test(url, channelRedirectUrl) {
  // setup our observer
  let watcher = onModifyListener(url, channelRedirectUrl).then(channel => {
    return onStopListener(channel);
  });
  let xhr = new XMLHttpRequest();
  xhr.open("GET", url);
  xhr.send();
  return watcher;
}

// This test verifies failure without web_accessible_resources.
add_task(async function test_redirect_to_non_accessible_resource() {
  let extension = getExtension();
  await extension.startup();
  let redirectUrl = await extension.awaitMessage("redirectURI");
  let url = `${gServerUrl}/redirect?redirect_uri=${redirectUrl}`;
  let result = await redirection_test(url);
  equal(result, url, `expected no redirect`);
  await extension.unload();
});

// This test makes a request against a server that redirects with a 302.
add_task(async function test_302_redirect_to_extension() {
  let extension = getExtension(true);
  await extension.startup();
  let redirectUrl = await extension.awaitMessage("redirectURI");
  let url = `${gServerUrl}/redirect?redirect_uri=${redirectUrl}`;
  let result = await redirection_test(url);
  equal(result, redirectUrl, "redirect request is finished");
  await extension.unload();
});

// This test uses channel.redirectTo during http-on-modify to redirect to the
// moz-extension url.
add_task(async function test_channel_redirect_to_extension() {
  let extension = getExtension(true);
  await extension.startup();
  let redirectUrl = await extension.awaitMessage("redirectURI");
  let url = `${gServerUrl}/dummy?r=${Math.random()}`;
  let result = await redirection_test(url, redirectUrl);
  equal(result, redirectUrl, "redirect request is finished");
  await extension.unload();
});

// This test verifies failure without web_accessible_resources.
add_task(async function test_content_redirect_to_non_accessible_resource() {
  let extension = getExtension();
  await extension.startup();
  let redirectUrl = await extension.awaitMessage("redirectURI");
  let url = `${gServerUrl}/redirect?redirect_uri=${redirectUrl}`;
  let watcher = onModifyListener(url).then(channel => {
    return onStopListener(channel);
  });
  let contentPage = await ExtensionTestUtils.loadContentPage(url, {
    redirectUrl: "about:blank",
  });
  equal(
    contentPage.browser.documentURI.spec,
    "about:blank",
    `expected no redirect`
  );
  equal(await watcher, url, "expected no redirect");
  await contentPage.close();
  await extension.unload();
});

// This test makes a request against a server that redirects with a 302.
add_task(async function test_content_302_redirect_to_extension() {
  let extension = getExtension(true);
  await extension.startup();
  let redirectUrl = await extension.awaitMessage("redirectURI");
  let url = `${gServerUrl}/redirect?redirect_uri=${redirectUrl}`;
  let contentPage = await ExtensionTestUtils.loadContentPage(url, {
    redirectUrl,
  });
  equal(contentPage.browser.documentURI.spec, redirectUrl, `expected redirect`);
  await contentPage.close();
  await extension.unload();
});

// This test uses channel.redirectTo during http-on-modify to redirect to the
// moz-extension url.
add_task(async function test_content_channel_redirect_to_extension() {
  let extension = getExtension(true);
  await extension.startup();
  let redirectUrl = await extension.awaitMessage("redirectURI");
  let url = `${gServerUrl}/dummy?r=${Math.random()}`;
  onModifyListener(url, redirectUrl);
  let contentPage = await ExtensionTestUtils.loadContentPage(url, {
    redirectUrl,
  });
  equal(contentPage.browser.documentURI.spec, redirectUrl, `expected redirect`);
  await contentPage.close();
  await extension.unload();
});

// This test makes a request against a server and tests redirect to another server page.
add_task(async function test_extension_302_redirect_web() {
  function background(serverUrl) {
    let expectedUrls = ["/redirect", "/dummy"];
    let expected = [
      "onBeforeRequest",
      "onHeadersReceived",
      "onBeforeRedirect",
      "onBeforeRequest",
      "onHeadersReceived",
      "onResponseStarted",
      "onCompleted",
    ];
    browser.webRequest.onBeforeRequest.addListener(
      details => {
        browser.test.assertTrue(
          details.url.includes(expectedUrls.shift()),
          "onBeforeRequest url matches"
        );
        browser.test.assertEq(
          expected.shift(),
          "onBeforeRequest",
          "onBeforeRequest matches"
        );
      },
      { urls: [serverUrl] }
    );
    browser.webRequest.onHeadersReceived.addListener(
      details => {
        browser.test.assertEq(
          expected.shift(),
          "onHeadersReceived",
          "onHeadersReceived matches"
        );
      },
      { urls: [serverUrl] }
    );
    browser.webRequest.onResponseStarted.addListener(
      details => {
        browser.test.assertEq(
          expected.shift(),
          "onResponseStarted",
          "onResponseStarted matches"
        );
      },
      { urls: [serverUrl] }
    );
    browser.webRequest.onBeforeRedirect.addListener(
      details => {
        browser.test.assertTrue(
          details.redirectUrl.includes("/dummy"),
          "onBeforeRedirect matches redirectUrl"
        );
        browser.test.assertEq(
          expected.shift(),
          "onBeforeRedirect",
          "onBeforeRedirect matches"
        );
      },
      { urls: [serverUrl] }
    );
    browser.webRequest.onCompleted.addListener(
      details => {
        browser.test.assertTrue(
          details.url.includes("/dummy"),
          "onCompleted expected url received"
        );
        browser.test.assertEq(
          expected.shift(),
          "onCompleted",
          "onCompleted matches"
        );
        browser.test.notifyPass("requestCompleted");
      },
      { urls: [serverUrl] }
    );
    browser.webRequest.onErrorOccurred.addListener(
      details => {
        browser.test.log(`onErrorOccurred ${JSON.stringify(details)}`);
        browser.test.notifyFail("requestCompleted");
      },
      { urls: [serverUrl] }
    );
  }
  let extension = getExtension(
    false,
    `(${background})("*://${server.identity.primaryHost}/*")`,
    false
  );
  await extension.startup();
  let redirectUrl = `${gServerUrl}/dummy`;
  let completed = extension.awaitFinish("requestCompleted");
  let url = `${gServerUrl}/redirect?r=${Math.random()}&redirect_uri=${redirectUrl}`;
  let contentPage = await ExtensionTestUtils.loadContentPage(url, {
    redirectUrl,
  });
  equal(
    contentPage.browser.documentURI.spec,
    redirectUrl,
    `expected content redirect`
  );
  await completed;
  await contentPage.close();
  await extension.unload();
});

// This test makes a request against a server and tests redirect to another server page, without
// onBeforeRedirect.  Bug 1448599
add_task(async function test_extension_302_redirect_opening() {
  let redirectUrl = `${gServerUrl}/dummy`;
  let expectData = [
    {
      event: "onBeforeRequest",
      url: `${gServerUrl}/redirect`,
    },
    {
      event: "onBeforeRequest",
      url: redirectUrl,
    },
  ];
  function background(serverUrl, expected) {
    browser.webRequest.onBeforeRequest.addListener(
      details => {
        let expect = expected.shift();
        browser.test.assertEq(
          expect.event,
          "onBeforeRequest",
          "onBeforeRequest event matches"
        );
        browser.test.assertTrue(
          details.url.startsWith(expect.url),
          "onBeforeRequest url matches"
        );
        if (expected.length === 0) {
          browser.test.notifyPass("requestCompleted");
        }
      },
      { urls: [serverUrl] }
    );
  }
  let extension = getExtension(
    false,
    `(${background})("*://${server.identity.primaryHost}/*", ${JSON.stringify(
      expectData
    )})`,
    false
  );
  await extension.startup();
  let completed = extension.awaitFinish("requestCompleted");
  let url = `${gServerUrl}/redirect?r=${Math.random()}&redirect_uri=${redirectUrl}`;
  let contentPage = await ExtensionTestUtils.loadContentPage(url, {
    redirectUrl,
  });
  equal(
    contentPage.browser.documentURI.spec,
    redirectUrl,
    `expected content redirect`
  );
  await completed;
  await contentPage.close();
  await extension.unload();
});

// This test makes a request against a server and tests redirect to another server page, without
// onBeforeRedirect.  Bug 1448599
add_task(async function test_extension_302_redirect_modify() {
  let redirectUrl = `${gServerUrl}/dummy`;
  let expectData = [
    {
      event: "onHeadersReceived",
      url: `${gServerUrl}/redirect`,
    },
    {
      event: "onHeadersReceived",
      url: redirectUrl,
    },
  ];
  function background(serverUrl, expected) {
    browser.webRequest.onHeadersReceived.addListener(
      details => {
        let expect = expected.shift();
        browser.test.assertEq(
          expect.event,
          "onHeadersReceived",
          "onHeadersReceived event matches"
        );
        browser.test.assertTrue(
          details.url.startsWith(expect.url),
          "onHeadersReceived url matches"
        );
        if (expected.length === 0) {
          browser.test.notifyPass("requestCompleted");
        }
      },
      { urls: ["<all_urls>"] }
    );
  }
  let extension = getExtension(
    false,
    `(${background})("*://${server.identity.primaryHost}/*", ${JSON.stringify(
      expectData
    )})`,
    false
  );
  await extension.startup();
  let completed = extension.awaitFinish("requestCompleted");
  let url = `${gServerUrl}/redirect?r=${Math.random()}&redirect_uri=${redirectUrl}`;
  let contentPage = await ExtensionTestUtils.loadContentPage(url, {
    redirectUrl,
  });
  equal(
    contentPage.browser.documentURI.spec,
    redirectUrl,
    `expected content redirect`
  );
  await completed;
  await contentPage.close();
  await extension.unload();
});

// This test makes a request against a server and tests redirect to another server page, without
// onBeforeRedirect.  Bug 1448599
add_task(async function test_extension_302_redirect_tracing() {
  let redirectUrl = `${gServerUrl}/dummy`;
  let expectData = [
    {
      event: "onCompleted",
      url: redirectUrl,
    },
  ];
  function background(serverUrl, expected) {
    browser.webRequest.onCompleted.addListener(
      details => {
        let expect = expected.shift();
        browser.test.assertEq(
          expect.event,
          "onCompleted",
          "onCompleted event matches"
        );
        browser.test.assertTrue(
          details.url.startsWith(expect.url),
          "onCompleted url matches"
        );
        if (expected.length === 0) {
          browser.test.notifyPass("requestCompleted");
        }
      },
      { urls: [serverUrl] }
    );
  }
  let extension = getExtension(
    false,
    `(${background})("*://${server.identity.primaryHost}/*", ${JSON.stringify(
      expectData
    )})`,
    false
  );
  await extension.startup();
  let completed = extension.awaitFinish("requestCompleted");
  let url = `${gServerUrl}/redirect?r=${Math.random()}&redirect_uri=${redirectUrl}`;
  let contentPage = await ExtensionTestUtils.loadContentPage(url, {
    redirectUrl,
  });
  equal(
    contentPage.browser.documentURI.spec,
    redirectUrl,
    `expected content redirect`
  );
  await completed;
  await contentPage.close();
  await extension.unload();
});

// This test makes a request against a server and tests webrequest.  Currently
// disabled due to NS_BINDING_ABORTED happening.
add_task(async function test_extension_302_redirect() {
  let extension = getExtension(true, () => {
    let myuri = browser.runtime.getURL("*");
    let exturi = browser.runtime.getURL("finished.html");
    browser.webRequest.onBeforeRedirect.addListener(
      details => {
        browser.test.assertEq(details.redirectUrl, exturi, "redirect matches");
      },
      { urls: ["<all_urls>", myuri] }
    );
    browser.webRequest.onCompleted.addListener(
      details => {
        browser.test.assertEq(details.url, exturi, "expected url received");
        browser.test.notifyPass("requestCompleted");
      },
      { urls: ["<all_urls>", myuri] }
    );
    browser.webRequest.onErrorOccurred.addListener(
      details => {
        browser.test.log(`onErrorOccurred ${JSON.stringify(details)}`);
        browser.test.notifyFail("requestCompleted");
      },
      { urls: ["<all_urls>", myuri] }
    );
    // send the extensions public uri to the test.
    browser.test.sendMessage("redirectURI", exturi);
  });
  await extension.startup();
  let redirectUrl = await extension.awaitMessage("redirectURI");
  let completed = extension.awaitFinish("requestCompleted");
  let url = `${gServerUrl}/redirect?r=${Math.random()}&redirect_uri=${redirectUrl}`;
  let contentPage = await ExtensionTestUtils.loadContentPage(url, {
    redirectUrl,
  });
  equal(
    contentPage.browser.documentURI.spec,
    redirectUrl,
    `expected content redirect`
  );
  await completed;
  await contentPage.close();
  await extension.unload();
}).skip();

// This test makes a request and uses onBeforeRequet to redirect to moz-ext.
// Currently disabled due to NS_BINDING_ABORTED happening.
add_task(async function test_extension_redirect() {
  let extension = getExtension(true, () => {
    let myuri = browser.runtime.getURL("*");
    let exturi = browser.runtime.getURL("finished.html");
    browser.webRequest.onBeforeRequest.addListener(
      details => {
        return { redirectUrl: exturi };
      },
      { urls: ["<all_urls>", myuri] },
      ["blocking"]
    );
    browser.webRequest.onBeforeRedirect.addListener(
      details => {
        browser.test.assertEq(details.redirectUrl, exturi, "redirect matches");
      },
      { urls: ["<all_urls>", myuri] }
    );
    browser.webRequest.onCompleted.addListener(
      details => {
        browser.test.assertEq(details.url, exturi, "expected url received");
        browser.test.notifyPass("requestCompleted");
      },
      { urls: ["<all_urls>", myuri] }
    );
    browser.webRequest.onErrorOccurred.addListener(
      details => {
        browser.test.log(`onErrorOccurred ${JSON.stringify(details)}`);
        browser.test.notifyFail("requestCompleted");
      },
      { urls: ["<all_urls>", myuri] }
    );
    // send the extensions public uri to the test.
    browser.test.sendMessage("redirectURI", exturi);
  });
  await extension.startup();
  let redirectUrl = await extension.awaitMessage("redirectURI");
  let completed = extension.awaitFinish("requestCompleted");
  let url = `${gServerUrl}/dummy?r=${Math.random()}`;
  let contentPage = await ExtensionTestUtils.loadContentPage(url, {
    redirectUrl,
  });
  equal(contentPage.browser.documentURI.spec, redirectUrl, `expected redirect`);
  await completed;
  await contentPage.close();
  await extension.unload();
}).skip();
