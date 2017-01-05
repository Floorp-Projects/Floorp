Cu.import("resource://gre/modules/NetUtil.jsm");

function test_policy(test) {
  do_print("Running test: " + test.toSource());

  var uri = NetUtil.newURI(test.url, "", null)
  var chan = NetUtil.newChannel({
    uri: uri,
    loadUsingSystemPrincipal: true
  });

  var referrer = NetUtil.newURI(test.referrer, "", null);
  chan.QueryInterface(Components.interfaces.nsIHttpChannel);
  chan.setReferrerWithPolicy(referrer, test.policy);
  if (test.expectedReferrerSpec === undefined) {
    try {
      chan.getRequestHeader("Referer");
      do_throw("Should not find a Referer header!");
    } catch(e) {
    }
    do_check_eq(chan.referrer, null);
  } else {
    var header = chan.getRequestHeader("Referer");
    do_check_eq(header, test.expectedReferrerSpec);
    do_check_eq(chan.referrer.asciiSpec, test.expectedReferrerSpec);
  }
}

const nsIHttpChannel = Ci.nsIHttpChannel;
var gTests = [
  {
    policy: nsIHttpChannel.REFERRER_POLICY_UNSET,
    url: "https://test.example/foo",
    referrer: "https://test.example/referrer",
    expectedReferrerSpec: "https://test.example/referrer"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_UNSET,
    url: "https://sub1.\xe4lt.example/foo",
    referrer: "https://sub1.\xe4lt.example/referrer",
    expectedReferrerSpec: "https://sub1.xn--lt-uia.example/referrer"
  },
  {
    policy: nsIHttpChannel.REFERRER_POLICY_UNSET,
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
