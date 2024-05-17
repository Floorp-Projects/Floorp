/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test steps:
// 1. Load file_link_dns_prefetch.sjs
// 2.`<link rel="dns-prefetch" href="https://example.org">` is in
//    the server-side sjs, so we will make the dns-request.
// 3. We verify that the dns request was made

const gDashboard = Cc["@mozilla.org/network/dashboard;1"].getService(
  Ci.nsIDashboard
);

/////////////////////////////////////////////////////////////////////////////
// To observe DNS requests when running via the mochitest proxy we must first take a few steps:
//
// 1. Update the mochitest proxy pac file to include dns resolution.
//  We do this by injecting "dnsResolve(host);" into the `FindProxyForURL()` pac function.
let existingPACScript = Services.prefs.getCharPref(
  "network.proxy.autoconfig_url"
);

let findProxyForURLFunction = "function FindProxyForURL(url, host){";
let directDnsPacScript = existingPACScript.replace(
  findProxyForURLFunction,
  `${findProxyForURLFunction}
    dnsResolve(host);
    `
);
Services.prefs.setStringPref(
  "network.proxy.autoconfig_url",
  directDnsPacScript
);

// 2. Ensure we don't disable dns prefetch despite using a proxy (this would otherwise happen after every request that the proxy completed)
Services.prefs.setBoolPref("network.dns.prefetch_via_proxy", true);

// 3. Make sure that HTTPS-First does not interfere with us loading HTTP tests
let initialHttpsFirst = Services.prefs.getBoolPref("dom.security.https_first");
Services.prefs.setBoolPref("dom.security.https_first", false);

// 4. And finally enable dns prefetching via the private dns service api (generally disabled in mochitest proxy)

Services.dns.QueryInterface(Ci.nsPIDNSService).prefetchEnabled = true;
/////////////////////////////////////////////////////////////////////////////

registerCleanupFunction(function () {
  // Restore proxy pac and dns prefetch behaviour via proxy
  Services.prefs.setCharPref("network.proxy.autoconfig_url", existingPACScript);
  Services.prefs.clearUserPref("network.dns.prefetch_via_proxy");
  Services.prefs.setBoolPref("dom.security.https_first", initialHttpsFirst);
  Services.dns.QueryInterface(Ci.nsPIDNSService).prefetchEnabled = false;
});

async function isRecordFound(hostname) {
  return new Promise(resolve => {
    gDashboard.requestDNSInfo(function (data) {
      let found = false;
      for (let i = 0; i < data.entries.length; i++) {
        if (data.entries[i].hostname == hostname) {
          found = true;
          break;
        }
      }
      resolve(found);
    });
  });
}

let https_requestUrl = `https://example.com/browser/netwerk/test/browser/file_link_dns_prefetch.sjs`;
let http_requestUrl = `http://example.com/browser/netwerk/test/browser/file_link_dns_prefetch.sjs`; // eslint-disable-line @microsoft/sdl/no-insecure-url

// Test dns-prefetch on https
add_task(async function test_https_dns_prefetch() {
  Services.dns.clearCache(true);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: https_requestUrl,
      waitForLoad: true,
    },
    async function () {}
  );

  Assert.ok(
    await TestUtils.waitForCondition(() => {
      return isRecordFound("example.org");
    }),
    "Record from link rel=dns-prefetch element should be found"
  );
  Assert.ok(await isRecordFound("example.com"), "Host record should be found");
});

// Test dns-prefetch on http
add_task(async function test_http_dns_prefetch() {
  Services.dns.clearCache(true);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: http_requestUrl,
      waitForLoad: true,
    },
    async function () {}
  );

  Assert.ok(
    await TestUtils.waitForCondition(() => {
      return isRecordFound("example.org");
    }),
    "Record from link rel=dns-prefetch element should be found"
  );
  Assert.ok(await isRecordFound("example.com"), "Host record should be found");
});

// Test dns-prefetch on https with the feature disabled
add_task(async function test_https_dns_prefetch_disabled() {
  Services.dns.clearCache(true);

  // Disable the feature to verify that it will not prefetch
  Services.prefs.setBoolPref("network.dns.disablePrefetchFromHTTPS", true);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: https_requestUrl,
      waitForLoad: true,
    },
    async function () {}
  );

  Assert.ok(await isRecordFound("example.com"), "Host record should be found");
  Assert.ok(
    !(await isRecordFound("example.org")),
    "Record from link rel=dns-prefetch element should not be found with disablePrefetchFromHTTPS set"
  );

  Services.prefs.clearUserPref("network.dns.disablePrefetchFromHTTPS");
});

// Test dns-prefetch on http with the feature disabled
add_task(async function test_http_dns_prefetch_disabled() {
  Services.dns.clearCache(true);

  // Disable the feature to verify, but this test is http, and so prefetch will execute
  Services.prefs.setBoolPref("network.dns.disablePrefetchFromHTTPS", true);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: http_requestUrl,
      waitForLoad: true,
    },
    async function () {}
  );

  Assert.ok(
    await TestUtils.waitForCondition(() => {
      return isRecordFound("example.org");
    }),
    "Record from link rel=dns-prefetch element should be found on http page with disablePrefetchFromHTTPS set"
  );
  Assert.ok(await isRecordFound("example.com"), "Host record should be found");

  Services.prefs.clearUserPref("network.dns.disablePrefetchFromHTTPS");
});

// Test if we speculatively prefetch dns for anchor elements on https documents
add_task(async function test_https_anchor_speculative_dns_prefetch() {
  Services.dns.clearCache(true);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: https_requestUrl,
      waitForLoad: true,
    },
    async function () {
      Assert.ok(
        await isRecordFound("example.com"),
        "Host record should be found"
      );
      Assert.ok(
        !(await isRecordFound("www.mozilla.org")),
        "By default we do not speculatively prefetch dns for anchor elements on https documents"
      );
    }
  );

  // And enable the pref to verify that it works
  Services.prefs.setBoolPref(
    "dom.prefetch_dns_for_anchor_https_document",
    true
  );
  Services.dns.clearCache(true);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: https_requestUrl,
      waitForLoad: true,
    },
    async function () {
      // The anchor element prefetchs are sent after pageload event; wait for them
      Assert.ok(
        await TestUtils.waitForCondition(() => {
          return isRecordFound("www.mozilla.org");
        }),
        "Speculatively prefetch dns for anchor elements on https documents"
      );
      Assert.ok(
        await isRecordFound("example.com"),
        "Host record should be found"
      );
    }
  );

  Services.prefs.clearUserPref("dom.prefetch_dns_for_anchor_https_document");
});

// Test that we speculatively prefetch dns for anchor elements on http documents
add_task(async function test_http_anchor_speculative_dns_prefetch() {
  Services.dns.clearCache(true);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: http_requestUrl,
      waitForLoad: true,
    },
    async function () {
      Assert.ok(
        await TestUtils.waitForCondition(() => {
          return isRecordFound("example.org");
        }),
        "Record from link rel=dns-prefetch element should be found"
      );

      // The anchor element prefetchs are sent after pageload event; wait for them
      Assert.ok(
        await TestUtils.waitForCondition(() => {
          return isRecordFound("www.mozilla.org");
        }),
        "By default we speculatively prefetch dns for anchor elements on http documents"
      );

      Assert.ok(
        await isRecordFound("example.com"),
        "Host record should be found"
      );
    }
  );

  // And disable the pref to verify that we no longer make the requests
  Services.prefs.setBoolPref(
    "dom.prefetch_dns_for_anchor_http_document",
    false
  );
  Services.dns.clearCache(true);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: http_requestUrl,
      waitForLoad: true,
    },
    async function () {
      Assert.ok(
        await TestUtils.waitForCondition(() => {
          return isRecordFound("example.org");
        }),
        "Record from link rel=dns-prefetch element should be found"
      );
      Assert.ok(
        !(await isRecordFound("www.mozilla.org")),
        "We disabled speculative prefetch dns for anchor elements on http documents"
      );
      Assert.ok(
        await isRecordFound("example.com"),
        "Host record should be found"
      );
    }
  );

  Services.prefs.clearUserPref("dom.prefetch_dns_for_anchor_http_document");
});
