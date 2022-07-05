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
      Services.io.newURI("https://nonexistent.example.com")
    )
  );

  // check that an ancestor domain is not identified as an sts host
  ok(!gSSService.isSecureURI(Services.io.newURI("https://com")));

  // check that the pref to toggle using the preload list works
  Services.prefs.setBoolPref(
    "network.stricttransportsecurity.preloadlist",
    false
  );
  ok(
    !gSSService.isSecureURI(
      Services.io.newURI("https://includesubdomains.preloaded.test")
    )
  );
  Services.prefs.setBoolPref(
    "network.stricttransportsecurity.preloadlist",
    true
  );
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://includesubdomains.preloaded.test")
    )
  );

  // check that a subdomain is an sts host (includeSubdomains is set)
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://subdomain.includesubdomains.preloaded.test")
    )
  );

  // check that another subdomain is an sts host (includeSubdomains is set)
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://a.b.c.def.includesubdomains.preloaded.test")
    )
  );

  // check that a subdomain is not an sts host (includeSubdomains is not set)
  ok(
    !gSSService.isSecureURI(
      Services.io.newURI("https://subdomain.noincludesubdomains.preloaded.test")
    )
  );

  // check that a host with a dot on the end won't break anything
  ok(
    !gSSService.isSecureURI(
      Services.io.newURI("https://notsts.nonexistent.example.com.")
    )
  );

  // check that processing a header with max-age: 0 will remove a preloaded
  // site from the list
  let uri = Services.io.newURI("https://includesubdomains.preloaded.test");
  let subDomainUri = Services.io.newURI(
    "https://subdomain.includesubdomains.preloaded.test"
  );
  gSSService.processHeader(uri, "max-age=0", secInfo);
  ok(!gSSService.isSecureURI(uri));
  ok(!gSSService.isSecureURI(subDomainUri));
  // check that processing another header (with max-age non-zero) will
  // re-enable a site's sts status
  gSSService.processHeader(uri, "max-age=1000", secInfo);
  ok(gSSService.isSecureURI(uri));
  // but this time include subdomains was not set, so test for that
  ok(!gSSService.isSecureURI(subDomainUri));
  gSSService.clearAll();

  // check that processing a header with max-age: 0 from a subdomain of a site
  // will not remove that (ancestor) site from the list
  uri = Services.io.newURI(
    "https://subdomain.noincludesubdomains.preloaded.test"
  );
  gSSService.processHeader(uri, "max-age=0", secInfo);
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://noincludesubdomains.preloaded.test")
    )
  );
  ok(!gSSService.isSecureURI(uri));

  uri = Services.io.newURI(
    "https://subdomain.includesubdomains.preloaded.test"
  );
  gSSService.processHeader(uri, "max-age=0", secInfo);
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
      Services.io.newURI("https://includesubdomains.preloaded.test")
    )
  );
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://subdomain.includesubdomains.preloaded.test")
    )
  );
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://sibling.includesubdomains.preloaded.test")
    )
  );
  ok(
    gSSService.isSecureURI(
      Services.io.newURI(
        "https://another.subdomain.includesubdomains.preloaded.test"
      )
    )
  );

  gSSService.processHeader(uri, "max-age=1000", secInfo);
  // Here's what we have now:
  // |-- includesubdomains.preloaded.test (in preload list, includes subdomains) IS sts host
  //     |-- subdomain.includesubdomains.preloaded.test (include subdomains is false) IS sts host
  //     |   `-- another.subdomain.includesubdomains.preloaded.test              IS NOT sts host
  //     `-- sibling.includesubdomains.preloaded.test                            IS sts host
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://subdomain.includesubdomains.preloaded.test")
    )
  );
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://sibling.includesubdomains.preloaded.test")
    )
  );
  ok(
    !gSSService.isSecureURI(
      Services.io.newURI(
        "https://another.subdomain.includesubdomains.preloaded.test"
      )
    )
  );

  // Test that an expired non-private browsing entry results in correctly
  // identifying a host that is on the preload list as no longer sts.
  // (This happens when we're in regular browsing mode, we get a header from
  // a site on the preload list, and that header later expires. We need to
  // then treat that host as no longer an sts host.)
  // (sanity check first - this should be in the preload list)
  uri = Services.io.newURI("https://includesubdomains2.preloaded.test");
  ok(gSSService.isSecureURI(uri));
  gSSService.processHeader(uri, "max-age=1", secInfo);
  do_timeout(1250, function() {
    ok(!gSSService.isSecureURI(uri));
    run_next_test();
  });
}

const PRIVATE_ORIGIN_ATTRIBUTES = { privateBrowsingId: 1 };

function test_private_browsing1() {
  gSSService.clearAll();
  let uri = Services.io.newURI("https://includesubdomains.preloaded.test");
  let subDomainUri = Services.io.newURI(
    "https://a.b.c.subdomain.includesubdomains.preloaded.test"
  );
  // sanity - includesubdomains.preloaded.test is preloaded, includeSubdomains set
  ok(gSSService.isSecureURI(uri, PRIVATE_ORIGIN_ATTRIBUTES));
  ok(gSSService.isSecureURI(subDomainUri, PRIVATE_ORIGIN_ATTRIBUTES));

  gSSService.processHeader(
    uri,
    "max-age=0",
    secInfo,
    PRIVATE_ORIGIN_ATTRIBUTES
  );
  ok(!gSSService.isSecureURI(uri, PRIVATE_ORIGIN_ATTRIBUTES));
  ok(!gSSService.isSecureURI(subDomainUri, PRIVATE_ORIGIN_ATTRIBUTES));

  // check adding it back in
  gSSService.processHeader(
    uri,
    "max-age=1000",
    secInfo,
    PRIVATE_ORIGIN_ATTRIBUTES
  );
  ok(gSSService.isSecureURI(uri, PRIVATE_ORIGIN_ATTRIBUTES));
  // but no includeSubdomains this time
  ok(!gSSService.isSecureURI(subDomainUri, PRIVATE_ORIGIN_ATTRIBUTES));

  // do the hokey-pokey...
  gSSService.processHeader(
    uri,
    "max-age=0",
    secInfo,
    PRIVATE_ORIGIN_ATTRIBUTES
  );
  ok(!gSSService.isSecureURI(uri, PRIVATE_ORIGIN_ATTRIBUTES));
  ok(!gSSService.isSecureURI(subDomainUri, PRIVATE_ORIGIN_ATTRIBUTES));

  // Test that an expired private browsing entry results in correctly
  // identifying a host that is on the preload list as no longer sts.
  // (This happens when we're in private browsing mode, we get a header from
  // a site on the preload list, and that header later expires. We need to
  // then treat that host as no longer an sts host.)
  // (sanity check first - this should be in the preload list)
  uri = Services.io.newURI("https://includesubdomains2.preloaded.test");
  ok(gSSService.isSecureURI(uri, PRIVATE_ORIGIN_ATTRIBUTES));
  gSSService.processHeader(
    uri,
    "max-age=1",
    secInfo,
    PRIVATE_ORIGIN_ATTRIBUTES
  );
  do_timeout(1250, function() {
    ok(!gSSService.isSecureURI(uri, PRIVATE_ORIGIN_ATTRIBUTES));
    // Simulate leaving private browsing mode
    Services.obs.notifyObservers(null, "last-pb-context-exited");
  });
}

function test_private_browsing2() {
  // if this test gets this far, it means there's a private browsing service
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://includesubdomains.preloaded.test")
    )
  );
  // the includesubdomains.preloaded.test entry has includeSubdomains set
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://subdomain.includesubdomains.preloaded.test")
    )
  );

  // Now that we're out of private browsing mode, we need to make sure
  // we've "forgotten" that we "forgot" this site's sts status.
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://includesubdomains2.preloaded.test")
    )
  );

  run_next_test();
}
