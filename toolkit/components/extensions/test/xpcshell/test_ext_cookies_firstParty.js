"use strict";

const server = createHttpServer({
  hosts: ["example.org", "example.net", "example.com"],
});

function promiseSetCookies() {
  return new Promise(resolve => {
    server.registerPathHandler("/setCookies", (request, response) => {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "text/html; charset=utf-8", false);
      response.setHeader("Set-Cookie", "none=a; sameSite=none", true);
      response.setHeader("Set-Cookie", "lax=b; sameSite=lax", true);
      response.setHeader("Set-Cookie", "strict=c; sameSite=strict", true);
      response.write("<html></html>");
      resolve();
    });
  });
}

function promiseLoadedCookies() {
  return new Promise(resolve => {
    let cookies;

    server.registerPathHandler("/checkCookies", (request, response) => {
      cookies = request.hasHeader("Cookie") ? request.getHeader("Cookie") : "";

      response.setStatusLine(request.httpVersion, 302, "Moved Permanently");
      response.setHeader("Content-Type", "text/html; charset=utf-8", false);
      response.setHeader("Location", "/ready");
    });

    server.registerPathHandler("/navigate", (request, response) => {
      cookies = request.hasHeader("Cookie") ? request.getHeader("Cookie") : "";

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "text/html; charset=utf-8", false);
      response.write(
        "<html><script>location = '/checkCookies';</script></html>"
      );
    });

    server.registerPathHandler("/fetch", (request, response) => {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "text/html; charset=utf-8", false);
      response.write("<html><script>fetch('/checkCookies');</script></html>");
    });

    server.registerPathHandler("/nestedfetch", (request, response) => {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "text/html; charset=utf-8", false);
      response.write(
        "<html><iframe src='http://example.net/nestedfetch2'></iframe></html>"
      );
    });

    server.registerPathHandler("/nestedfetch2", (request, response) => {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "text/html; charset=utf-8", false);
      response.write(
        "<html><iframe src='http://example.org/fetch'></iframe></html>"
      );
    });

    server.registerPathHandler("/ready", (request, response) => {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "text/html; charset=utf-8", false);
      response.write("<html></html>");

      resolve(cookies);
    });
  });
}

add_task(async function setup() {
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  Services.prefs.setBoolPref("network.cookie.sameSite.laxByDefault", true);

  // We don't want to have 'secure' cookies because our test http server doesn't run in https.
  Services.prefs.setBoolPref(
    "network.cookie.sameSite.noneRequiresSecure",
    false
  );

  // Let's set 3 cookies before loading the extension.
  let cookiesPromise = promiseSetCookies();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.org/setCookies"
  );
  await cookiesPromise;
  await contentPage.close();
  Assert.equal(Services.cookies.cookies.length, 3);
});

add_task(async function test_cookies_firstParty() {
  async function pageScript() {
    const ifr = document.createElement("iframe");
    ifr.src = "http://example.org/" + location.search.slice(1);
    document.body.appendChild(ifr);
  }
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["*://example.org/"],
    },
    files: {
      "page.html": `<body><script src="page.js"></script></body>`,
      "page.js": pageScript,
    },
  });

  await extension.startup();

  // This page will load example.org in an iframe.
  let url = `moz-extension://${extension.uuid}/page.html`;
  let cookiesPromise = promiseLoadedCookies();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    url + "?checkCookies",
    { extension }
  );

  // Let's check the cookies received during the last loading.
  Assert.equal(await cookiesPromise, "none=a; lax=b; strict=c");
  await contentPage.close();

  // Let's navigate.
  cookiesPromise = promiseLoadedCookies();
  contentPage = await ExtensionTestUtils.loadContentPage(url + "?navigate", {
    extension,
  });

  // Let's check the cookies received during the last loading.
  Assert.equal(await cookiesPromise, "none=a; lax=b; strict=c");
  await contentPage.close();

  // Let's run a fetch()
  cookiesPromise = promiseLoadedCookies();
  contentPage = await ExtensionTestUtils.loadContentPage(url + "?fetch", {
    extension,
  });

  // Let's check the cookies received during the last loading.
  Assert.equal(await cookiesPromise, "none=a; lax=b; strict=c");
  await contentPage.close();

  // Let's run a fetch() from a nested iframe (extension -> example.net ->
  // example.org -> fetch)
  cookiesPromise = promiseLoadedCookies();
  contentPage = await ExtensionTestUtils.loadContentPage(url + "?nestedfetch", {
    extension,
  });

  // Let's check the cookies received during the last loading.
  Assert.equal(await cookiesPromise, "none=a");
  await contentPage.close();

  // Let's run a fetch() from a nested iframe (extension -> example.org -> fetch)
  cookiesPromise = promiseLoadedCookies();
  contentPage = await ExtensionTestUtils.loadContentPage(
    url + "?nestedfetch2",
    {
      extension,
    }
  );

  // Let's check the cookies received during the last loading.
  Assert.equal(await cookiesPromise, "none=a; lax=b; strict=c");
  await contentPage.close();

  await extension.unload();
});

add_task(async function test_cookies_iframes() {
  server.registerPathHandler("/echocookies", (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/html; charset=utf-8", false);
    response.write(
      request.hasHeader("Cookie") ? request.getHeader("Cookie") : ""
    );
  });

  server.registerPathHandler("/contentScriptHere", (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/html; charset=utf-8", false);
    response.write("<html></html>");
  });

  server.registerPathHandler("/pageWithFrames", (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/html; charset=utf-8", false);

    response.write(`
      <html>
        <iframe src="http://example.com/contentScriptHere"></iframe>
        <iframe src="http://example.net/contentScriptHere"></iframe>
      </html>
    `);
  });

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["*://example.org/"],
      content_scripts: [
        {
          js: ["contentScript.js"],
          matches: [
            "*://example.com/contentScriptHere",
            "*://example.net/contentScriptHere",
          ],
          run_at: "document_end",
          all_frames: true,
        },
      ],
    },
    files: {
      "contentScript.js": async () => {
        const res = await fetch("http://example.org/echocookies");
        const cookies = await res.text();
        browser.test.assertEq(
          "none=a",
          cookies,
          "expected cookies in content script"
        );
        browser.test.sendMessage("extfetch:" + location.hostname);
      },
    },
  });

  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/pageWithFrames"
  );
  await Promise.all([
    extension.awaitMessage("extfetch:example.com"),
    extension.awaitMessage("extfetch:example.net"),
  ]);
  await contentPage.close();
  await extension.unload();
});

add_task(async function test_cookies_background() {
  async function background() {
    const res = await fetch("http://example.org/echocookies", {
      credentials: "include",
    });
    const cookies = await res.text();
    browser.test.sendMessage("fetchcookies", cookies);
  }

  const tests = [
    {
      permissions: ["http://example.org/*"],
      cookies: "none=a; lax=b; strict=c",
    },
    {
      permissions: [],
      cookies: "none=a",
    },
  ];

  for (let test of tests) {
    let extension = ExtensionTestUtils.loadExtension({
      background,
      manifest: {
        permissions: test.permissions,
      },
    });

    server.registerPathHandler("/echocookies", (request, response) => {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "text/html; charset=utf-8", false);
      response.setHeader(
        "Access-Control-Allow-Origin",
        `moz-extension://${extension.uuid}`,
        false
      );
      response.setHeader("Access-Control-Allow-Credentials", "true", false);
      response.write(
        request.hasHeader("Cookie") ? request.getHeader("Cookie") : ""
      );
    });

    await extension.startup();
    equal(
      await extension.awaitMessage("fetchcookies"),
      test.cookies,
      "extension with permissions can see SameSite-restricted cookies"
    );

    await extension.unload();
  }
});

add_task(async function test_cookies_contentScript() {
  server.registerPathHandler("/empty", (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/html; charset=utf-8", false);
    response.write("<html><body></body></html>");
  });

  async function contentScript() {
    let res = await fetch("http://example.org/checkCookies");
    browser.test.assertEq(location.origin + "/ready", res.url, "request OK");
    browser.test.sendMessage("fetch-done");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          run_at: "document_end",
          js: ["contentscript.js"],
          matches: ["*://*/*"],
        },
      ],
    },
    files: {
      "contentscript.js": contentScript,
    },
  });

  await extension.startup();

  let cookiesPromise = promiseLoadedCookies();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.org/empty"
  );
  await extension.awaitMessage("fetch-done");

  // Let's check the cookies received during the last loading.
  Assert.equal(await cookiesPromise, "none=a; lax=b; strict=c");
  await contentPage.close();

  await extension.unload();
});
