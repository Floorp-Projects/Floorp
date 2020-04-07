/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { MatchURLFilters } = ChromeUtils.import(
  "resource://gre/modules/MatchURLFilters.jsm"
);

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);

function createTestFilter({ url, filters }) {
  let m = new MatchURLFilters(filters);
  return m.matches(url);
}

function expectPass({ url, filters }) {
  ok(
    createTestFilter({ url, filters }),
    `Expected match: ${JSON.stringify(filters)}, ${url}`
  );
}

function expectFail({ url, filters }) {
  ok(
    !createTestFilter({ url, filters }),
    `Expected no match: ${JSON.stringify(filters)}, ${url}`
  );
}

function expectThrow({ url, filters, exceptionMessageContains }) {
  let logData = { filters, url };

  Assert.throws(
    () => {
      createTestFilter({ url, filters });
    },
    exceptionMessageContains,
    `Check received exception for expected message: ${JSON.stringify(logData)}`
  );
}

add_task(async function test_match_url_filters() {
  const shouldPass = true;
  const shouldFail = true;
  const shouldThrow = true;

  var testCases = [
    // Empty, undefined and null filters.
    {
      shouldThrow,
      exceptionMessageContains: /filters array should not be empty/,
      filters: [],
      url: "http://mozilla.org",
    },
    {
      shouldThrow,
      exceptionMessageContains: /filters should be an array/,
      filters: undefined,
      url: "http://mozilla.org",
    },
    {
      shouldThrow,
      exceptionMessageContains: /filters should be an array/,
      filters: null,
      url: "http://mozilla.org",
    },

    // Wrong formats (in a real webextension this will be blocked by the schema validation).
    {
      shouldThrow,
      exceptionMessageContains: /filters should be an array/,
      filters: {},
      url: "http://mozilla.org",
    },
    {
      shouldThrow,
      exceptionMessageContains: /filters should be an array/,
      filters: { nonExistentCriteria: true },
      url: "http://mozilla.org",
    },
    {
      shouldPass,
      filters: [{ nonExistentCriteria: true }],
      url: "http://mozilla.org",
    },

    // Schemes filter over various url schemes.
    { shouldPass, filters: [{ schemes: ["http"] }], url: "http://mozilla.org" },
    {
      shouldPass,
      filters: [{ schemes: ["https"] }],
      url: "https://mozilla.org",
    },
    { shouldPass, filters: [{ schemes: ["ftp"] }], url: "ftp://fake/ftp/url" },
    { shouldPass, filters: [{ schemes: ["about"] }], url: "about:blank" },
    { shouldPass, filters: [{ schemes: ["data"] }], url: "data:,testDataURL" },
    { shouldFail, filters: [{ schemes: ["http"] }], url: "ftp://fake/ftp/url" },

    // Multiple schemes: pass when at least one scheme matches.
    {
      shouldPass,
      filters: [{ schemes: ["https", "about"] }],
      url: "https://mozilla.org",
    },
    {
      shouldPass,
      filters: [{ schemes: ["about", "https"] }],
      url: "https://mozilla.org",
    },
    {
      shouldFail,
      filters: [{ schemes: ["about", "http"] }],
      url: "https://mozilla.org",
    },

    // Port filter: standard (implicit) ports.
    { shouldPass, filters: [{ ports: [443] }], url: "https://mozilla.org" },
    { shouldPass, filters: [{ ports: [80] }], url: "http://mozilla.org" },
    {
      shouldPass,
      filters: [{ ports: [21] }],
      url: "ftp://ftp.mozilla.org",
      prefs: [["network.ftp.enabled", true]],
    },

    // Port matching unknown protocols will fail.
    {
      shouldFail,
      filters: [{ ports: [21] }],
      url: "ftp://ftp.mozilla.org",
      prefs: [["network.ftp.enabled", false]],
    },

    // Port filter: schemes without a default port.
    { shouldFail, filters: [{ ports: [-1] }], url: "about:blank" },
    { shouldFail, filters: [{ ports: [-1] }], url: "data:,testDataURL" },

    { shouldFail, filters: [{ ports: [[1, 65535]] }], url: "about:blank" },
    {
      shouldFail,
      filters: [{ ports: [[1, 65535]] }],
      url: "data:,testDataURL",
    },

    // Host filters (hostEquals, hostContains, hostPrefix, hostSuffix): schemes with an host.
    { shouldFail, filters: [{ hostEquals: "" }], url: "https://mozilla.org" },
    { shouldPass, filters: [{ hostEquals: null }], url: "https://mozilla.org" },
    {
      shouldPass,
      filters: [{ hostEquals: "mozilla.org" }],
      url: "https://mozilla.org",
    },
    {
      shouldFail,
      filters: [{ hostEquals: "mozilla.com" }],
      url: "https://mozilla.org",
    },
    // NOTE: trying at least once another valid protocol.
    {
      shouldPass,
      filters: [{ hostEquals: "mozilla.org" }],
      url: "ftp://mozilla.org",
    },
    {
      shouldFail,
      filters: [{ hostEquals: "mozilla.com" }],
      url: "ftp://mozilla.org",
    },
    {
      shouldPass,
      filters: [{ hostEquals: "mozilla.org" }],
      url: "https://mozilla.org:8888",
    },

    {
      shouldPass,
      filters: [{ hostContains: "moz" }],
      url: "https://mozilla.org",
    },
    // NOTE: an implicit '.' char is inserted into the host.
    {
      shouldPass,
      filters: [{ hostContains: ".moz" }],
      url: "https://mozilla.org",
    },
    {
      shouldFail,
      filters: [{ hostContains: "com" }],
      url: "https://mozilla.org",
    },
    { shouldPass, filters: [{ hostContains: "" }], url: "https://mozilla.org" },
    {
      shouldPass,
      filters: [{ hostContains: null }],
      url: "https://mozilla.org",
    },

    {
      shouldPass,
      filters: [{ hostPrefix: "moz" }],
      url: "https://mozilla.org",
    },
    {
      shouldFail,
      filters: [{ hostPrefix: "org" }],
      url: "https://mozilla.org",
    },
    { shouldPass, filters: [{ hostPrefix: "" }], url: "https://mozilla.org" },
    { shouldPass, filters: [{ hostPrefix: null }], url: "https://mozilla.org" },

    {
      shouldPass,
      filters: [{ hostSuffix: ".org" }],
      url: "https://mozilla.org",
    },
    {
      shouldFail,
      filters: [{ hostSuffix: "moz" }],
      url: "https://mozilla.org",
    },
    { shouldPass, filters: [{ hostSuffix: "" }], url: "https://mozilla.org" },
    { shouldPass, filters: [{ hostSuffix: null }], url: "https://mozilla.org" },
    {
      shouldPass,
      filters: [{ hostSuffix: "lla.org" }],
      url: "https://mozilla.org:8888",
    },

    // hostEquals: urls without an host.
    // TODO: should we explicitly cover hostContains, hostPrefix, hostSuffix for
    // these sub-cases?
    { shouldFail, filters: [{ hostEquals: "blank" }], url: "about:blank" },
    { shouldFail, filters: [{ hostEquals: "blank" }], url: "about://blank" },
    {
      shouldFail,
      filters: [{ hostEquals: "testDataURL" }],
      url: "data:,testDataURL",
    },
    { shouldPass, filters: [{ hostEquals: "" }], url: "about:blank" },
    { shouldPass, filters: [{ hostEquals: "" }], url: "about://blank" },
    { shouldPass, filters: [{ hostEquals: "" }], url: "data:,testDataURL" },

    // Path filters (pathEquals, pathContains, pathPrefix, pathSuffix).
    {
      shouldFail,
      filters: [{ pathEquals: "" }],
      url: "https://mozilla.org/test/path",
    },
    {
      shouldPass,
      filters: [{ pathEquals: null }],
      url: "https://mozilla.org/test/path",
    },
    {
      shouldPass,
      filters: [{ pathEquals: "/test/path" }],
      url: "https://mozilla.org/test/path",
    },
    {
      shouldFail,
      filters: [{ pathEquals: "/wrong/path" }],
      url: "https://mozilla.org/test/path",
    },
    {
      shouldPass,
      filters: [{ pathEquals: "/test/path" }],
      url: "https://mozilla.org:8888/test/path",
    },
    // NOTE: trying at least once another valid protocol
    {
      shouldPass,
      filters: [{ pathEquals: "/test/path" }],
      url: "ftp://mozilla.org/test/path",
    },
    {
      shouldFail,
      filters: [{ pathEquals: "/wrong/path" }],
      url: "ftp://mozilla.org/test/path",
    },

    {
      shouldPass,
      filters: [{ pathContains: "st/" }],
      url: "https://mozilla.org/test/path",
    },
    {
      shouldPass,
      filters: [{ pathContains: "/test" }],
      url: "https://mozilla.org/test/path",
    },
    {
      shouldFail,
      filters: [{ pathContains: "org" }],
      url: "https://mozilla.org/test/path",
    },
    {
      shouldPass,
      filters: [{ pathContains: "" }],
      url: "https://mozilla.org/test/path",
    },
    {
      shouldPass,
      filters: [{ pathContains: null }],
      url: "https://mozilla.org/test/path",
    },
    {
      shouldFail,
      filters: [{ pathContains: "param" }],
      url: "https://mozilla.org:8888/test/path?param=1",
    },
    {
      shouldFail,
      filters: [{ pathContains: "ref" }],
      url: "https://mozilla.org:8888/test/path#ref",
    },
    {
      shouldPass,
      filters: [{ pathContains: "st/pa" }],
      url: "https://mozilla.org:8888/test/path",
    },

    {
      shouldPass,
      filters: [{ pathPrefix: "/te" }],
      url: "https://mozilla.org/test/path",
    },
    {
      shouldFail,
      filters: [{ pathPrefix: "org/" }],
      url: "https://mozilla.org/test/path",
    },
    {
      shouldPass,
      filters: [{ pathPrefix: "" }],
      url: "https://mozilla.org/test/path",
    },
    {
      shouldPass,
      filters: [{ pathPrefix: null }],
      url: "https://mozilla.org/test/path",
    },

    {
      shouldPass,
      filters: [{ pathSuffix: "/path" }],
      url: "https://mozilla.org/test/path",
    },
    {
      shouldFail,
      filters: [{ pathSuffix: "th/" }],
      url: "https://mozilla.org/test/path",
    },
    {
      shouldPass,
      filters: [{ pathSuffix: "" }],
      url: "https://mozilla.org/test/path",
    },
    {
      shouldPass,
      filters: [{ pathSuffix: null }],
      url: "https://mozilla.org/test/path",
    },
    {
      shouldFail,
      filters: [{ pathSuffix: "p=1" }],
      url: "https://mozilla.org:8888/test/path?p=1",
    },
    {
      shouldFail,
      filters: [{ pathSuffix: "ref" }],
      url: "https://mozilla.org:8888/test/path#ref",
    },

    // Query filters (queryEquals, queryContains, queryPrefix, querySuffix).
    {
      shouldFail,
      filters: [{ queryEquals: "" }],
      url: "https://mozilla.org/?param=val",
    },
    {
      shouldPass,
      filters: [{ queryEquals: null }],
      url: "https://mozilla.org/?param=val",
    },
    {
      shouldPass,
      filters: [{ queryEquals: "param=val" }],
      url: "https://mozilla.org/?param=val",
    },
    {
      shouldFail,
      filters: [{ queryEquals: "?param=val" }],
      url: "https://mozilla.org/?param=val",
    },
    {
      shouldFail,
      filters: [{ queryEquals: "/path?param=val" }],
      url: "https://mozilla.org/path?param=val",
    },

    // NOTE: about scheme urls cannot be matched by query.
    {
      shouldFail,
      filters: [{ queryEquals: "param=val" }],
      url: "about:blank?param=val",
    },
    {
      shouldFail,
      filters: [{ queryEquals: "param" }],
      url: "ftp://mozilla.org?param=val",
    },

    {
      shouldPass,
      filters: [{ queryContains: "ram" }],
      url: "https://mozilla.org/?param=val",
    },
    {
      shouldPass,
      filters: [{ queryContains: "=val" }],
      url: "https://mozilla.org/?param=val",
    },
    {
      shouldFail,
      filters: [{ queryContains: "?param" }],
      url: "https://mozilla.org/?param=val",
    },
    {
      shouldFail,
      filters: [{ queryContains: "path" }],
      url: "https://mozilla.org/path/?p=v#ref",
    },
    {
      shouldPass,
      filters: [{ queryContains: "" }],
      url: "https://mozilla.org/?param=val",
    },
    {
      shouldPass,
      filters: [{ queryContains: null }],
      url: "https://mozilla.org/?param=val",
    },

    {
      shouldPass,
      filters: [{ queryPrefix: "param" }],
      url: "https://mozilla.org/?param=val",
    },
    {
      shouldFail,
      filters: [{ queryPrefix: "p=" }],
      url: "https://mozilla.org/?param=val",
    },
    {
      shouldFail,
      filters: [{ queryPrefix: "path" }],
      url: "https://mozilla.org/path?param=val",
    },
    {
      shouldPass,
      filters: [{ queryPrefix: "" }],
      url: "https://mozilla.org/?param=val",
    },
    {
      shouldPass,
      filters: [{ queryPrefix: null }],
      url: "https://mozilla.org/?param=val",
    },

    {
      shouldPass,
      filters: [{ querySuffix: "=val" }],
      url: "https://mozilla.org/?param=val",
    },
    {
      shouldFail,
      filters: [{ querySuffix: "=wrong" }],
      url: "https://mozilla.org/?param=val",
    },
    {
      shouldPass,
      filters: [{ querySuffix: "" }],
      url: "https://mozilla.org/?param=val",
    },
    {
      shouldPass,
      filters: [{ querySuffix: null }],
      url: "https://mozilla.org/?param=val",
    },

    // URL filters (urlEquals, urlContains, urlPrefix, urlSuffix).
    {
      shouldFail,
      filters: [{ urlEquals: "" }],
      url: "https://mozilla.org/?p=v#ref",
    },
    {
      shouldPass,
      filters: [{ urlEquals: null }],
      url: "https://mozilla.org/?p=v#ref",
    },
    {
      shouldPass,
      filters: [{ urlEquals: "https://mozilla.org/?p=v#ref" }],
      url: "https://mozilla.org/?p=v#ref",
    },
    {
      shouldFail,
      filters: [{ urlEquals: "https://mozilla.org/?p=v#ref2" }],
      url: "https://mozilla.org/?p=v#ref",
    },
    {
      shouldPass,
      filters: [{ urlEquals: "about:blank?p=v#ref" }],
      url: "about:blank?p=v#ref",
    },
    {
      shouldPass,
      filters: [{ urlEquals: "ftp://mozilla.org?p=v#ref" }],
      url: "ftp://mozilla.org?p=v#ref",
    },

    {
      shouldPass,
      filters: [{ urlContains: "org/?p" }],
      url: "https://mozilla.org/?p=v#ref",
    },
    {
      shouldPass,
      filters: [{ urlContains: "=v#ref" }],
      url: "https://mozilla.org/?p=v#ref",
    },
    {
      shouldFail,
      filters: [{ urlContains: "ftp" }],
      url: "https://mozilla.org/?p=v#ref",
    },
    {
      shouldPass,
      filters: [{ urlContains: "" }],
      url: "https://mozilla.org/?p=v#ref",
    },
    {
      shouldPass,
      filters: [{ urlContains: null }],
      url: "https://mozilla.org/?p=v#ref",
    },

    {
      shouldPass,
      filters: [{ urlPrefix: "http" }],
      url: "https://mozilla.org/?p=v#ref",
    },
    {
      shouldFail,
      filters: [{ urlPrefix: "moz" }],
      url: "https://mozilla.org/?p=v#ref",
    },
    {
      shouldPass,
      filters: [{ urlPrefix: "" }],
      url: "https://mozilla.org/?p=v#ref",
    },
    {
      shouldPass,
      filters: [{ urlPrefix: null }],
      url: "https://mozilla.org/?p=v#ref",
    },

    {
      shouldPass,
      filters: [{ urlSuffix: "#ref" }],
      url: "https://mozilla.org/?p=v#ref",
    },
    {
      shouldFail,
      filters: [{ urlSuffix: "=wrong" }],
      url: "https://mozilla.org/?p=v#ref",
    },
    {
      shouldPass,
      filters: [{ urlSuffix: "" }],
      url: "https://mozilla.org/?p=v#ref",
    },
    {
      shouldPass,
      filters: [{ urlSuffix: null }],
      url: "https://mozilla.org/?p=v#ref",
    },

    // More url filters: urlMatches.
    {
      shouldPass,
      filters: [{ urlMatches: ".*://mozilla" }],
      url: "https://mozilla.org/?p=v#ref",
    },
    {
      shouldPass,
      filters: [{ urlMatches: ".*://mozilla" }],
      url: "ftp://mozilla.org/?p=v#ref",
    },
    {
      shouldPass,
      filters: [{ urlMatches: ".*://.*/?p" }],
      url: "ftp://mozilla.org/?p=v#ref",
    },
    // NOTE: urlMatches should not match the url without the ref.
    {
      shouldFail,
      filters: [{ urlMatches: "v#ref$" }],
      url: "https://mozilla.org/?p=v#ref",
    },
    {
      shouldPass,
      filters: [{ urlMatches: "^ftp" }],
      url: "ftp://mozilla.org/?p=v#ref",
    },

    // More url filters: originAndPathMatches.
    {
      shouldPass,
      filters: [{ originAndPathMatches: ".*://mozilla" }],
      url: "https://mozilla.org/?p=v#ref",
    },
    {
      shouldPass,
      filters: [{ originAndPathMatches: ".*://mozilla" }],
      url: "ftp://mozilla.org/?p=v#ref",
    },
    // NOTE: urlMatches should not match the url without the query and the ref.
    {
      shouldFail,
      filters: [{ originAndPathMatches: ".*://.*/?p" }],
      url: "ftp://mozilla.org/?p=v#ref",
    },
    {
      shouldFail,
      filters: [{ originAndPathMatches: "v#ref$" }],
      url: "https://mozilla.org/?p=v#ref",
    },
    {
      shouldPass,
      filters: [{ originAndPathMatches: "^ftp" }],
      url: "ftp://mozilla.org/?p=v#ref",
    },

    // Filter with all criteria: all matches, none matches, some matches.

    // All matches.
    {
      shouldPass,
      filters: [
        {
          schemes: ["https", "http"],
          ports: [443, 80],
          hostEquals: "www.mozilla.org",
          hostContains: ".moz",
          hostPrefix: "www",
          hostSuffix: "org",
          pathEquals: "/sub/path",
          pathContains: "b/p",
          pathPrefix: "/sub",
          pathSuffix: "/path",
          queryEquals: "p=v",
          queryContains: "1=",
          queryPrefix: "p1",
          querySuffix: "=v",
          urlEquals: "https://www.mozilla.org/sub/path?p1=v#ref",
          urlContains: "org/sub",
          urlPrefix: "https://moz",
          urlSuffix: "#ref",
          urlMatches: "v#ref$",
          originAndPathMatches: ".*://moz.*/",
        },
      ],
      url: "https://www.mozilla.org/sub/path?p1=v#ref",
    },
    // None matches.
    {
      shouldFail,
      filters: [
        {
          schemes: ["http"],
          ports: [80],
          hostEquals: "mozilla.com",
          hostContains: "www.moz",
          hostPrefix: "www",
          hostSuffix: "com",
          pathEquals: "/wrong/path",
          pathContains: "g/p",
          pathPrefix: "/wrong",
          pathSuffix: "/wrong",
          queryEquals: "p2=v",
          queryContains: "2=",
          queryPrefix: "p2",
          querySuffix: "=value",
          urlEquals: "http://mozilla.com/sub/path?p1=v#ref",
          urlContains: "com/sub",
          urlPrefix: "http://moz",
          urlSuffix: "#ref2",
          urlMatches: "value#ref2$",
          originAndPathMatches: ".*://moz.*com/",
        },
      ],
      url: "https://mozilla.org/sub/path?p1=v#ref",
    },
    // Some matches
    {
      shouldFail,
      filters: [
        {
          schemes: ["https"],
          ports: [80],
          hostEquals: "mozilla.com",
          hostContains: "www.moz",
          hostPrefix: "www",
          hostSuffix: "com",
          pathEquals: "/wrong/path",
          pathContains: "g/p",
          pathPrefix: "/wrong",
          pathSuffix: "/wrong",
          queryEquals: "p2=v",
          queryContains: "2=",
          queryPrefix: "p2",
          querySuffix: "=value",
          urlEquals: "http://mozilla.com/sub/path?p1=v#ref",
          urlContains: "com/sub",
          urlPrefix: "http://moz",
          urlSuffix: "#ref2",
          urlMatches: "value#ref2$",
          originAndPathMatches: ".*://moz.*com/",
        },
      ],
      url: "https://mozilla.org/sub/path?p1=v#ref",
    },

    // Filter with multiple filters: all matches, some matches, none matches.

    // All matches.
    {
      shouldPass,
      filters: [
        { schemes: ["https", "http"] },
        { ports: [443, 80] },
        { hostEquals: "www.mozilla.org" },
        { hostContains: ".moz" },
        { hostPrefix: "www" },
        { hostSuffix: "org" },
        { pathEquals: "/sub/path" },
        { pathContains: "b/p" },
        { pathPrefix: "/sub" },
        { pathSuffix: "/path" },
        { queryEquals: "p=v" },
        { queryContains: "1=" },
        { queryPrefix: "p1" },
        { querySuffix: "=v" },
        { urlEquals: "https://www.mozilla.org/sub/path?p1=v#ref" },
        { urlContains: "org/sub" },
        { urlPrefix: "https://moz" },
        { urlSuffix: "#ref" },
        { urlMatches: "v#ref$" },
        { originAndPathMatches: ".*://moz.*/" },
      ],
      url: "https://www.mozilla.org/sub/path?p1=v#ref",
    },

    // None matches.
    {
      shouldFail,
      filters: [
        { schemes: ["http"] },
        { ports: [80] },
        { hostEquals: "mozilla.com" },
        { hostContains: "www.moz" },
        { hostPrefix: "www" },
        { hostSuffix: "com" },
        { pathEquals: "/wrong/path" },
        { pathContains: "g/p" },
        { pathPrefix: "/wrong" },
        { pathSuffix: "/wrong" },
        { queryEquals: "p2=v" },
        { queryContains: "2=" },
        { queryPrefix: "p2" },
        { querySuffix: "=value" },
        { urlEquals: "http://mozilla.com/sub/path?p1=v#ref" },
        { urlContains: "com/sub" },
        { urlPrefix: "http://moz" },
        { urlSuffix: "#ref2" },
        { urlMatches: "value#ref2$" },
        { originAndPathMatches: ".*://moz.*com/" },
      ],
      url: "https://mozilla.org/sub/path?p1=v#ref",
    },

    // Some matches.
    {
      shouldPass,
      filters: [
        { schemes: ["https"] },
        { ports: [80] },
        { hostEquals: "mozilla.com" },
        { hostContains: "www.moz" },
        { hostPrefix: "www" },
        { hostSuffix: "com" },
        { pathEquals: "/wrong/path" },
        { pathContains: "g/p" },
        { pathPrefix: "/wrong" },
        { pathSuffix: "/wrong" },
        { queryEquals: "p2=v" },
        { queryContains: "2=" },
        { queryPrefix: "p2" },
        { querySuffix: "=value" },
        { urlEquals: "http://mozilla.com/sub/path?p1=v#ref" },
        { urlContains: "com/sub" },
        { urlPrefix: "http://moz" },
        { urlSuffix: "#ref2" },
        { urlMatches: "value#ref2$" },
        { originAndPathMatches: ".*://moz.*com/" },
      ],
      url: "https://mozilla.org/sub/path?p1=v#ref",
    },
  ];

  // Run all the the testCases defined above.
  for (let currentTest of testCases) {
    let { exceptionMessageContains, url, filters, prefs } = currentTest;

    if (prefs !== undefined) {
      for (let [name, val] of prefs) {
        Preferences.set(name, val);
      }
    }

    if (currentTest.shouldThrow) {
      expectThrow({ url, filters, exceptionMessageContains });
    } else if (currentTest.shouldFail) {
      expectFail({ url, filters });
    } else {
      expectPass({ url, filters });
    }

    if (prefs !== undefined) {
      for (let [name] of prefs) {
        Preferences.reset(name);
      }
    }
  }
});
