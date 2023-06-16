"use strict";

/**
 * This test verifies that the extension API's access to cookies is consistent
 * with the cookies as seen by web pages under the following modes:
 * - Every top-level document shares the same cookie jar, every subdocument of
 *   the top-level document has a distinct cookie jar tied to the site of the
 *   top-level document (dFPI).
 * - All documents have a cookie jar keyed by the domain of the top-level
 *   document (FPI).
 * - All cookies are in one cookie jar (classic behavior = no FPI nor dFPI)
 *
 * FPI and dFPI are implemented using OriginAttributes, and historically the
 * consequence of not recognizing an origin attribute is that cookies cannot be
 * deleted. Hence, the functionality of the cookies API is verified as follows,
 * by the testCookiesAPI/runTestCase methods.
 *
 * 1. Load page that creates cookies for the top and a framed document:
 *    - "delete_me"
 *    - "edit_me"
 * 2. cookies.getAll: get all cookies with extension API.
 * 3. cookies.remove: Remove "delete_me" cookies with the extension API.
 * 4. cookies.set: Edit "edit_me" cookie with the extension API.
 * 5. Verify that the web page can see "edit_me" cookie (via document.cookie).
 * 6. cookies.get: "edit_me" is still present.
 * 7. cookies.remove: "edit_me" can be removed.
 * 8. cookies.getAll: no cookies left.
 */

const FIRST_DOMAIN = "first.example.com";
const FIRST_DOMAIN_ETLD_PLUS_1 = "example.com";
const FIRST_DOMAIN_ETLD_PLUS_MANY = "nested.under.first.example.com";
const THIRD_PARTY_DOMAIN = "third.example.net";
const server = createHttpServer({
  hosts: [FIRST_DOMAIN, FIRST_DOMAIN_ETLD_PLUS_MANY, THIRD_PARTY_DOMAIN],
});
const LOCAL_IP_AND_PORT = `127.0.0.1:${server.identity.primaryPort}`;

server.registerPathHandler("/top", (request, response) => {
  response.setHeader("Set-Cookie", `delete_me=top; SameSite=none`);
  response.setHeader("Set-Cookie", `edit_me=top; SameSite=none`, true);
  response.setHeader("Content-Type", "text/html; charset=utf-8", false);
  response.write(
    `<!DOCTYPE html><iframe src="//third.example.net/framed"></iframe>`
  );
});
server.registerPathHandler("/framed", (request, response) => {
  response.setHeader("Set-Cookie", `delete_me=frame; SameSite=none`);
  response.setHeader("Set-Cookie", `edit_me=frame; SameSite=none`, true);
});

// Background script of the extension that drives the test.
// It first waits for the content scripts in /top and /framed to connect,
// in order to verify that cookie operations by the extension API are reflected
// to the web page (verified through document.cookie from the content script).
function backgroundScript() {
  let portsByDomain = new Map();

  async function getDocumentCookies(port) {
    return new Promise(resolve => {
      port.onMessage.addListener(function listener(cookieString) {
        port.onMessage.removeListener(listener);
        resolve(cookieString);
      });
      port.postMessage("get_cookies");
    });
  }

  // Stringify cookie identifier for comparisons in assertions.
  function stringifyCookie(cookie) {
    if (!cookie) {
      return "COOKIE MISSING";
    }
    let domain = cookie.domain;
    if (!domain) {
      // The return value of `cookies.remove` has a URL instead of a domain.
      domain = new URL(cookie.url).hostname;
    }
    return `${cookie.name} domain=${domain} firstPartyDomain=${
      cookie.firstPartyDomain
    } partitionKey=${JSON.stringify(cookie.partitionKey)}`;
  }
  function stringifyCookies(cookies) {
    return cookies.map(stringifyCookie).sort().join(" , ");
  }

  // detailsIn may have partitionKey and firstPartyDomain attributes.
  // expectedOut has partitionKey and firstPartyDomain attributes.
  async function runTestCase({ domain, detailsIn, expectedOut }) {
    const port = portsByDomain.get(domain);
    browser.test.assertTrue(port, `Got port to document for ${domain}`);

    let allCookies = await browser.cookies.getAll({
      domain,
      firstPartyDomain: null,
      partitionKey: {},
    });

    let allCookiesWithFPD = await browser.cookies.getAll({
      domain,
      ...detailsIn,
    });
    browser.test.assertEq(
      stringifyCookies(allCookies),
      stringifyCookies(allCookiesWithFPD),
      "cookies.getAll returns consistent results"
    );

    for (let [key, expectedValue] of Object.entries(expectedOut)) {
      expectedValue = JSON.stringify(expectedValue);
      browser.test.assertTrue(
        allCookies.every(c => JSON.stringify(c[key]) === expectedValue),
        `All ${allCookies.length} cookies have ${key}=${expectedValue}`
      );
    }

    // delete_me: get, remove, get.
    const cookieToDelete = {
      url: `http://${domain}/`,
      name: "delete_me",
      ...detailsIn,
    };
    const deletedCookie = {
      ...cookieToDelete,
      ...expectedOut,
    };
    browser.test.assertEq(
      stringifyCookie(deletedCookie),
      stringifyCookie(await browser.cookies.get(cookieToDelete)),
      "delete_me cookie exists before removal"
    );
    browser.test.assertEq(
      stringifyCookie(deletedCookie),
      stringifyCookie(await browser.cookies.remove(cookieToDelete)),
      "delete_me cookie has been removed by cookies.remove"
    );
    browser.test.assertEq(
      null,
      await browser.cookies.get(cookieToDelete),
      "delete_me cookie does not exist any more"
    );

    // edit_me: set, retrieve via document.cookie
    const cookieToEdit = {
      url: `http://${domain}/`,
      name: "edit_me",
      ...detailsIn,
    };
    const editedCookie = await browser.cookies.set({
      ...cookieToEdit,
      value: `new_value_${domain}`,
    });
    browser.test.assertEq(
      stringifyCookie({ ...cookieToEdit, ...expectedOut }),
      stringifyCookie(editedCookie),
      "edit_me cookie updated"
    );
    browser.test.assertEq(
      await getDocumentCookies(port),
      `edit_me=new_value_${domain}`,
      "Expected cookies after removing and editing a cookie"
    );

    // edit_me: get, remove, getAll.
    browser.test.assertEq(
      stringifyCookie(editedCookie),
      stringifyCookie(await browser.cookies.get(cookieToEdit)),
      "edit_me cookie still exists"
    );
    await browser.cookies.remove(cookieToEdit);
    let allCookiesAtEnd = await browser.cookies.getAll({
      domain,
      firstPartyDomain: null,
      partitionKey: {},
    });
    browser.test.assertEq(
      "[]",
      JSON.stringify(allCookiesAtEnd),
      "No cookies left"
    );
  }

  let resolveTestReady;
  let testReadyPromise = new Promise(resolve => {
    resolveTestReady = resolve;
  });

  browser.test.onMessage.addListener(async (msg, testCase) => {
    await testReadyPromise;
    browser.test.assertEq("runTest", msg, `Starting: ${testCase.description}`);
    try {
      await runTestCase(testCase);
    } catch (e) {
      browser.test.fail(`Unexpected error: ${e} :: ${e.stack}`);
    }
    browser.test.sendMessage("runTest_done");
  });

  // cookie-checker-contentscript.js will connect.
  browser.runtime.onConnect.addListener(port => {
    portsByDomain.set(port.name, port);
    browser.test.log(`Got port #${portsByDomain.size} ${port.name}`);
    if (portsByDomain.size === 2) {
      // The top document and the embedded frame has loaded and the
      // content script that we use to read cookies is connected.
      // The test can now start.
      resolveTestReady();
    }
  });
}

// The primary purpose of this test is to verify that the cookies API can read
// and write cookies that are actually in use by the web page.
async function testCookiesAPI({ testCases, topDomain = FIRST_DOMAIN }) {
  let extension = ExtensionTestUtils.loadExtension({
    background: backgroundScript,
    manifest: {
      permissions: [
        "cookies",
        // Remove port to work around bug 1350523.
        `*://${topDomain.replace(/:\d+$/, "")}/*`,
        `*://${THIRD_PARTY_DOMAIN}/*`,
      ],
      content_scripts: [
        {
          js: ["cookie-checker-contentscript.js"],
          matches: [
            // Remove port to work around bug 1362809.
            `*://${topDomain.replace(/:\d+$/, "")}/top`,
            `*://${THIRD_PARTY_DOMAIN}/framed`,
          ],
          all_frames: true,
          run_at: "document_end",
        },
      ],
    },
    files: {
      "cookie-checker-contentscript.js": () => {
        const port = browser.runtime.connect({ name: location.hostname });
        port.onMessage.addListener(msg => {
          browser.test.assertEq(msg, "get_cookies", "Expected port message");
          port.postMessage(document.cookie);
        });
      },
    },
  });
  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    `http://${topDomain}/top`
  );
  for (let testCase of testCases) {
    info(`Running test case: ${testCase.description}`);
    extension.sendMessage("runTest", testCase);
    await extension.awaitMessage("runTest_done");
  }
  await contentPage.close();
  await extension.unload();
}

add_task(async function setup() {
  // SameSite=none is needed to set cookies in third-party contexts.
  // SameSite=none usually requires Secure, but the test server doesn't support
  // https, so disable the Secure requirement for SameSite=none.
  Services.prefs.setBoolPref(
    "network.cookie.sameSite.noneRequiresSecure",
    false
  );
});

add_task(async function test_no_partitioning() {
  const testCases = [
    {
      description: "first-party cookies without any partitioning",
      domain: FIRST_DOMAIN,
      detailsIn: {
        firstPartyDomain: "",
        partitionKey: null,
      },
      expectedOut: {
        firstPartyDomain: "",
        partitionKey: null,
      },
    },
    {
      description: "third-party cookies without any partitioning",
      domain: THIRD_PARTY_DOMAIN,
      detailsIn: {
        // Without (d)FPI, firstPartyDomain and partitionKey are optional.
      },
      expectedOut: {
        firstPartyDomain: "",
        partitionKey: null,
      },
    },
  ];
  await runWithPrefs(
    // dFPI is enabled by default on Nightly, disable it.
    [["network.cookie.cookieBehavior", 4]],
    () => testCookiesAPI({ testCases })
  );
});

add_task(async function test_firstPartyIsolate() {
  const testCases = [
    {
      description: "first-party cookies with FPI",
      domain: FIRST_DOMAIN,
      detailsIn: {
        firstPartyDomain: FIRST_DOMAIN_ETLD_PLUS_1,
      },
      expectedOut: {
        firstPartyDomain: FIRST_DOMAIN_ETLD_PLUS_1,
        partitionKey: null,
      },
    },
    {
      description: "third-party cookies with FPI",
      domain: THIRD_PARTY_DOMAIN,
      detailsIn: {
        firstPartyDomain: FIRST_DOMAIN_ETLD_PLUS_1,
      },
      expectedOut: {
        firstPartyDomain: FIRST_DOMAIN_ETLD_PLUS_1,
        partitionKey: null,
      },
    },
  ];
  await runWithPrefs(
    [
      // FPI is mutually exclusive with dFPI. Disable dFPI.
      ["network.cookie.cookieBehavior", 4],
      ["privacy.firstparty.isolate", true],
    ],
    () => testCookiesAPI({ testCases })
  );
});

add_task(async function test_dfpi() {
  const testCases = [
    {
      description: "first-party cookies with dFPI",
      domain: FIRST_DOMAIN,
      detailsIn: {
        // partitionKey is optional and expected to default to unpartitioned.
      },
      expectedOut: {
        firstPartyDomain: "",
        partitionKey: null,
      },
    },
    {
      description: "third-party cookies with dFPI",
      domain: THIRD_PARTY_DOMAIN,
      detailsIn: {
        partitionKey: { topLevelSite: `http://${FIRST_DOMAIN_ETLD_PLUS_1}` },
      },
      expectedOut: {
        firstPartyDomain: "",
        partitionKey: { topLevelSite: `http://${FIRST_DOMAIN_ETLD_PLUS_1}` },
      },
    },
  ];
  await runWithPrefs(
    // Enable dFPI; 5 = BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN.
    [["network.cookie.cookieBehavior", 5]],
    () => testCookiesAPI({ testCases })
  );
});

add_task(async function test_dfpi_with_ip_and_port() {
  const testCases = [
    {
      description: "first-party cookies for IP with port",
      domain: "127.0.0.1",
      detailsIn: {
        partitionKey: null,
      },
      expectedOut: {
        firstPartyDomain: "",
        partitionKey: null,
      },
    },
    {
      description: "third-party cookies for IP with port",
      domain: THIRD_PARTY_DOMAIN,
      detailsIn: {
        partitionKey: { topLevelSite: `http://${LOCAL_IP_AND_PORT}` },
      },
      expectedOut: {
        firstPartyDomain: "",
        partitionKey: { topLevelSite: `http://${LOCAL_IP_AND_PORT}` },
      },
    },
  ];
  await runWithPrefs(
    // Enable dFPI; 5 = BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN.
    [["network.cookie.cookieBehavior", 5]],
    () => testCookiesAPI({ testCases, topDomain: LOCAL_IP_AND_PORT })
  );
});

add_task(async function test_dfpi_with_nested_subdomains() {
  const testCases = [
    {
      description: "first-party cookies with DFPI at eTLD+many",
      domain: FIRST_DOMAIN_ETLD_PLUS_MANY,
      detailsIn: {
        partitionKey: null,
      },
      expectedOut: {
        firstPartyDomain: "",
        partitionKey: null,
      },
    },
    {
      description: "third-party cookies for first party with eTLD+many",
      domain: THIRD_PARTY_DOMAIN,
      detailsIn: {
        // Partitioned cookies are keyed by eTLD+1, so even if eTLD+many is
        // passed, then eTLD+1 is stored (and returned).
        partitionKey: { topLevelSite: `http://${FIRST_DOMAIN_ETLD_PLUS_MANY}` },
      },
      expectedOut: {
        firstPartyDomain: "",
        partitionKey: { topLevelSite: `http://${FIRST_DOMAIN_ETLD_PLUS_1}` },
      },
    },
  ];
  await runWithPrefs(
    // Enable dFPI; 5 = BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN.
    [["network.cookie.cookieBehavior", 5]],
    () => testCookiesAPI({ testCases, topDomain: FIRST_DOMAIN_ETLD_PLUS_MANY })
  );
});

add_task(async function test_dfpi_with_non_default_use_site() {
  // privacy.dynamic_firstparty.use_site is a pref that can be used to toggle
  // the internal representation of partitionKey. True (default) means keyed
  // by site (scheme, host, port); false means keyed by host only.
  const testCases = [
    {
      description: "first-party cookies with dFPI and use_site=false",
      domain: FIRST_DOMAIN,
      detailsIn: {
        partitionKey: null,
      },
      expectedOut: {
        firstPartyDomain: "",
        partitionKey: null,
      },
    },
    {
      description: "third-party cookies with dFPI and use_site=false",
      domain: THIRD_PARTY_DOMAIN,
      detailsIn: {
        partitionKey: { topLevelSite: `http://${FIRST_DOMAIN_ETLD_PLUS_1}` },
      },
      expectedOut: {
        firstPartyDomain: "",
        // When use_site=false, the scheme is not stored, and the
        // implementation just prepends "https" as a dummy scheme.
        partitionKey: { topLevelSite: `https://${FIRST_DOMAIN_ETLD_PLUS_1}` },
      },
    },
  ];
  await runWithPrefs(
    [
      // Enable dFPI; 5 = BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN.
      ["network.cookie.cookieBehavior", 5],
      ["privacy.dynamic_firstparty.use_site", false],
    ],
    () => testCookiesAPI({ testCases })
  );
});
add_task(async function test_dfpi_with_ip_and_port_and_non_default_use_site() {
  // privacy.dynamic_firstparty.use_site is a pref that can be used to toggle
  // the internal representation of partitionKey. True (default) means keyed
  // by site (scheme, host, port); false means keyed by host only.
  const testCases = [
    {
      description: "first-party cookies for IP:port with dFPI+use_site=false",
      domain: "127.0.0.1",
      detailsIn: {
        partitionKey: null,
      },
      expectedOut: {
        firstPartyDomain: "",
        partitionKey: null,
      },
    },
    {
      description: "third-party cookies for IP:port with dFPI+use_site=false",
      domain: THIRD_PARTY_DOMAIN,
      detailsIn: {
        // When use_site=false, the scheme is not stored in the internal
        // representation of the partitionKey. So even though the web page
        // creates the cookie at HTTP, the cookies are still detected when
        // "https" is used.
        partitionKey: { topLevelSite: `https://${LOCAL_IP_AND_PORT}` },
      },
      expectedOut: {
        firstPartyDomain: "",
        // When use_site=false, the scheme and port are not stored.
        // "https" is used as a dummy scheme, and the port is not used.
        partitionKey: { topLevelSite: "https://127.0.0.1" },
      },
    },
  ];
  await runWithPrefs(
    [
      // Enable dFPI; 5 = BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN.
      ["network.cookie.cookieBehavior", 5],
      ["privacy.dynamic_firstparty.use_site", false],
    ],
    () => testCookiesAPI({ testCases, topDomain: LOCAL_IP_AND_PORT })
  );
});

add_task(async function dfpi_invalid_partitionKey() {
  AddonTestUtils.init(globalThis);
  AddonTestUtils.createAppInfo(
    "xpcshell@tests.mozilla.org",
    "XPCShell",
    "1",
    "42"
  );
  // The test below uses the browser.privacy API, which relies on
  // ExtensionSettingsStore, which in turn depends on AddonManager.
  await AddonTestUtils.promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      permissions: ["cookies", "*://example.com/*", "privacy"],
    },
    async background() {
      const url = "http://example.com/";
      const name = "dfpi_invalid_partitionKey_dummy_name";
      const value = "1";

      // Shorthands to minimize boilerplate.
      const set = d => browser.cookies.set({ url, name, value, ...d });
      const remove = d => browser.cookies.remove({ url, name, ...d });
      const get = d => browser.cookies.get({ url, name, ...d });
      const getAll = d => browser.cookies.getAll(d);

      await browser.test.assertRejects(
        set({ partitionKey: { topLevelSite: "example.net" } }),
        /Invalid value for 'partitionKey' attribute/,
        "partitionKey must be a URL, not a domain"
      );
      await browser.test.assertRejects(
        set({ partitionKey: { topLevelSite: "chrome://foo" } }),
        /Invalid value for 'partitionKey' attribute/,
        "partitionKey cannot be the chrome:-scheme (canonicalization fails)"
      );
      await browser.test.assertRejects(
        set({ partitionKey: { topLevelSite: "chrome://foo/foo/foo" } }),
        /Invalid value for 'partitionKey' attribute/,
        "partitionKey cannot be the chrome:-scheme (canonicalization passes)"
      );
      await browser.test.assertRejects(
        set({ partitionKey: { topLevelSite: "http://[]:" } }),
        /Invalid value for 'partitionKey' attribute/,
        "partitionKey must be a valid URL"
      );

      browser.test.assertThrows(
        () => get({ partitionKey: "" }),
        /Error processing partitionKey: Expected object instead of ""/,
        "cookies.get should reject invalid partitionKey (string)"
      );
      browser.test.assertThrows(
        () => get({ partitionKey: { topLevelSite: "http://x", badkey: 0 } }),
        /Error processing partitionKey: Unexpected property "badkey"/,
        "cookies.get should reject unsupported keys in partitionKey"
      );
      await browser.test.assertRejects(
        remove({ partitionKey: { topLevelSite: "invalid" } }),
        /Invalid value for 'partitionKey' attribute/,
        "cookies.remove should reject invalid partitionKey.topLevelSite"
      );
      await browser.test.assertRejects(
        get({ partitionKey: { topLevelSite: "invalid" } }),
        /Invalid value for 'partitionKey' attribute/,
        "cookies.get should reject invalid partitionKey.topLevelSite"
      );
      await browser.test.assertRejects(
        getAll({ partitionKey: { topLevelSite: "invalid" } }),
        /Invalid value for 'partitionKey' attribute/,
        "cookies.getAll should reject invalid partitionKey.topLevelSite"
      );

      // firstPartyDomain and partitionKey are mutually exclusive, because
      // FPI and dFPI are mutually exclusive.
      await browser.test.assertRejects(
        set({ firstPartyDomain: "example.net", partitionKey: {} }),
        /Partitioned cookies cannot have a 'firstPartyDomain' attribute./,
        "partitionKey and firstPartyDomain cannot both be non-empty"
      );

      // On Nightly, dFPI is enabled by default. We have to disable it first,
      // before we can enable FPI. Otherwise we would get error:
      // Can't enable firstPartyIsolate when cookieBehavior is 'reject_trackers_and_partition_foreign'
      await browser.privacy.websites.cookieConfig.set({
        value: { behavior: "reject_trackers" },
      });
      await browser.privacy.websites.firstPartyIsolate.set({
        value: true,
      });

      // FPI and dFPI are mutually exclusive. FPI is documented to require the
      // firstPartyDomain attribute, let's verify that, despite it being
      // technically possible to support both attributes.
      for (let cookiesMethod of [get, getAll, remove, set]) {
        await browser.test.assertRejects(
          cookiesMethod({ partitionKey: { topLevelSite: url } }),
          /First-Party Isolation is enabled, but the required 'firstPartyDomain' attribute was not set./,
          `cookies.${cookiesMethod.name} requires firstPartyDomain when FPI is enabled`
        );
      }

      // The pref changes above (to dFPI/FPI) via the browser.privacy API  will
      // be undone when the extension unloads.

      browser.test.sendMessage("test_done");
    },
  });
  await extension.startup();
  await extension.awaitMessage("test_done");
  await extension.unload();

  await AddonTestUtils.promiseShutdownManager();
});

add_task(async function dfpi_moz_extension() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["cookies", "*://example.com/*"],
    },
    async background() {
      let cookie = await browser.cookies.set({
        url: "http://example.com/",
        name: "moz_ext_party",
        value: "1",
        // moz-extension: URL is passed here, in an attempt to mark the cookie
        // as part of the "moz-extension:"-partition. Below we will expect ""
        // because the dFPI implementation treats "moz-extension" as
        // unpartitioned, see
        // https://searchfox.org/mozilla-central/rev/ac7da6c7306d86e2f86a302ce1e170ad54b3c1fe/caps/OriginAttributes.cpp#79-82
        partitionKey: { topLevelSite: browser.runtime.getURL("/") },
      });
      browser.test.assertEq(
        null,
        cookie.partitionKey,
        "Cookies in moz-extension:-URL are unpartitioned"
      );
      let deletedCookie = await browser.cookies.remove({
        url: "http://example.com/",
        name: "moz_ext_party",
        partitionKey: { topLevelSite: "moz-extension://ignoreme" },
      });
      browser.test.assertEq(
        null,
        deletedCookie.partitionKey,
        "moz-extension:-partition key is treated as unpartitioned"
      );
      browser.test.sendMessage("test_done");
    },
  });
  await extension.startup();
  await extension.awaitMessage("test_done");
  await extension.unload();
});

add_task(async function dfpi_about_scheme_as_partitionKey() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["cookies", "*://example.com/*"],
    },
    async background() {
      let cookie = await browser.cookies.set({
        url: "http://example.com/",
        name: "moz_ext_party",
        value: "1",
        partitionKey: { topLevelSite: "about:blank" },
      });
      // It doesn't really make sense to partition in `about:blank` (since it
      // cannot really be a first party), but for completeness of test coverage
      // we also check that the use of an about:-scheme results in predictable
      // behavior. The weird "about://"-URL below is the serialization of the
      // internal value of the partitionKey attribute:
      // https://searchfox.org/mozilla-central/rev/ac7da6c7306d86e2f86a302ce1e170ad54b3c1fe/caps/OriginAttributes.cpp#73-77
      browser.test.assertEq(
        "about://about.ef2a7dd5-93bc-417f-a698-142c3116864f.mozilla",
        cookie.partitionKey.topLevelSite,
        "An URL-like representation of the internal about:-format is returned"
      );
      let deletedCookie = await browser.cookies.remove({
        url: "http://example.com/",
        name: "moz_ext_party",
        partitionKey: {
          topLevelSite:
            "about://about.ef2a7dd5-93bc-417f-a698-142c3116864f.mozilla",
        },
      });
      browser.test.assertEq(
        "about://about.ef2a7dd5-93bc-417f-a698-142c3116864f.mozilla",
        deletedCookie.partitionKey.topLevelSite,
        "Cookie can be deleted via the dummy about:-scheme"
      );
      browser.test.sendMessage("test_done");
    },
  });
  await extension.startup();
  await extension.awaitMessage("test_done");
  await extension.unload();
});

// Same-site frames are expected to be unpartitioned.
// The cookies API can receive partitionKey and url that are same-site. While
// such cookies won't be sent to websites in practice, we do want to verify that
// the behavior is predictable.
add_task(async function test_url_is_same_site_as_partitionKey() {
  // This loads a page with a frame at third.example.net (= THIRD_PARTY_DOMAIN).
  let contentPage = await ExtensionTestUtils.loadContentPage(
    `http://${THIRD_PARTY_DOMAIN}/top`
  );
  await contentPage.close();
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["cookies", "*://third.example.net/"],
    },
    async background() {
      // Retrieve all cookies, partitioned and unpartitioned. We expect only
      // unpartitioned cookies at first because the top frame and the child
      // frame have the same origin.
      let initialCookies = await browser.cookies.getAll({ partitionKey: {} });
      browser.test.assertEq(
        "delete_me=frame,edit_me=frame",
        initialCookies.map(c => `${c.name}=${c.value}`).join(),
        "Same-site frames are in unpartitioned storage; /frame overwrites /top"
      );
      browser.test.assertTrue(
        await browser.cookies.remove({
          url: "https://third.example.net/",
          name: "delete_me",
        }),
        "Removed unpartitioned cookie"
      );
      browser.test.assertEq(
        "[null,null]",
        JSON.stringify(initialCookies.map(c => c.partitionKey)),
        "Cookies in same-site/same-origin frames are not partitioned"
      );

      // We only have one unpartitioned cookie (edit_cookie) left.

      // Add new cookie whose partitionKey is same-site relative to url.
      let newCookie = await browser.cookies.set({
        url: "http://third.example.net/",
        name: "edit_me",
        value: "url_is_partitionKey_eTLD+2",
        partitionKey: { topLevelSite: "http://third.example.net" },
      });
      browser.test.assertEq(
        "http://example.net",
        newCookie.partitionKey.topLevelSite,
        "Created cookie with partitionKey=url; eTLD+2 is normalized as eTLD+1"
      );

      browser.test.assertTrue(
        await browser.cookies.remove({
          url: "http://third.example.net/",
          name: "edit_me",
          partitionKey: {},
        }),
        "Removed unpartitioned cookie when partitionKey: {} is used"
      );

      browser.test.assertEq(
        null,
        await browser.cookies.remove({
          url: "http://third.example.net/",
          name: "edit_me",
          partitionKey: {},
        }),
        "No more unpartitioned cookies to remove"
      );

      browser.test.assertTrue(
        await browser.cookies.remove({
          url: "http://third.example.net/",
          name: "edit_me",
          partitionKey: { topLevelSite: "http://example.net" },
        }),
        "Removed partitioned cookie when partitionKey is passed"
      );

      browser.test.sendMessage("test_done");
    },
  });
  await extension.startup();
  await extension.awaitMessage("test_done");
  await extension.unload();
});

add_task(async function test_getAll_partitionKey() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["cookies", "*://third.example.net/"],
    },
    async background() {
      const url = "http://third.example.net";
      const name = "test_url_is_identical_to_partitionKey";
      const partitionKey = { topLevelSite: "http://example.com" };
      const firstPartyDomain = "example.net";

      // Create non-partitioned cookie, create partitioned cookie.
      await browser.cookies.set({ url, name, value: "no_partition" });
      await browser.cookies.set({ url, name, value: "fpd", firstPartyDomain });
      await browser.cookies.set({ url, name, partitionKey, value: "party" });
      // partitionKey + firstPartyDomain was tested in dfpi_invalid_partitionKey

      async function getAllValues(details) {
        let cookies = await browser.cookies.getAll(details);
        let values = cookies.map(c => c.value);
        return values.sort().join(); // Serialize for use with assertEq.
      }

      browser.test.assertEq(
        "no_partition",
        await getAllValues({}),
        "getAll() returns unpartitioned by default"
      );

      browser.test.assertEq(
        "no_partition,party",
        await getAllValues({ partitionKey: {} }),
        "getAll() with partitionKey: {} returns all cookies"
      );

      browser.test.assertEq(
        "party",
        await getAllValues({ partitionKey }),
        "getAll() with specific partitionKey returns partitionKey cookies only"
      );

      browser.test.assertEq(
        "",
        await getAllValues({ partitionKey: { topLevelSite: url } }),
        "getAll() with partitionKey set to cookie URL does not match anything"
      );

      browser.test.assertEq(
        "",
        await getAllValues({ partitionKey, firstPartyDomain }),
        "getAll() with non-empty partitionKey and firstPartyDomain does not match anything"
      );
      browser.test.assertEq(
        "fpd",
        await getAllValues({ partitionKey: {}, firstPartyDomain }),
        "getAll() with empty partitionKey and firstPartyDomain matches fpd"
      );

      browser.test.assertEq(
        "fpd,no_partition,party",
        await getAllValues({ partitionKey: {}, firstPartyDomain: null }),
        "getAll() with empty partitionKey and firstPartyDomain:null matches everything"
      );

      await browser.cookies.remove({ url, name });
      await browser.cookies.remove({ url, name, firstPartyDomain });
      await browser.cookies.remove({ url, name, partitionKey });

      browser.test.sendMessage("test_done");
    },
  });
  await extension.startup();
  await extension.awaitMessage("test_done");
  await extension.unload();
});

add_task(async function no_unexpected_cookies_at_end_of_test() {
  let results = [];
  for (const cookie of Services.cookies.cookies) {
    results.push({
      name: cookie.name,
      value: cookie.value,
      host: cookie.host,
      originAttributes: cookie.originAttributes,
    });
  }
  Assert.deepEqual(results, [], "Test should not leave any cookies");
});
