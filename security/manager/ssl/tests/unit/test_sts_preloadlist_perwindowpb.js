"use strict";

var gSSService = Cc["@mozilla.org/ssservice;1"].getService(
  Ci.nsISiteSecurityService
);

function Observer() {}
Observer.prototype = {
  observe(subject, topic, data) {
    if (topic == "last-pb-context-exited") {
      run_next_test();
    }
  },
};

var gObserver = new Observer();
var secInfo = Cc[
  "@mozilla.org/security/transportsecurityinfo;1"
].createInstance(Ci.nsITransportSecurityInfo);

function cleanup() {
  Services.obs.removeObserver(gObserver, "last-pb-context-exited");
  gSSService.clearAll();
}

function run_test() {
  do_get_profile();

  registerCleanupFunction(cleanup);
  Services.obs.addObserver(gObserver, "last-pb-context-exited");

  add_test(test_part1);
  add_test(test_private_browsing1);
  add_test(test_private_browsing2);

  run_next_test();
}

function test_part1() {
  // check that a host not in the list is not identified as an sts host
  ok(
    !gSSService.isSecureURI(
      Services.io.newURI("https://nonexistent.example.com"),
      0
    )
  );

  // check that an ancestor domain is not identified as an sts host
  ok(!gSSService.isSecureURI(Services.io.newURI("https://com"), 0));

  // check that the pref to toggle using the preload list works
  Services.prefs.setBoolPref(
    "network.stricttransportsecurity.preloadlist",
    false
  );
  ok(
    !gSSService.isSecureURI(
      Services.io.newURI("https://includesubdomains.preloaded.test"),
      0
    )
  );
  Services.prefs.setBoolPref(
    "network.stricttransportsecurity.preloadlist",
    true
  );
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://includesubdomains.preloaded.test"),
      0
    )
  );

  // check that a subdomain is an sts host (includeSubdomains is set)
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://subdomain.includesubdomains.preloaded.test"),
      0
    )
  );

  // check that another subdomain is an sts host (includeSubdomains is set)
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://a.b.c.def.includesubdomains.preloaded.test"),
      0
    )
  );

  // check that a subdomain is not an sts host (includeSubdomains is not set)
  ok(
    !gSSService.isSecureURI(
      Services.io.newURI(
        "https://subdomain.noincludesubdomains.preloaded.test"
      ),
      0
    )
  );

  // check that a host with a dot on the end won't break anything
  ok(
    !gSSService.isSecureURI(
      Services.io.newURI("https://notsts.nonexistent.example.com."),
      0
    )
  );

  // check that processing a header with max-age: 0 will remove a preloaded
  // site from the list
  let uri = Services.io.newURI("https://includesubdomains.preloaded.test");
  let subDomainUri = Services.io.newURI(
    "https://subdomain.includesubdomains.preloaded.test"
  );
  gSSService.processHeader(
    uri,
    "max-age=0",
    secInfo,
    0,
    Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
  );
  ok(!gSSService.isSecureURI(uri, 0));
  ok(!gSSService.isSecureURI(subDomainUri, 0));
  // check that processing another header (with max-age non-zero) will
  // re-enable a site's sts status
  gSSService.processHeader(
    uri,
    "max-age=1000",
    secInfo,
    0,
    Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
  );
  ok(gSSService.isSecureURI(uri, 0));
  // but this time include subdomains was not set, so test for that
  ok(!gSSService.isSecureURI(subDomainUri, 0));
  gSSService.clearAll();

  // check that processing a header with max-age: 0 from a subdomain of a site
  // will not remove that (ancestor) site from the list
  uri = Services.io.newURI(
    "https://subdomain.noincludesubdomains.preloaded.test"
  );
  gSSService.processHeader(
    uri,
    "max-age=0",
    secInfo,
    0,
    Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
  );
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://noincludesubdomains.preloaded.test"),
      0
    )
  );
  ok(!gSSService.isSecureURI(uri, 0));

  uri = Services.io.newURI(
    "https://subdomain.includesubdomains.preloaded.test"
  );
  gSSService.processHeader(
    uri,
    "max-age=0",
    secInfo,
    0,
    Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
  );
  // we received a header with "max-age=0", so we have "no information"
  // regarding the sts state of subdomain.includesubdomains.preloaded.test specifically,
  // but it is actually still an STS host, because of the preloaded
  // includesubdomains.preloaded.test including subdomains.
  // Here's a drawing:
  // |-- includesubdomains.preloaded.test (in preload list, includes subdomains) IS sts host
  //     |-- subdomain.includesubdomains.preloaded.test                          IS sts host
  //     |   `-- another.subdomain.includesubdomains.preloaded.test              IS sts host
  //     `-- sibling.includesubdomains.preloaded.test                            IS sts host
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://includesubdomains.preloaded.test"),
      0
    )
  );
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://subdomain.includesubdomains.preloaded.test"),
      0
    )
  );
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://sibling.includesubdomains.preloaded.test"),
      0
    )
  );
  ok(
    gSSService.isSecureURI(
      Services.io.newURI(
        "https://another.subdomain.includesubdomains.preloaded.test"
      ),
      0
    )
  );

  gSSService.processHeader(
    uri,
    "max-age=1000",
    secInfo,
    0,
    Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
  );
  // Here's what we have now:
  // |-- includesubdomains.preloaded.test (in preload list, includes subdomains) IS sts host
  //     |-- subdomain.includesubdomains.preloaded.test (include subdomains is false) IS sts host
  //     |   `-- another.subdomain.includesubdomains.preloaded.test              IS NOT sts host
  //     `-- sibling.includesubdomains.preloaded.test                            IS sts host
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://subdomain.includesubdomains.preloaded.test"),
      0
    )
  );
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://sibling.includesubdomains.preloaded.test"),
      0
    )
  );
  ok(
    !gSSService.isSecureURI(
      Services.io.newURI(
        "https://another.subdomain.includesubdomains.preloaded.test"
      ),
      0
    )
  );

  // Test that an expired non-private browsing entry results in correctly
  // identifying a host that is on the preload list as no longer sts.
  // (This happens when we're in regular browsing mode, we get a header from
  // a site on the preload list, and that header later expires. We need to
  // then treat that host as no longer an sts host.)
  // (sanity check first - this should be in the preload list)
  uri = Services.io.newURI("https://includesubdomains2.preloaded.test");
  ok(gSSService.isSecureURI(uri, 0));
  gSSService.processHeader(
    uri,
    "max-age=1",
    secInfo,
    0,
    Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
  );
  do_timeout(1250, function() {
    ok(!gSSService.isSecureURI(uri, 0));
    run_next_test();
  });
}

const IS_PRIVATE = Ci.nsISocketProvider.NO_PERMANENT_STORAGE;

function test_private_browsing1() {
  gSSService.clearAll();
  let uri = Services.io.newURI("https://includesubdomains.preloaded.test");
  let subDomainUri = Services.io.newURI(
    "https://a.b.c.subdomain.includesubdomains.preloaded.test"
  );
  // sanity - includesubdomains.preloaded.test is preloaded, includeSubdomains set
  ok(gSSService.isSecureURI(uri, IS_PRIVATE));
  ok(gSSService.isSecureURI(subDomainUri, IS_PRIVATE));

  gSSService.processHeader(
    uri,
    "max-age=0",
    secInfo,
    IS_PRIVATE,
    Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
  );
  ok(!gSSService.isSecureURI(uri, IS_PRIVATE));
  ok(!gSSService.isSecureURI(subDomainUri, IS_PRIVATE));

  // check adding it back in
  gSSService.processHeader(
    uri,
    "max-age=1000",
    secInfo,
    IS_PRIVATE,
    Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
  );
  ok(gSSService.isSecureURI(uri, IS_PRIVATE));
  // but no includeSubdomains this time
  ok(!gSSService.isSecureURI(subDomainUri, IS_PRIVATE));

  // do the hokey-pokey...
  gSSService.processHeader(
    uri,
    "max-age=0",
    secInfo,
    IS_PRIVATE,
    Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
  );
  ok(!gSSService.isSecureURI(uri, IS_PRIVATE));
  ok(!gSSService.isSecureURI(subDomainUri, IS_PRIVATE));

  // Test that an expired private browsing entry results in correctly
  // identifying a host that is on the preload list as no longer sts.
  // (This happens when we're in private browsing mode, we get a header from
  // a site on the preload list, and that header later expires. We need to
  // then treat that host as no longer an sts host.)
  // (sanity check first - this should be in the preload list)
  uri = Services.io.newURI("https://includesubdomains2.preloaded.test");
  ok(gSSService.isSecureURI(uri, IS_PRIVATE));
  gSSService.processHeader(
    uri,
    "max-age=1",
    secInfo,
    IS_PRIVATE,
    Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
  );
  do_timeout(1250, function() {
    ok(!gSSService.isSecureURI(uri, IS_PRIVATE));
    // Simulate leaving private browsing mode
    Services.obs.notifyObservers(null, "last-pb-context-exited");
  });
}

function test_private_browsing2() {
  // if this test gets this far, it means there's a private browsing service
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://includesubdomains.preloaded.test"),
      0
    )
  );
  // the includesubdomains.preloaded.test entry has includeSubdomains set
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://subdomain.includesubdomains.preloaded.test"),
      0
    )
  );

  // Now that we're out of private browsing mode, we need to make sure
  // we've "forgotten" that we "forgot" this site's sts status.
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://includesubdomains2.preloaded.test"),
      0
    )
  );

  run_next_test();
}
