"use strict";

add_setup(() => {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.feedback", true);
});

// This function is serialized and called in the context of the test extension's
// background page. dnrTestUtils is passed to the background function.
function makeDnrTestUtils() {
  const dnrTestUtils = {};
  const dnr = browser.declarativeNetRequest;

  const DUMMY_ACTION = {
    // "modifyHeaders" is the only action that allows multiple rule matches.
    type: "modifyHeaders",
    responseHeaders: [{ operation: "append", header: "x", value: "y" }],
  };
  async function testMatchesRequest(request, ruleIds, description) {
    browser.test.assertDeepEq(
      ruleIds,
      (await dnr.testMatchOutcome(request)).matchedRules.map(mr => mr.ruleId),
      description
    );
  }
  async function testMatchesUrlFilter({
    urlFilter,
    isUrlFilterCaseSensitive,
    urls = [],
    urlsNonMatching = [],
  }) {
    // Sanity check: verify that there are no unexpected escaped characters,
    // because that can surprise.
    function sanityCheckUrl(url) {
      const normalizedUrl = new URL(url).href;
      if (normalizedUrl.split("%").length !== url.split("*").length) {
        // ^ we only check for %-escapes and not exact URL equality because the
        // tests imported from Chrome often omit the "/" (path separator).
        browser.test.assertEq(normalizedUrl, url, "url should be canonical");
      }
    }

    await dnr.updateSessionRules({
      addRules: [
        {
          id: 12345,
          condition: { urlFilter, isUrlFilterCaseSensitive },
          action: DUMMY_ACTION,
        },
      ],
    });
    for (let url of urls) {
      sanityCheckUrl(url);
      const request = { url, type: "other" };
      const description = `urlFilter ${urlFilter} should match: ${url}`;
      await testMatchesRequest(request, [12345], description);
    }
    for (let url of urlsNonMatching) {
      sanityCheckUrl(url);
      const request = { url, type: "other" };
      const description = `urlFilter ${urlFilter} should not match: ${url}`;
      await testMatchesRequest(request, [], description);
    }
    await dnr.updateSessionRules({ removeRuleIds: [12345] });
  }
  Object.assign(dnrTestUtils, {
    DUMMY_ACTION,
    testMatchesRequest,
    testMatchesUrlFilter,
  });
  return dnrTestUtils;
}

async function runAsDNRExtension({ background, manifest }) {
  let extension = ExtensionTestUtils.loadExtension({
    background: `(${background})((${makeDnrTestUtils})())`,
    manifest: {
      manifest_version: 3,
      permissions: ["declarativeNetRequest", "declarativeNetRequestFeedback"],
      // While testing urlFilter itself does not require any host permissions,
      // we are asking for host permissions anyway because the "modifyHeaders"
      // action requires host permissions, and we use the "modifyHeaders" action
      // to ensure that we can detect when multiple rules match.
      host_permissions: ["<all_urls>"],
      granted_host_permissions: true,
      ...manifest,
    },
    temporarilyInstalled: true, // <-- for granted_host_permissions
  });
  await extension.startup();
  await extension.awaitFinish();
  await extension.unload();
}

// This test checks various urlFilters with a possibly ambiguous interpretation.
// In some cases the semantic difference in interpretation can have different
// outcomes; in these cases we have chosen the behavior as observed in Chrome.
add_task(async function ambiguous_urlFilter_patterns() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { testMatchesUrlFilter } = dnrTestUtils;

      // Left anchor, empty pattern: always matches
      // Ambiguous with Right anchor, but same result.
      await testMatchesUrlFilter({
        urlFilter: "|",
        urls: ["http://a/"],
        urlsNonMatching: [],
      });

      // Domain anchor, empty pattern: always matches.
      // Ambiguous with Left anchor + Right anchor, the latter would not match
      // anything (only an empty string, but URLs cannot be empty).
      await testMatchesUrlFilter({
        urlFilter: "||",
        urls: ["http://a/"],
        urlsNonMatching: [],
      });

      // Domain anchor plus Right separator: never matches.
      // Ambiguous with Left anchor + | + Right anchor, that is no match either.
      await testMatchesUrlFilter({
        urlFilter: "|||",
        urls: [],
        urlsNonMatching: ["http://a./|||"],
      });

      // Repeated separator: ^^^^ matches separator chars (=everything except
      // alphanumeric, "_", "-", ".", "%"), but when at the end of a string,
      // the last "^" can also be interpreted as a right anchor (like ^^^|).
      // Ambiguous: while "^" is defined to match the end of URL, it could also
      // be interpreted as "^^^^" matching the end of URL 4x, i.e. always.
      await testMatchesUrlFilter({
        urlFilter: "^^^^",
        urls: [
          // Note: "^" is escaped "%5E" when part of the URL, except after "#".
          "http://a/#frag^^^^", // four ^ characters ("^^^^").
          "http://a/#frag^^^", // three ^ characters ("^^^") + end of URL.
          "http://a/?&#", // four separator characters ("/?&#");
          "http://a/#^", // three separator characters ("/??") + end of URL.
          // ^ Note that "^" is after "#" and therefore not %5E. If "^" were to
          // somehow be %-encoded to "%5E", then the end would become "/#%5E"
          // and the "/#%" would only be 3 separators followed by alphanum. The
          // test matching shows that the canonical representation of "^" after
          // a "#" is "^" and can be matched.
        ],
        urlsNonMatching: [
          "http://a/?", // Just two separator + end of URL, not matching 4x "^".
          "http://a/____", // _ is specified to not match ^.
          "http://a/----", // - is specified to not match ^.
          "http://a/....", // . is specified to not match ^.
        ],
      });
      // Not ambiguous, but for comparison with "^^^^": all http(s) match.
      await testMatchesUrlFilter({
        urlFilter: "^^^",
        urls: ["https://a/"], // "://" always matches "^^^".
        // Not seen by DNR in practice, but could be passed to testMatchOutcome:
        urlsNonMatching: ["file:hello/no/three/consecutive/special/characters"],
      });

      // Separator plus Right anchor: always matches.
      // Ambiguous: "^" is defined to match the end of URL once, but a right
      // domain anchor already matches that. A potential interpretation is for
      // "^" to be required to match a non-alphanumeric (etc.), but in practice
      // "^" is allowed to match the end of the URL. Effectively "^|" = "|".
      await testMatchesUrlFilter({
        urlFilter: "^|",
        urls: [
          "http://a/", // "/" matches "^".
          "http://a/a", // "a" does not match "^", but "^" matches the end.
        ],
        urlsNonMatching: [],
      });

      // Domain anchor plus separator: "^" only matches non-alphanum (etc.)
      // Ambiguous: "||" is defined to match a domain anchor. There is no
      // domain part after the trailing "." of a FQDN. Still, "." matches.
      await testMatchesUrlFilter({
        urlFilter: "||^",
        urls: ["http://a./"], // FQDN: "/" after "." matches "^".
        urlsNonMatching: ["http://a/", "http://a/||"],
      });

      browser.test.notifyPass();
    },
  });
});

add_task(async function urlFilter_domain_anchor() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { testMatchesUrlFilter } = dnrTestUtils;

      await testMatchesUrlFilter({
        // Not a domain anchor, but for comparison with "||ps" below:
        urlFilter: "ps",
        urls: [
          "https://example.com/", // ps in scheme.
          "http://ps.example.com/", // ps at start of domain.
          "http://sub.ps.example.com/", // ps at superdomain.
          "http://ps/", // ps as sole host.
          "http://example-ps.com/", // ps in middle of domain.
          "http://ps@example.com/", // ps as user without password.
          "http://user:ps@example.com/", // ps in password.
          "http://ps:pass@example.com/", // ps in user.
          "http://example.com/ps", // ps at end.
          "http://example.com/#ps", // ps in fragment.
        ],
        urlsNonMatching: [
          "http://example.com/", // no ps anywhere.
        ],
      });

      await testMatchesUrlFilter({
        urlFilter: "||ps",
        urls: [
          "http://ps.example.com/", // ps at start of domain.
          "http://sub.ps.example.com/", // ps at superdomain.
          "http://ps/", // ps as sole host.
        ],
        urlsNonMatching: [
          "http://example.com/", // no ps anywhere.
          "https://example.com/", // ps in scheme.
          "http://example-ps.com/", // ps in middle
          "http://ps@example.com/", // ps as user without password.
          "http://user:ps@example.com/", // ps in password.
          "http://ps:pass@example.com/", // ps in user.
          "http://example.com/ps", // ps at end.
        ],
      });

      await testMatchesUrlFilter({
        urlFilter: "||1",
        urls: [
          "http://127.0.0.1/",
          "http://2.0.0.1/",
          "http://www.1example.com/",
        ],
        urlsNonMatching: [
          "http://[::1]/",
          "http://[1::1]/",
          "http://hostwithport:1/",
          "http://host/1",
          "http://fqdn.:1/",
          "http://fqdn./1",
        ],
      });

      await testMatchesUrlFilter({
        urlFilter: "||^1",
        urls: [
          "http://[1::1]/", // "[1" at start matches "^1".
          "http://fqdn.:1/", // ":1" matches "^1" and is after a ".".
          "http://fqdn./1", // "/1" matches "^1" and is after a ".".
        ],
        urlsNonMatching: [
          "http://127.0.0.1/",
          "http://2.0.0.1/",
          "http://www.1example.com/",
          "http://[::1]/",
          "http://hostwithport:1/",
          "http://host/1",
        ],
      });

      browser.test.notifyPass();
    },
  });
});

// Extreme patterns that should not be used in practice, but are not explicitly
// documented to be disallowed.
add_task(
  // Stuck in ccov: https://bugzilla.mozilla.org/show_bug.cgi?id=1806494#c4
  { skip_if: () => mozinfo.ccov },
  async function extreme_urlFilter_patterns() {
    await runAsDNRExtension({
      background: async dnrTestUtils => {
        const { testMatchesRequest, DUMMY_ACTION } = dnrTestUtils;

        await browser.declarativeNetRequest.updateSessionRules({
          addRules: [
            {
              id: 1,
              condition: {
                urlFilter: "*".repeat(1e6),
              },
              action: DUMMY_ACTION,
            },
            {
              id: 2,
              condition: {
                urlFilter: "^".repeat(1e6),
              },
              action: DUMMY_ACTION,
            },
            {
              id: 3,
              condition: {
                // Note: 2 chars repeat 5e5 instead of 1e6 because newURI limits
                // the length of the URL (to network.standard-url.max-length),
                // so we would not be able to verify whether the URL is really
                // that long.
                urlFilter: "*^".repeat(5e5),
              },
              action: DUMMY_ACTION,
            },
            {
              id: 4,
              condition: {
                // Note: well beyond the maximum length of a URL. But as "*" can
                // match any char (including zero length), this still matches.
                urlFilter: "h" + "*".repeat(1e7) + "endofurl",
              },
              action: DUMMY_ACTION,
            },
          ],
        });

        await testMatchesRequest(
          { url: "http://example.com/", type: "other" },
          [1],
          "urlFilter with 1M wildcard chars matches any URL"
        );

        await testMatchesRequest(
          { url: "http://example.com/" + "x".repeat(1e6), type: "other" },
          [1],
          "urlFilter with 1M wildcards matches, other '^' do not match alpha"
        );

        await testMatchesRequest(
          { url: "http://example.com/" + "/".repeat(1e6), type: "other" },
          [1, 2, 3],
          "urlFilter with 1M wildcards, ^ and *^ all match URL with 1M '/' chars"
        );

        await testMatchesRequest(
          { url: "http://example.com/" + "x/".repeat(5e5), type: "other" },
          [1, 3],
          "urlFilter with 1M wildcards and *^ match URL with 1M 'x/' chars"
        );

        await testMatchesRequest(
          { url: "http://example.com/endofurl", type: "other" },
          [1, 4],
          "urlFilter with 1M and 10M wildcards matches URL"
        );

        browser.test.notifyPass();
      },
    });
  }
);

add_task(async function test_isUrlFilterCaseSensitive() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { testMatchesUrlFilter } = dnrTestUtils;

      await testMatchesUrlFilter({
        urlFilter: "AbC",
        isUrlFilterCaseSensitive: true,
        urls: [
          "http://true.example.com/AbC", // Exact match.
        ],
        urlsNonMatching: [
          "http://true.example.com/abc", // All lower.
          "http://true.example.com/ABC", // All upper.
          "http://true.example.com/???", // ABC not present at all.
          "http://true.AbC/", // When canonicalized, the host is lower case.
        ],
      });
      await testMatchesUrlFilter({
        urlFilter: "AbC",
        isUrlFilterCaseSensitive: false,
        urls: [
          "http://false.example.com/AbC", // Exact match.
          "http://false.example.com/abc", // All lower.
          "http://false.example.com/ABC", // All upper.
          "http://false.AbC/", // When canonicalized, the host is lower case.
        ],
        urlsNonMatching: [
          "http://false.example.com/???", // ABC not present at all.
        ],
      });

      // Chrome's initial DNR API specified isUrlFilterCaseSensitive to be true
      // by default. Later, it became false by default.
      // https://github.com/w3c/webextensions/issues/269
      await testMatchesUrlFilter({
        urlFilter: "AbC",
        // isUrlFilterCaseSensitive: false, // is implied by default.
        urls: [
          "http://default.example.com/AbC", // Exact match.
          "http://default.example.com/abc", // All lower.
          "http://default.example.com/ABC", // All upper.
          "http://default.AbC/", // When canonicalized, the host is lower case.
        ],
        urlsNonMatching: [
          "http://default.example.com/???", // ABC not present at all.
        ],
      });

      browser.test.notifyPass();
    },
  });
});

// Imported tests from Chromium from:
// https://chromium.googlesource.com/chromium/src.git/+/refs/tags/110.0.5442.0/components/url_pattern_index/url_pattern_unittest.cc
// kAnchorNone -> "" (anywhere in the string)
// kBoundary -> | (start or end of string)
// kSubdomain -> || (start of (sub)domain)
// kMatchCase -> isUrlFilterCaseSensitive: true
// kDonotMatchCase -> isUrlFilterCaseSensitive: false (this is the default).
// proto::URL_PATTERN_TYPE_WILDCARDED / proto::URL_PATTERN_TYPE_SUBSTRING -> ""
//
// Minus two tests ("", kBoundary, kBoundary) because the resulting pattern is
// "||" and ambiguous with ("", kSubdomain, "").
add_task(async function test_chrome_parity() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { testMatchesUrlFilter } = dnrTestUtils;
      const testCases = [
        // {"", proto::URL_PATTERN_TYPE_SUBSTRING}
        {
          urlFilter: "*",
          url: "http://ex.com/",
          expectMatch: true,
        },
        // // {"", proto::URL_PATTERN_TYPE_WILDCARDED}
        // { // Already tested before.
        //   urlFilter: "*",
        //   url: "http://ex.com/",
        //   expectMatch: true,
        // },
        // {"", kBoundary, kAnchorNone}
        {
          urlFilter: "|",
          url: "http://ex.com/",
          expectMatch: true,
        },
        // {"", kSubdomain, kAnchorNone}
        {
          urlFilter: "||",
          url: "http://ex.com/",
          expectMatch: true,
        },
        // // {"", kSubdomain, kAnchorNone}
        // { // Already tested before.
        //   urlFilter: "||",
        //   url: "http://ex.com/",
        //   expectMatch: true,
        // },
        // {"^", kSubdomain, kAnchorNone}
        {
          urlFilter: "||^",
          url: "http://ex.com/",
          expectMatch: false,
        },
        // {".", kSubdomain, kAnchorNone}
        {
          urlFilter: "||.",
          url: "http://ex.com/",
          expectMatch: false,
        },
        // // {"", kAnchorNone, kBoundary}
        // { // Already tested before.
        //   urlFilter: "|",
        //   url: "http://ex.com/",
        //   expectMatch: true,
        // },
        // {"^", kAnchorNone, kBoundary}
        {
          urlFilter: "^|",
          url: "http://ex.com/",
          expectMatch: true,
        },
        // {".", kAnchorNone, kBoundary}
        {
          urlFilter: ".|",
          url: "http://ex.com/",
          expectMatch: false,
        },
        // // {"", kBoundary, kBoundary}
        // { // "||" is ambiguous, cannot mean Left anchor + Right anchor
        //   urlFilter: "||",
        //   url: "http://ex.com/",
        //   expectMatch: false,
        // },
        // {"", kSubdomain, kBoundary}
        {
          urlFilter: "|||",
          url: "http://ex.com/",
          expectMatch: false,
        },
        // {"com/", kSubdomain, kBoundary}
        {
          urlFilter: "||com/|",
          url: "http://ex.com/",
          expectMatch: true,
        },
        // {"xampl", proto::URL_PATTERN_TYPE_SUBSTRING}
        {
          urlFilter: "xampl",
          url: "http://example.com",
          expectMatch: true,
        },
        // {"example", proto::URL_PATTERN_TYPE_SUBSTRING}
        {
          urlFilter: "example",
          url: "http://example.com",
          expectMatch: true,
        },
        // {"/a?a"}
        {
          urlFilter: "/a?a",
          url: "http://ex.com/a?a",
          expectMatch: true,
        },
        // {"^abc"}
        {
          urlFilter: "^abc",
          url: "http://ex.com/abc?a",
          expectMatch: true,
        },
        // {"^abc"}
        {
          urlFilter: "^abc",
          url: "http://ex.com/a?abc",
          expectMatch: true,
        },
        // {"^abc"}
        {
          urlFilter: "^abc",
          url: "http://ex.com/abc?abc",
          expectMatch: true,
        },
        // {"^abc^abc"}
        {
          urlFilter: "^abc^abc",
          url: "http://ex.com/abc?abc",
          expectMatch: true,
        },
        // {"^com^abc^abc"}
        {
          urlFilter: "^com^abc^abc",
          url: "http://ex.com/abc?abc",
          expectMatch: false,
        },
        // {"http://ex", kBoundary, kAnchorNone}
        {
          urlFilter: "|http://ex",
          url: "http://example.com",
          expectMatch: true,
        },
        // {"http://ex", kAnchorNone, kAnchorNone}
        {
          urlFilter: "http://ex",
          url: "http://example.com",
          expectMatch: true,
        },
        // {"mple.com/", kAnchorNone, kBoundary}
        {
          urlFilter: "mple.com/|",
          url: "http://example.com",
          expectMatch: true,
        },
        // {"mple.com/", kAnchorNone, kAnchorNone}
        {
          urlFilter: "mple.com/",
          url: "http://example.com",
          expectMatch: true,
        },
        // {"mple.com/", kSubdomain, kAnchorNone}
        {
          urlFilter: "||mple.com/",
          url: "http://example.com",
          expectMatch: false,
        },
        // {"ex.com", kSubdomain, kAnchorNone}
        {
          urlFilter: "||ex.com",
          url: "http://hex.com",
          expectMatch: false,
        },
        // {"ex.com", kSubdomain, kAnchorNone}
        {
          urlFilter: "||ex.com",
          url: "http://ex.com",
          expectMatch: true,
        },
        // {"ex.com", kSubdomain, kAnchorNone}
        {
          urlFilter: "||ex.com",
          url: "http://hex.ex.com",
          expectMatch: true,
        },
        // {"ex.com", kSubdomain, kAnchorNone}
        {
          urlFilter: "||ex.com",
          url: "http://hex.hex.com",
          expectMatch: false,
        },
        // {"example.com^", kSubdomain, kAnchorNone}
        {
          urlFilter: "||example.com^",
          url: "http://www.example.com",
          expectMatch: true,
        },
        // {"http://*mpl", kBoundary, kAnchorNone}
        {
          urlFilter: "|http://*mpl",
          url: "http://example.com",
          expectMatch: true,
        },
        // {"mpl*com/", kAnchorNone, kBoundary}
        {
          urlFilter: "mpl*com/|",
          url: "http://example.com",
          expectMatch: true,
        },
        // {"example^com"}
        {
          urlFilter: "example^com",
          url: "http://example.com",
          expectMatch: false,
        },
        // {"example^com"}
        {
          urlFilter: "example^com",
          url: "http://example/com",
          expectMatch: true,
        },
        // {"example.com^"}
        {
          urlFilter: "example.com^",
          url: "http://example.com:8080",
          expectMatch: true,
        },
        // {"http*.com/", kBoundary, kBoundary}
        {
          urlFilter: "|http*.com/|",
          url: "http://example.com",
          expectMatch: true,
        },
        // {"http*.org/", kBoundary, kBoundary}
        {
          urlFilter: "|http*.org/|",
          url: "http://example.com",
          expectMatch: false,
        },
        // {"/path?*&p1=*&p2="}
        {
          urlFilter: "/path?*&p1=*&p2=",
          url: "http://ex.com/aaa/path/bbb?k=v&p1=0&p2=1",
          expectMatch: false,
        },
        // {"/path?*&p1=*&p2="}
        {
          urlFilter: "/path?*&p1=*&p2=",
          url: "http://ex.com/aaa/path?k=v&p1=0&p2=1",
          expectMatch: true,
        },
        // {"/path?*&p1=*&p2="}
        {
          urlFilter: "/path?*&p1=*&p2=",
          url: "http://ex.com/aaa/path?k=v&k=v&p1=0&p2=1",
          expectMatch: true,
        },
        // {"/path?*&p1=*&p2="}
        {
          urlFilter: "/path?*&p1=*&p2=",
          url: "http://ex.com/aaa/path?k=v&p1=0&p3=10&p2=1",
          expectMatch: true,
        },
        // {"/path?*&p1=*&p2="}
        {
          urlFilter: "/path?*&p1=*&p2=",
          url: "http://ex.com/aaa/path&p1=0&p2=1",
          expectMatch: false,
        },
        // {"/path?*&p1=*&p2="}
        {
          urlFilter: "/path?*&p1=*&p2=",
          url: "http://ex.com/aaa/path?k=v&p2=0&p1=1",
          expectMatch: false,
        },
        // {"abc*def*ghijk*xyz"}
        {
          urlFilter: "abc*def*ghijk*xyz",
          url: "http://example.com/abcdeffffghijkmmmxyzzz",
          expectMatch: true,
        },
        // {"abc*cdef"}
        {
          urlFilter: "abc*cdef",
          url: "http://example.com/abcdef",
          expectMatch: false,
        },
        // {"^^a^^"}
        {
          urlFilter: "^^a^^",
          url: "http://ex.com/?a=/",
          expectMatch: true,
        },
        // {"^^a^^"}
        {
          urlFilter: "^^a^^",
          url: "http://ex.com/?a=/&b=0",
          expectMatch: true,
        },
        // {"^^a^^"}
        {
          urlFilter: "^^a^^",
          url: "http://ex.com/?a=x",
          expectMatch: false,
        },
        // {"^^a^^"}
        {
          urlFilter: "^^a^^",
          url: "http://ex.com/?a=",
          expectMatch: true,
        },
        // {"ex.com^path^*k=v^"}
        {
          urlFilter: "ex.com^path^*k=v^",
          url: "http://ex.com/path/?k1=v1&ak=v&kk=vv",
          expectMatch: true,
        },
        // {"ex.com^path^*k=v^"}
        {
          urlFilter: "ex.com^path^*k=v^",
          url: "http://ex.com/p/path/?k1=v1&ak=v&kk=vv",
          expectMatch: false,
        },
        // {"a^a&a^a&"}
        {
          urlFilter: "a^a&a^a&",
          url: "http://ex.com/a/a/a/a/?a&a&a&a&a",
          expectMatch: true,
        },
        // {"abc*def^"}
        {
          urlFilter: "abc*def^",
          url: "http://ex.com/abc/a/ddef/",
          expectMatch: true,
        },
        // {"https://example.com/"}
        {
          urlFilter: "https://example.com/",
          url: "http://example.com/",
          expectMatch: false,
        },
        // {"example.com/", kSubdomain, kAnchorNone}
        {
          urlFilter: "||example.com/",
          url: "http://example.com/",
          expectMatch: true,
        },
        // {"examp", kSubdomain, kAnchorNone}
        {
          urlFilter: "||examp",
          url: "http://example.com/",
          expectMatch: true,
        },
        // {"xamp", kSubdomain, kAnchorNone}
        {
          urlFilter: "||xamp",
          url: "http://example.com/",
          expectMatch: false,
        },
        // {"examp", kSubdomain, kAnchorNone}
        {
          urlFilter: "||examp",
          url: "http://test.example.com/",
          expectMatch: true,
        },
        // {"t.examp", kSubdomain, kAnchorNone}
        {
          urlFilter: "||t.examp",
          url: "http://test.example.com/",
          expectMatch: false,
        },
        // {"com^", kSubdomain, kAnchorNone}
        {
          urlFilter: "||com^",
          url: "http://test.example.com/",
          expectMatch: true,
        },
        // {"com^x", kSubdomain, kBoundary}
        {
          urlFilter: "||com^x|",
          url: "http://a.com/x",
          expectMatch: true,
        },
        // {"x.com", kSubdomain, kAnchorNone}
        {
          urlFilter: "||x.com",
          url: "http://ex.com/?url=x.com",
          expectMatch: false,
        },
        // {"ex.com/", kSubdomain, kBoundary}
        {
          urlFilter: "||ex.com/|",
          url: "http://ex.com/",
          expectMatch: true,
        },
        // {"ex.com^", kSubdomain, kBoundary}
        {
          urlFilter: "||ex.com^|",
          url: "http://ex.com/",
          expectMatch: true,
        },
        // {"ex.co", kSubdomain, kBoundary}
        {
          urlFilter: "||ex.co|",
          url: "http://ex.com/",
          expectMatch: false,
        },
        // {"ex.com", kSubdomain, kBoundary}
        {
          urlFilter: "||ex.com|",
          url: "http://rex.com.ex.com/",
          expectMatch: false,
        },
        // {"ex.com/", kSubdomain, kBoundary}
        {
          urlFilter: "||ex.com/|",
          url: "http://rex.com.ex.com/",
          expectMatch: true,
        },
        // {"http", kSubdomain, kBoundary}
        {
          urlFilter: "||http|",
          url: "http://http.com/",
          expectMatch: false,
        },
        // {"http", kSubdomain, kAnchorNone}
        {
          urlFilter: "||http",
          url: "http://http.com/",
          expectMatch: true,
        },
        // {"/example.com", kSubdomain, kBoundary}
        {
          urlFilter: "||/example.com|",
          url: "http://example.com/",
          expectMatch: false,
        },
        // {"/example.com/", kSubdomain, kBoundary}
        {
          urlFilter: "||/example.com/|",
          url: "http://example.com/",
          expectMatch: false,
        },
        // {".", kSubdomain, kAnchorNone}
        {
          urlFilter: "||.",
          url: "http://a..com/",
          expectMatch: true,
        },
        // {"^", kSubdomain, kAnchorNone}
        {
          urlFilter: "||^",
          url: "http://a..com/",
          expectMatch: false,
        },
        // {".", kSubdomain, kAnchorNone}
        {
          urlFilter: "||.",
          url: "http://a.com./",
          expectMatch: false,
        },
        // {"^", kSubdomain, kAnchorNone}
        {
          urlFilter: "||^",
          url: "http://a.com./",
          expectMatch: true,
        },
        // {".", kSubdomain, kAnchorNone}
        {
          urlFilter: "||.",
          url: "http://a.com../",
          expectMatch: true,
        },
        // {"^", kSubdomain, kAnchorNone}
        {
          urlFilter: "||^",
          url: "http://a.com../",
          expectMatch: true,
        },
        // {"/path", kSubdomain, kAnchorNone}
        {
          urlFilter: "||/path",
          url: "http://a.com./path/to/x",
          expectMatch: true,
        },
        // {"^path", kSubdomain, kAnchorNone}
        {
          urlFilter: "||^path",
          url: "http://a.com./path/to/x",
          expectMatch: true,
        },
        // {"/path", kSubdomain, kBoundary}
        {
          urlFilter: "||/path|",
          url: "http://a.com./path",
          expectMatch: true,
        },
        // {"^path", kSubdomain, kBoundary}
        {
          urlFilter: "||^path|",
          url: "http://a.com./path",
          expectMatch: true,
        },
        // {"path", kSubdomain, kBoundary}
        {
          urlFilter: "||path|",
          url: "http://a.com./path",
          expectMatch: false,
        },
        // {"path", proto::URL_PATTERN_TYPE_SUBSTRING, kDonotMatchCase}
        {
          urlFilter: "path",
          url: "http://a.com/PaTh",
          isUrlFilterCaseSensitive: false,
          expectMatch: true,
        },
        // {"path", proto::URL_PATTERN_TYPE_SUBSTRING, kMatchCase}
        {
          urlFilter: "path",
          url: "http://a.com/PaTh",
          isUrlFilterCaseSensitive: true,
          expectMatch: false,
        },
        // {"path", proto::URL_PATTERN_TYPE_SUBSTRING, kDonotMatchCase}
        {
          urlFilter: "path",
          url: "http://a.com/path",
          isUrlFilterCaseSensitive: false,
          expectMatch: true,
        },
        // {"path", proto::URL_PATTERN_TYPE_SUBSTRING, kMatchCase}
        {
          urlFilter: "path",
          url: "http://a.com/path",
          isUrlFilterCaseSensitive: true,
          expectMatch: true,
        },
        // {"abc*def^", proto::URL_PATTERN_TYPE_WILDCARDED, kMatchCase}
        {
          urlFilter: "abc*def^",
          url: "http://a.com/abcxAdef/vo",
          isUrlFilterCaseSensitive: true,
          expectMatch: true,
        },
        // {"abc*def^", proto::URL_PATTERN_TYPE_WILDCARDED, kMatchCase}
        {
          urlFilter: "abc*def^",
          url: "http://a.com/aBcxAdeF/vo",
          isUrlFilterCaseSensitive: true,
          expectMatch: false,
        },
        // {"abc*def^", proto::URL_PATTERN_TYPE_WILDCARDED, kDonotMatchCase}
        {
          urlFilter: "abc*def^",
          url: "http://a.com/aBcxAdeF/vo",
          isUrlFilterCaseSensitive: false,
          expectMatch: true,
        },
        // {"abc*def^", proto::URL_PATTERN_TYPE_WILDCARDED, kDonotMatchCase}
        {
          urlFilter: "abc*def^",
          url: "http://a.com/abcxAdef/vo",
          isUrlFilterCaseSensitive: false,
          expectMatch: true,
        },
        // {"abc^", kAnchorNone, kAnchorNone}
        {
          urlFilter: "abc^",
          url: "https://xyz.com/abc/123",
          expectMatch: true,
        },
        // {"abc^", kAnchorNone, kAnchorNone}
        {
          urlFilter: "abc^",
          url: "https://xyz.com/abc",
          expectMatch: true,
        },
        // {"abc^", kAnchorNone, kAnchorNone}
        {
          urlFilter: "abc^",
          url: "https://abc.com",
          expectMatch: false,
        },
        // {"abc^", kAnchorNone, kBoundary}
        {
          urlFilter: "abc^|",
          url: "https://xyz.com/abc/",
          expectMatch: true,
        },
        // {"abc^", kAnchorNone, kBoundary}
        {
          urlFilter: "abc^|",
          url: "https://xyz.com/abc",
          expectMatch: true,
        },
        // {"abc^", kAnchorNone, kBoundary}
        {
          urlFilter: "abc^|",
          url: "https://xyz.com/abc/123",
          expectMatch: false,
        },
        // {"http://abc.com/x^", kBoundary, kAnchorNone}
        {
          urlFilter: "|http://abc.com/x^",
          url: "http://abc.com/x",
          expectMatch: true,
        },
        // {"http://abc.com/x^", kBoundary, kAnchorNone}
        {
          urlFilter: "|http://abc.com/x^",
          url: "http://abc.com/x/",
          expectMatch: true,
        },
        // {"http://abc.com/x^", kBoundary, kAnchorNone}
        {
          urlFilter: "|http://abc.com/x^",
          url: "http://abc.com/x/123",
          expectMatch: true,
        },
        // {"http://abc.com/x^", kBoundary, kBoundary}
        {
          urlFilter: "|http://abc.com/x^|",
          url: "http://abc.com/x",
          expectMatch: true,
        },
        // {"http://abc.com/x^", kBoundary, kBoundary}
        {
          urlFilter: "|http://abc.com/x^|",
          url: "http://abc.com/x/",
          expectMatch: true,
        },
        // {"http://abc.com/x^", kBoundary, kBoundary}
        {
          urlFilter: "|http://abc.com/x^|",
          url: "http://abc.com/x/123",
          expectMatch: false,
        },
        // {"abc.com^", kSubdomain, kAnchorNone}
        {
          urlFilter: "||abc.com^",
          url: "http://xyz.abc.com/123",
          expectMatch: true,
        },
        // {"abc.com^", kSubdomain, kAnchorNone}
        {
          urlFilter: "||abc.com^",
          url: "http://xyz.abc.com",
          expectMatch: true,
        },
        // {"abc.com^", kSubdomain, kAnchorNone}
        {
          urlFilter: "||abc.com^",
          url: "http://abc.com.xyz.com?q=abc.com",
          expectMatch: false,
        },
        // {"abc.com^", kSubdomain, kBoundary}
        {
          urlFilter: "||abc.com^|",
          url: "http://xyz.abc.com/123",
          expectMatch: false,
        },
        // {"abc.com^", kSubdomain, kBoundary}
        {
          urlFilter: "||abc.com^|",
          url: "http://xyz.abc.com",
          expectMatch: true,
        },
        // {"abc.com^", kSubdomain, kBoundary}
        {
          urlFilter: "||abc.com^|",
          url: "http://abc.com.xyz.com?q=abc.com/",
          expectMatch: false,
        },
        // {"abc*^", kAnchorNone, kAnchorNone}
        {
          urlFilter: "abc*^",
          url: "https://abc.com",
          expectMatch: true,
        },
        // {"abc*^", kAnchorNone, kAnchorNone}
        {
          urlFilter: "abc*^",
          url: "https://abc.com?q=123",
          expectMatch: true,
        },
        // {"abc*^", kAnchorNone, kBoundary}
        {
          urlFilter: "abc*^|",
          url: "https://abc.com",
          expectMatch: true,
        },
        // {"abc*^", kAnchorNone, kBoundary}
        {
          urlFilter: "abc*^|",
          url: "https://abc.com?q=123",
          expectMatch: true,
        },
        // {"abc*", kAnchorNone, kBoundary}
        {
          urlFilter: "abc*|",
          url: "https://a.com/abcxyz",
          expectMatch: true,
        },
        // {"*google.com", kBoundary, kAnchorNone}
        {
          urlFilter: "|*google.com",
          url: "https://www.google.com",
          expectMatch: true,
        },
        // {"*", kBoundary, kBoundary}
        {
          urlFilter: "|*|",
          url: "https://example.com",
          expectMatch: true,
        },
        // // {"", kBoundary, kBoundary}
        // { // "||" is ambiguous, cannot mean Left anchor + Right anchor
        //   urlFilter: "||",
        //   url: "https://example.com",
        //   expectMatch: false,
        // },
      ];
      for (let test of testCases) {
        let { urlFilter, url, expectMatch, isUrlFilterCaseSensitive } = test;
        if (expectMatch) {
          await testMatchesUrlFilter({
            urlFilter,
            isUrlFilterCaseSensitive,
            urls: [url],
          });
        } else {
          await testMatchesUrlFilter({
            urlFilter,
            isUrlFilterCaseSensitive,
            urlsNonMatching: [url],
          });
        }
      }

      browser.test.notifyPass();
    },
  });
});
