/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function test_policy(test) {
  do_print("Running test: " + test.toSource());

  let prefs = Services.prefs;

  if (test.trimmingPolicy !== undefined) {
    prefs.setIntPref("network.http.referer.trimmingPolicy",
                     test.trimmingPolicy);
  } else {
    prefs.setIntPref("network.http.referer.trimmingPolicy", 0);
  }

  if (test.XOriginTrimmingPolicy !== undefined) {
    prefs.setIntPref("network.http.referer.XOriginTrimmingPolicy",
                     test.XOriginTrimmingPolicy);
  } else {
    prefs.setIntPref("network.http.referer.XOriginTrimmingPolicy", 0);
  }

  let referrer = NetUtil.newURI(test.referrer);
  let triggeringPrincipal = Services.scriptSecurityManager.createCodebasePrincipal(referrer, {});
  let chan = NetUtil.newChannel({
    uri: test.url,
    loadingPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    triggeringPrincipal: triggeringPrincipal,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER
  });

  chan.QueryInterface(Components.interfaces.nsIHttpChannel);
  chan.setReferrerWithPolicy(referrer, test.policy);
  if (test.expectedReferrerSpec === undefined) {
    try {
      chan.getRequestHeader("Referer");
      do_throw("Should not find a Referer header!");
    } catch(e) {
    }
    Assert.equal(chan.referrer, null);
  } else {
    let header = chan.getRequestHeader("Referer");
    Assert.equal(header, test.expectedReferrerSpec);
    Assert.equal(chan.referrer.asciiSpec, test.expectedReferrerSpec);
  }
}

const nsIHttpChannel = Ci.nsIHttpChannel;
var gTests = [
  // Test same origin policy w/o cross origin
  {
    policy: nsIHttpChannel.REFERRER_POLICY_SAME_ORIGIN,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo?a"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_SAME_ORIGIN,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: undefined
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_SAME_ORIGIN,
    trimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_SAME_ORIGIN,
    trimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: undefined
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_SAME_ORIGIN,
    trimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_SAME_ORIGIN,
    trimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: undefined
  },

  // Test origin when xorigin policy w/o cross origin
  {
    policy: nsIHttpChannel.REFERRER_POLICY_ORIGIN_WHEN_XORIGIN,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo?a"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_ORIGIN_WHEN_XORIGIN,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: "https://foo.example/"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_ORIGIN_WHEN_XORIGIN,
    trimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_ORIGIN_WHEN_XORIGIN,
    trimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: "https://foo.example/"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_ORIGIN_WHEN_XORIGIN,
    trimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_ORIGIN_WHEN_XORIGIN,
    trimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: "https://foo.example/"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_ORIGIN_WHEN_XORIGIN,
    XOriginTrimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo?a"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_ORIGIN_WHEN_XORIGIN,
    XOriginTrimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: "https://foo.example/"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_ORIGIN_WHEN_XORIGIN,
    XOriginTrimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo?a"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_ORIGIN_WHEN_XORIGIN,
    XOriginTrimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: "https://foo.example/"
  },

  // Test strict origin when xorigin policy w/o cross origin
  {
    policy: nsIHttpChannel.REFERRER_POLICY_STRICT_ORIGIN_WHEN_XORIGIN,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo?a"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_STRICT_ORIGIN_WHEN_XORIGIN,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: "https://foo.example/"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_STRICT_ORIGIN_WHEN_XORIGIN,
    url: "http://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: undefined
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_STRICT_ORIGIN_WHEN_XORIGIN,
    trimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_STRICT_ORIGIN_WHEN_XORIGIN,
    trimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: "https://foo.example/"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_STRICT_ORIGIN_WHEN_XORIGIN,
    trimmingPolicy: 1,
    url: "http://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: undefined
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_STRICT_ORIGIN_WHEN_XORIGIN,
    trimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_STRICT_ORIGIN_WHEN_XORIGIN,
    trimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: "https://foo.example/"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_STRICT_ORIGIN_WHEN_XORIGIN,
    trimmingPolicy: 2,
    url: "http://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: undefined
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_STRICT_ORIGIN_WHEN_XORIGIN,
    XOriginTrimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo?a"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_STRICT_ORIGIN_WHEN_XORIGIN,
    XOriginTrimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: "https://foo.example/"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_STRICT_ORIGIN_WHEN_XORIGIN,
    XOriginTrimmingPolicy: 1,
    url: "http://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: undefined
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_STRICT_ORIGIN_WHEN_XORIGIN,
    XOriginTrimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo?a"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_STRICT_ORIGIN_WHEN_XORIGIN,
    XOriginTrimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: "https://foo.example/"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_STRICT_ORIGIN_WHEN_XORIGIN,
    XOriginTrimmingPolicy: 2,
    url: "http://test.example/foo?a",
    referrer: "https://foo.example/foo?a",
    expectedReferrerSpec: undefined
  },

  // Test mix and choose max of XOriginTrimmingPolicy and trimmingPolicy
  {
    policy: nsIHttpChannel.REFERRER_POLICY_UNSAFE_URL,
    XOriginTrimmingPolicy: 2,
    trimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://test1.example/foo?a",
    expectedReferrerSpec: "https://test1.example/"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_UNSAFE_URL,
    XOriginTrimmingPolicy: 2,
    trimmingPolicy: 1,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/foo"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_UNSAFE_URL,
    XOriginTrimmingPolicy: 1,
    trimmingPolicy: 2,
    url: "https://test.example/foo?a",
    referrer: "https://test.example/foo?a",
    expectedReferrerSpec: "https://test.example/"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_UNSAFE_URL,
    XOriginTrimmingPolicy: 1,
    trimmingPolicy: 0,
    url: "https://test.example/foo?a",
    referrer: "https://test1.example/foo?a",
    expectedReferrerSpec: "https://test1.example/foo"
  },

];

function run_test() {
  gTests.forEach(test => test_policy(test));
  Services.prefs.clearUserPref("network.http.referer.trimmingPolicy");
  Services.prefs.clearUserPref("network.http.referer.XOriginTrimmingPolicy");
}
