const ReferrerInfo = Components.Constructor("@mozilla.org/referrer-info;1",
                                            "nsIReferrerInfo",
                                            "init");

function test_policy(test) {
  info("Running test: " + test.toSource());

  var prefs = Cc["@mozilla.org/preferences-service;1"]
    .getService(Ci.nsIPrefBranch);
  if (test.defaultReferrerPolicyPref !== undefined) {
    prefs.setIntPref("network.http.referer.defaultPolicy",
                     test.defaultReferrerPolicyPref);
  } else {
    prefs.setIntPref("network.http.referer.defaultPolicy", 3);
  }

  var uri = NetUtil.newURI(test.url)
  var chan = NetUtil.newChannel({
    uri: uri,
    loadUsingSystemPrincipal: true
  });

  var referrer = NetUtil.newURI(test.referrer);
  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.referrerInfo = new ReferrerInfo(test.policy, true, referrer);

  if (test.expectedReferrerSpec === undefined) {
    try {
      chan.getRequestHeader("Referer");
      do_throw("Should not find a Referer header!");
    } catch(e) {
    }
  } else {
    var header = chan.getRequestHeader("Referer");
    Assert.equal(header, test.expectedReferrerSpec);
  }
}

const nsIHttpChannel = Ci.nsIHttpChannel;
// Assuming cross origin because we have no triggering principal available
var gTests = [
  {
    policy: nsIHttpChannel.REFERRER_POLICY_UNSET,
    defaultReferrerPolicyPref: 0,
    url: "https://test.example/foo",
    referrer: "https://test.example/referrer",
    expectedReferrerSpec: undefined
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_UNSET,
    defaultReferrerPolicyPref: 1,
    url: "http://test.example/foo",
    referrer: "http://test1.example/referrer",
    expectedReferrerSpec: undefined
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_UNSET,
    defaultReferrerPolicyPref: 2,
    url: "https://sub1.\xe4lt.example/foo",
    referrer: "https://sub1.\xe4lt.example/referrer",
    expectedReferrerSpec: "https://sub1.xn--lt-uia.example/"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_UNSET,
    defaultReferrerPolicyPref: 2,
    url: "https://test.example/foo",
    referrer: "https://test1.example/referrer",
    expectedReferrerSpec: "https://test1.example/"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_UNSET,
    defaultReferrerPolicyPref: 3,
    url: "https://test.example/foo",
    referrer: "https://test.example/referrer",
    expectedReferrerSpec: "https://test.example/referrer"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_UNSET,
    defaultReferrerPolicyPref: 3,
    url: "https://sub1.\xe4lt.example/foo",
    referrer: "https://sub1.\xe4lt.example/referrer",
    expectedReferrerSpec: "https://sub1.xn--lt-uia.example/referrer"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_UNSET,
    defaultReferrerPolicyPref: 3,
    url: "http://test.example/foo",
    referrer: "https://test.example/referrer",
    expectedReferrerSpec: undefined
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_NO_REFERRER,
    url: "https://test.example/foo",
    referrer: "https://test.example/referrer",
    expectedReferrerSpec: undefined
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_ORIGIN,
    url: "https://test.example/foo",
    referrer: "https://test.example/referrer",
    expectedReferrerSpec: "https://test.example/"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_ORIGIN,
    url: "https://sub1.\xe4lt.example/foo",
    referrer: "https://sub1.\xe4lt.example/referrer",
    expectedReferrerSpec: "https://sub1.xn--lt-uia.example/"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_UNSAFE_URL,
    url: "https://test.example/foo",
    referrer: "https://test.example/referrer",
    expectedReferrerSpec: "https://test.example/referrer"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_UNSAFE_URL,
    url: "https://sub1.\xe4lt.example/foo",
    referrer: "https://sub1.\xe4lt.example/referrer",
    expectedReferrerSpec: "https://sub1.xn--lt-uia.example/referrer"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_UNSAFE_URL,
    url: "http://test.example/foo",
    referrer: "https://test.example/referrer",
    expectedReferrerSpec: "https://test.example/referrer"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_UNSAFE_URL,
    url: "http://sub1.\xe4lt.example/foo",
    referrer: "https://sub1.\xe4lt.example/referrer",
    expectedReferrerSpec: "https://sub1.xn--lt-uia.example/referrer"
  },
];

function run_test() {
  gTests.forEach(test => test_policy(test));
}
