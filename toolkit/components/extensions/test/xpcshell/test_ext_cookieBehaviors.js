"use strict";

const { UrlClassifierTestUtils } = ChromeUtils.import(
  "resource://testing-common/UrlClassifierTestUtils.jsm"
);

const {
  // cookieBehavior constants.
  BEHAVIOR_REJECT,
  BEHAVIOR_REJECT_TRACKER,
} = Ci.nsICookieService;

function createPage({ script, body = "" } = {}) {
  if (script) {
    body += `<script src="${script}"></script>`;
  }

  return `<!DOCTYPE html>
    <html>
      <head>
        <meta charset="utf-8">
      </head>
      <body>
        ${body}
      </body>
    </html>`;
}

const server = createHttpServer({ hosts: ["example.com", "itisatracker.org"] });
server.registerDirectory("/data/", do_get_file("data"));
server.registerPathHandler("/test-cookies", (request, response) => {
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/json", false);
  response.setHeader("Set-Cookie", "myKey=myCookie", true);
  response.write('{"success": true}');
});
server.registerPathHandler("/subframe.html", (request, response) => {
  response.write(createPage());
});
server.registerPathHandler("/page-with-tracker.html", (request, response) => {
  response.write(
    createPage({
      body: `<iframe src="http://itisatracker.org/test-cookies"></iframe>`,
    })
  );
});
server.registerPathHandler("/sw.js", (request, response) => {
  response.setHeader("Content-Type", "text/javascript", false);
  response.write("");
});

function assertCookiesForHost(url, cookiesCount, message) {
  const { host } = new URL(url);
  const cookies = Services.cookies.cookies.filter(
    cookie => cookie.host === host
  );
  equal(cookies.length, cookiesCount, message);
  return cookies;
}

// Test that the indexedDB and localStorage are allowed in an extension page
// and that the indexedDB is allowed in a extension worker.
add_task(async function test_ext_page_allowed_storage() {
  function testWebStorages() {
    const url = window.location.href;

    try {
      // In a webpage accessing indexedDB throws on cookiesBehavior reject,
      // here we verify that doesn't happen for an extension page.
      browser.test.assertTrue(
        indexedDB,
        "IndexedDB global should be accessible"
      );

      // In a webpage localStorage is undefined on cookiesBehavior reject,
      // here we verify that doesn't happen for an extension page.
      browser.test.assertTrue(
        localStorage,
        "localStorage global should be defined"
      );

      const worker = new Worker("worker.js");
      worker.onmessage = event => {
        browser.test.assertTrue(
          event.data.pass,
          "extension page worker have access to indexedDB"
        );

        browser.test.sendMessage("test-storage:done", url);
      };

      worker.postMessage({});
    } catch (err) {
      browser.test.fail(`Unexpected error: ${err}`);
      browser.test.sendMessage("test-storage:done", url);
    }
  }

  function testWorker() {
    this.onmessage = () => {
      try {
        void indexedDB;
        postMessage({ pass: true });
      } catch (err) {
        postMessage({ pass: false });
        throw err;
      }
    };
  }

  async function createExtension() {
    let extension = ExtensionTestUtils.loadExtension({
      files: {
        "test_web_storages.js": testWebStorages,
        "worker.js": testWorker,
        "page_subframe.html": createPage({ script: "test_web_storages.js" }),
        "page_with_subframe.html": createPage({
          body: '<iframe src="page_subframe.html"></iframe>',
        }),
        "page.html": createPage({
          script: "test_web_storages.js",
        }),
      },
    });

    await extension.startup();

    const EXT_BASE_URL = `moz-extension://${extension.uuid}/`;

    return { extension, EXT_BASE_URL };
  }

  const cookieBehaviors = [
    "BEHAVIOR_LIMIT_FOREIGN",
    "BEHAVIOR_REJECT_FOREIGN",
    "BEHAVIOR_REJECT",
    "BEHAVIOR_REJECT_TRACKER",
    "BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN",
  ];
  equal(
    cookieBehaviors.length,
    Ci.nsICookieService.BEHAVIOR_LAST,
    "all behaviors should be covered"
  );

  for (const behavior of cookieBehaviors) {
    info(
      `Test extension page access to indexedDB & localStorage with ${behavior}`
    );
    ok(
      behavior in Ci.nsICookieService,
      `${behavior} is a valid CookieBehavior`
    );
    Services.prefs.setIntPref(
      "network.cookie.cookieBehavior",
      Ci.nsICookieService[behavior]
    );

    // Create a new extension to ensure that the cookieBehavior just set is going to be
    // used for the requests triggered by the extension page.
    const { extension, EXT_BASE_URL } = await createExtension();
    const extPage = await ExtensionTestUtils.loadContentPage("about:blank", {
      extension,
      remote: extension.extension.remote,
    });

    info("Test from a top level extension page");
    await extPage.loadURL(`${EXT_BASE_URL}page.html`);

    let testedFromURL = await extension.awaitMessage("test-storage:done");
    equal(
      testedFromURL,
      `${EXT_BASE_URL}page.html`,
      "Got the results from the expected url"
    );

    info("Test from a sub frame extension page");
    await extPage.loadURL(`${EXT_BASE_URL}page_with_subframe.html`);

    testedFromURL = await extension.awaitMessage("test-storage:done");
    equal(
      testedFromURL,
      `${EXT_BASE_URL}page_subframe.html`,
      "Got the results from the expected url"
    );

    await extPage.close();
    await extension.unload();
  }
});

add_task(async function test_ext_page_3rdparty_cookies() {
  // Disable tracking protection to test cookies on BEHAVIOR_REJECT_TRACKER
  // (otherwise tracking protection would block the tracker iframe and
  // we would not be actually checking the cookie behavior).
  Services.prefs.setBoolPref("privacy.trackingprotection.enabled", false);
  await UrlClassifierTestUtils.addTestTrackers();
  registerCleanupFunction(function() {
    UrlClassifierTestUtils.cleanupTestTrackers();
    Services.prefs.clearUserPref("privacy.trackingprotection.enabled");
    Services.cookies.removeAll();
  });

  function testRequestScript() {
    browser.test.onMessage.addListener((msg, url) => {
      const done = () => {
        browser.test.sendMessage(`${msg}:done`);
      };

      switch (msg) {
        case "xhr": {
          let req = new XMLHttpRequest();
          req.onload = done;
          req.open("GET", url);
          req.send();
          break;
        }
        case "fetch": {
          window.fetch(url).then(done);
          break;
        }
        case "worker fetch": {
          const worker = new Worker("test_worker.js");
          worker.onmessage = evt => {
            if (evt.data.requestDone) {
              done();
            }
          };
          worker.postMessage({ url });
          break;
        }
        default: {
          browser.test.fail(`Received an unexpected message: ${msg}`);
          done();
        }
      }
    });

    browser.test.sendMessage("testRequestScript:ready", window.location.href);
  }

  function testWorker() {
    this.onmessage = evt => {
      fetch(evt.data.url).then(() => {
        postMessage({ requestDone: true });
      });
    };
  }

  async function createExtension() {
    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        permissions: ["http://example.com/*", "http://itisatracker.org/*"],
      },
      files: {
        "test_worker.js": testWorker,
        "test_request.js": testRequestScript,
        "page_subframe.html": createPage({ script: "test_request.js" }),
        "page_with_subframe.html": createPage({
          body: '<iframe src="page_subframe.html"></iframe>',
        }),
        "page.html": createPage({ script: "test_request.js" }),
      },
    });

    await extension.startup();

    const EXT_BASE_URL = `moz-extension://${extension.uuid}`;

    return { extension, EXT_BASE_URL };
  }

  const testUrl = "http://example.com/test-cookies";
  const testRequests = ["xhr", "fetch", "worker fetch"];
  const tests = [
    { behavior: "BEHAVIOR_ACCEPT", cookiesCount: 1 },
    { behavior: "BEHAVIOR_REJECT_FOREIGN", cookiesCount: 1 },
    { behavior: "BEHAVIOR_REJECT", cookiesCount: 0 },
    { behavior: "BEHAVIOR_LIMIT_FOREIGN", cookiesCount: 1 },
    { behavior: "BEHAVIOR_REJECT_TRACKER", cookiesCount: 1 },
  ];

  function clearAllCookies() {
    Services.cookies.removeAll();
    let cookies = Services.cookies.cookies;
    equal(cookies.length, 0, "There shouldn't be any cookies after clearing");
  }

  async function runTestRequests(extension, cookiesCount, msg) {
    for (const testRequest of testRequests) {
      clearAllCookies();
      extension.sendMessage(testRequest, testUrl);
      await extension.awaitMessage(`${testRequest}:done`);
      assertCookiesForHost(
        testUrl,
        cookiesCount,
        `${msg}: cookies count on ${testRequest} "${testUrl}"`
      );
    }
  }

  for (const { behavior, cookiesCount } of tests) {
    info(`Test cookies on http requests with ${behavior}`);
    ok(
      behavior in Ci.nsICookieService,
      `${behavior} is a valid CookieBehavior`
    );
    Services.prefs.setIntPref(
      "network.cookie.cookieBehavior",
      Ci.nsICookieService[behavior]
    );

    // Create a new extension to ensure that the cookieBehavior just set is going to be
    // used for the requests triggered by the extension page.
    const { extension, EXT_BASE_URL } = await createExtension();

    // Run all the test requests on a top level extension page.
    let extPage = await ExtensionTestUtils.loadContentPage(
      `${EXT_BASE_URL}/page.html`,
      {
        extension,
        remote: extension.extension.remote,
      }
    );
    await extension.awaitMessage("testRequestScript:ready");
    await runTestRequests(
      extension,
      cookiesCount,
      `Test top level extension page on ${behavior}`
    );
    await extPage.close();

    // Rerun all the test requests on a sub frame extension page.
    extPage = await ExtensionTestUtils.loadContentPage(
      `${EXT_BASE_URL}/page_with_subframe.html`,
      {
        extension,
        remote: extension.extension.remote,
      }
    );
    await extension.awaitMessage("testRequestScript:ready");
    await runTestRequests(
      extension,
      cookiesCount,
      `Test sub frame extension page on ${behavior}`
    );
    await extPage.close();

    await extension.unload();
  }

  // Test tracking url blocking from a webpage subframe.
  info(
    "Testing blocked tracker cookies in webpage subframe on BEHAVIOR_REJECT_TRACKERS"
  );
  Services.prefs.setIntPref(
    "network.cookie.cookieBehavior",
    BEHAVIOR_REJECT_TRACKER
  );

  const trackerURL = "http://itisatracker.org/test-cookies";
  const { extension, EXT_BASE_URL } = await createExtension();
  const extPage = await ExtensionTestUtils.loadContentPage(
    `${EXT_BASE_URL}/_generated_background_page.html`,
    {
      extension,
      remote: extension.extension.remote,
    }
  );
  clearAllCookies();

  await extPage.spawn(
    "http://example.com/page-with-tracker.html",
    async iframeURL => {
      const iframe = this.content.document.createElement("iframe");
      iframe.setAttribute("src", iframeURL);
      return new Promise(resolve => {
        iframe.onload = () => resolve();
        this.content.document.body.appendChild(iframe);
      });
    }
  );

  assertCookiesForHost(
    trackerURL,
    0,
    "Test cookies on web subframe inside top level extension page on BEHAVIOR_REJECT_TRACKER"
  );
  clearAllCookies();

  await extPage.close();
  await extension.unload();
});

// Test that a webpage embedded as a subframe of an extension page is not allowed to use
// IndexedDB and register a ServiceWorker when it shouldn't be based on the cookieBehavior.
add_task(
  async function test_webpage_subframe_storage_respect_cookiesBehavior() {
    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        permissions: ["http://example.com/*"],
        web_accessible_resources: ["subframe.html"],
      },
      files: {
        "toplevel.html": createPage({
          body: `
          <iframe id="ext" src="subframe.html"></iframe>
          <iframe id="web" src="http://example.com/subframe.html"></iframe>
        `,
        }),
        "subframe.html": createPage(),
      },
    });

    Services.prefs.setIntPref("network.cookie.cookieBehavior", BEHAVIOR_REJECT);

    await extension.startup();

    let extensionPage = await ExtensionTestUtils.loadContentPage(
      `moz-extension://${extension.uuid}/toplevel.html`,
      {
        extension,
        remote: extension.extension.remote,
      }
    );

    let results = await extensionPage.spawn(null, async () => {
      let extFrame = this.content.document.querySelector("iframe#ext");
      let webFrame = this.content.document.querySelector("iframe#web");

      function testIDB(win) {
        try {
          void win.indexedDB;
          return { success: true };
        } catch (err) {
          return { error: `${err}` };
        }
      }

      async function testServiceWorker(win) {
        try {
          await win.navigator.serviceWorker.register("sw.js");
          return { success: true };
        } catch (err) {
          return { error: `${err}` };
        }
      }

      return {
        extTopLevel: testIDB(this.content),
        extSubFrame: testIDB(extFrame.contentWindow),
        webSubFrame: testIDB(webFrame.contentWindow),
        webServiceWorker: await testServiceWorker(webFrame.contentWindow),
      };
    });

    let contentPage = await ExtensionTestUtils.loadContentPage(
      "http://example.com/subframe.html"
    );

    results.extSubFrameContent = await contentPage.spawn(
      extension.uuid,
      uuid => {
        return new Promise(resolve => {
          let frame = this.content.document.createElement("iframe");
          frame.setAttribute("src", `moz-extension://${uuid}/subframe.html`);
          frame.onload = () => {
            try {
              void frame.contentWindow.indexedDB;
              resolve({ success: true });
            } catch (err) {
              resolve({ error: `${err}` });
            }
          };
          this.content.document.body.appendChild(frame);
        });
      }
    );

    Assert.deepEqual(
      results.extTopLevel,
      { success: true },
      "IndexedDB allowed in a top level extension page"
    );

    Assert.deepEqual(
      results.extSubFrame,
      { success: true },
      "IndexedDB allowed in a subframe extension page with a top level extension page"
    );

    Assert.deepEqual(
      results.webSubFrame,
      { error: "SecurityError: The operation is insecure." },
      "IndexedDB not allowed in a subframe webpage with a top level extension page"
    );
    Assert.deepEqual(
      results.webServiceWorker,
      { error: "SecurityError: The operation is insecure." },
      "IndexedDB and Cache not allowed in a service worker registered in the subframe webpage extension page"
    );

    Assert.deepEqual(
      results.extSubFrameContent,
      { success: true },
      "IndexedDB allowed in a subframe extension page with a top level webpage"
    );

    await extensionPage.close();
    await contentPage.close();

    await extension.unload();
  }
);

// Test that the webpage's indexedDB and localStorage are still not allowed from a content script
// when the cookie behavior doesn't allow it, even when they are allowed in the extension pages.
add_task(async function test_content_script_on_cookieBehaviorReject() {
  Services.prefs.setIntPref("network.cookie.cookieBehavior", BEHAVIOR_REJECT);

  function contentScript() {
    // Ensure that when the current cookieBehavior doesn't allow a webpage to use indexedDB
    // or localStorage, then a WebExtension content script is not allowed to use it as well.
    browser.test.assertThrows(
      () => indexedDB,
      /The operation is insecure/,
      "a content script can't use indexedDB from a page where it is disallowed"
    );

    browser.test.assertThrows(
      () => localStorage,
      /The operation is insecure/,
      "a content script can't use localStorage from a page where it is disallowed"
    );

    browser.test.notifyPass("cs_disallowed_storage");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://*/*/file_sample.html"],
          js: ["content_script.js"],
        },
      ],
    },
    files: {
      "content_script.js": contentScript,
    },
  });

  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/file_sample.html"
  );

  await extension.awaitFinish("cs_disallowed_storage");

  await contentPage.close();
  await extension.unload();
});

add_task(function clear_cookieBehavior_pref() {
  Services.prefs.clearUserPref("network.cookie.cookieBehavior");
});
