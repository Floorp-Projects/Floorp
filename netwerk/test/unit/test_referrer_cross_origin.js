/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const ReferrerInfo = Components.Constructor(
  "@mozilla.org/referrer-info;1",
  "nsIReferrerInfo",
  "init"
);

function test_policy(test) {
  info("Running test: " + test.toSource());

  let prefs = Services.prefs;

  if (test.trimmingPolicy !== undefined) {
    prefs.setIntPref(
      "network.http.referer.trimmingPolicy",
      test.trimmingPolicy
    );
  } else {
    prefs.setIntPref("network.http.referer.trimmingPolicy", 0);
  }

  if (test.XOriginTrimmingPolicy !== undefined) {
    prefs.setIntPref(
      "network.http.referer.XOriginTrimmingPolicy",
      test.XOriginTrimmingPolicy
    );
  } else {
    prefs.setIntPref("network.http.referer.XOriginTrimmingPolicy", 0);
  }

  let referrer = NetUtil.newURI(test.referrer);
  let triggeringPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    referrer,
    {}
  );
  let chan = NetUtil.newChannel({
    uri: test.url,
    loadingPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    triggeringPrincipal,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
  });

  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.referrerInfo = new ReferrerInfo(test.policy, true, referrer);

  if (test.expectedReferrerSpec === undefined) {
    try {
      chan.getRequestHeader("Referer");
      do_throw("Should not find a Referer header!");
    } catch (e) {}
  } else {
    let header = chan.getRequestHeader("Referer");
    Assert.equal(header, test.expectedReferrerSpec);
  }
}

const nsIReferrerInfo = Ci.nsIReferrerInfo;
var gTests = [
  // Test same origin policy w/o cross origin
  {
    policy: nsIReferrerInfo.SAME_ORIGIN,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo?a",
  },
  {
    policy: nsIReferrerInfo.SAME_ORIGIN,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: undefined,
  },
  {
    policy: nsIReferrerInfo.SAME_ORIGIN,
    trimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo",
  },
  {
    policy: nsIReferrerInfo.SAME_ORIGIN,
    trimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: undefined,
  },
  {
    policy: nsIReferrerInfo.SAME_ORIGIN,
    trimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/",
  },
  {
    policy: nsIReferrerInfo.SAME_ORIGIN,
    trimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: undefined,
  },

  // Test origin when xorigin policy w/o cross origin
  {
    policy: nsIReferrerInfo.ORIGIN_WHEN_CROSS_ORIGIN,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo?a",
  },
  {
    policy: nsIReferrerInfo.ORIGIN_WHEN_CROSS_ORIGIN,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: "https://foo.example/",
  },
  {
    policy: nsIReferrerInfo.ORIGIN_WHEN_CROSS_ORIGIN,
    trimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo",
  },
  {
    policy: nsIReferrerInfo.ORIGIN_WHEN_CROSS_ORIGIN,
    trimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: "https://foo.example/",
  },
  {
    policy: nsIReferrerInfo.ORIGIN_WHEN_CROSS_ORIGIN,
    trimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/",
  },
  {
    policy: nsIReferrerInfo.ORIGIN_WHEN_CROSS_ORIGIN,
    trimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: "https://foo.example/",
  },
  {
    policy: nsIReferrerInfo.ORIGIN_WHEN_CROSS_ORIGIN,
    XOriginTrimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo?a",
  },
  {
    policy: nsIReferrerInfo.ORIGIN_WHEN_CROSS_ORIGIN,
    XOriginTrimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: "https://foo.example/",
  },
  {
    policy: nsIReferrerInfo.ORIGIN_WHEN_CROSS_ORIGIN,
    XOriginTrimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo?a",
  },
  {
    policy: nsIReferrerInfo.ORIGIN_WHEN_CROSS_ORIGIN,
    XOriginTrimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: "https://foo.example/",
  },

  // Test strict origin when xorigin policy w/o cross origin
  {
    policy: nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo?a",
  },
  {
    policy: nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: "https://foo.example/",
  },
  {
    policy: nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
    url: "http://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: undefined,
  },
  {
    policy: nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
    trimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo",
  },
  {
    policy: nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
    trimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: "https://foo.example/",
  },
  {
    policy: nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
    trimmingPolicy: 1,
    url: "http://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: undefined,
  },
  {
    policy: nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
    trimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/",
  },
  {
    policy: nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
    trimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: "https://foo.example/",
  },
  {
    policy: nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
    trimmingPolicy: 2,
    url: "http://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: undefined,
  },
  {
    policy: nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
    XOriginTrimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo?a",
  },
  {
    policy: nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
    XOriginTrimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: "https://foo.example/",
  },
  {
    policy: nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
    XOriginTrimmingPolicy: 1,
    url: "http://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: undefined,
  },
  {
    policy: nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
    XOriginTrimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo?a",
  },
  {
    policy: nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
    XOriginTrimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: "https://foo.example/",
  },
  {
    policy: nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
    XOriginTrimmingPolicy: 2,
    url: "http://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: undefined,
  },

  // Test mix and choose max of XOriginTrimmingPolicy and trimmingPolicy
  {
    policy: nsIReferrerInfo.UNSAFE_URL,
    XOriginTrimmingPolicy: 2,
    trimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://test1.example/foo?a",
    expectedReferrerSpec: "https://test1.example/",
  },
  {
    policy: nsIReferrerInfo.UNSAFE_URL,
    XOriginTrimmingPolicy: 2,
    trimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo",
  },
  {
    policy: nsIReferrerInfo.UNSAFE_URL,
    XOriginTrimmingPolicy: 1,
    trimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/",
  },
  {
    policy: nsIReferrerInfo.UNSAFE_URL,
    XOriginTrimmingPolicy: 1,
    trimmingPolicy: 0,
    url: "https://test.example/foo?a",
    referrer: "https://test1.example/foo?a",
    expectedReferrerSpec: "https://test1.example/foo",
  },
];

function run_test() {
  gTests.forEach(test => test_policy(test));
  Services.prefs.clearUserPref("network.http.referer.trimmingPolicy");
  Services.prefs.clearUserPref("network.http.referer.XOriginTrimmingPolicy");
}
