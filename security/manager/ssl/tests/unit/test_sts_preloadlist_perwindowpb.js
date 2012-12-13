var Cc = Components.classes;
var Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/Services.jsm");

var gSTSService = Cc["@mozilla.org/stsservice;1"]
                  .getService(Ci.nsIStrictTransportSecurityService);

function Observer() {}
Observer.prototype = {
  observe: function(subject, topic, data) {
    if (topic == "last-pb-context-exited")
      run_next_test();
  }
};

var gObserver = new Observer();

// This is a list of every host we call processStsHeader with
// (we have to remove any state added to the sts service so as to not muck
// with other tests).
var hosts = ["http://keyerror.com", "http://subdomain.intercom.io",
             "http://subdomain.pixi.me", "http://crypto.cat",
             "http://logentries.com"];

function cleanup() {
  Services.obs.removeObserver(gObserver, "last-pb-context-exited");

  for (var host of hosts) {
    var uri = Services.io.newURI(host, null, null);
    gSTSService.removeStsState(uri, 0);
  }
}

function run_test() {
  do_register_cleanup(cleanup);
  Services.obs.addObserver(gObserver, "last-pb-context-exited", false);

  add_test(test_part1);
  add_test(test_private_browsing1);
  add_test(test_private_browsing2);

  run_next_test();
}

function test_part1() {
  // check that a host not in the list is not identified as an sts host
  do_check_false(gSTSService.isStsHost("nonexistent.mozilla.com", 0));

  // check that an ancestor domain is not identified as an sts host
  do_check_false(gSTSService.isStsHost("com", 0));

  // Note: the following were taken from the STS preload list
  // as of Sept. 2012. If the list changes, this test will need to be modified.
  // check that the pref to toggle using the preload list works
  Services.prefs.setBoolPref("network.stricttransportsecurity.preloadlist", false);
  do_check_false(gSTSService.isStsHost("factor.cc", 0));
  Services.prefs.setBoolPref("network.stricttransportsecurity.preloadlist", true);
  do_check_true(gSTSService.isStsHost("factor.cc", 0));

  // check that an entry at the beginning of the list is an sts host
  do_check_true(gSTSService.isStsHost("arivo.com.br", 0));

  // check that a subdomain is an sts host (includeSubdomains is set)
  do_check_true(gSTSService.isStsHost("subdomain.arivo.com.br", 0));

  // check that another subdomain is an sts host (includeSubdomains is set)
  do_check_true(gSTSService.isStsHost("a.b.c.subdomain.arivo.com.br", 0));

  // check that an entry in the middle of the list is an sts host
  do_check_true(gSTSService.isStsHost("neg9.org", 0));

  // check that a subdomain is not an sts host (includeSubdomains is not set)
  do_check_false(gSTSService.isStsHost("subdomain.neg9.org", 0));

  // check that an entry at the end of the list is an sts host
  do_check_true(gSTSService.isStsHost("www.noisebridge.net", 0));

  // check that a subdomain is not an sts host (includeSubdomains is not set)
  do_check_false(gSTSService.isStsHost("a.subdomain.www.noisebridge.net", 0));

  // check that a host with a dot on the end won't break anything
  do_check_false(gSTSService.isStsHost("notsts.nonexistent.mozilla.com.", 0));

  // check that processing a header with max-age: 0 will remove a preloaded
  // site from the list
  var uri = Services.io.newURI("http://keyerror.com", null, null);
  gSTSService.processStsHeader(uri, "max-age=0", 0);
  do_check_false(gSTSService.isStsHost("keyerror.com", 0));
  do_check_false(gSTSService.isStsHost("subdomain.keyerror.com", 0));
  // check that processing another header (with max-age non-zero) will
  // re-enable a site's sts status
  gSTSService.processStsHeader(uri, "max-age=1000", 0);
  do_check_true(gSTSService.isStsHost("keyerror.com", 0));
  // but this time include subdomains was not set, so test for that
  do_check_false(gSTSService.isStsHost("subdomain.keyerror.com", 0));

  // check that processing a header with max-age: 0 from a subdomain of a site
  // will not remove that (ancestor) site from the list
  var uri = Services.io.newURI("http://subdomain.intercom.io", null, null);
  gSTSService.processStsHeader(uri, "max-age=0", 0);
  do_check_true(gSTSService.isStsHost("intercom.io", 0));
  do_check_false(gSTSService.isStsHost("subdomain.intercom.io", 0));

  var uri = Services.io.newURI("http://subdomain.pixi.me", null, null);
  gSTSService.processStsHeader(uri, "max-age=0", 0);
  // we received a header with "max-age=0", so we have "no information"
  // regarding the sts state of subdomain.pixi.me specifically, but
  // it is actually still an STS host, because of the preloaded pixi.me
  // including subdomains.
  // Here's a drawing:
  // |-- pixi.me (in preload list, includes subdomains)      IS sts host
  //     |-- subdomain.pixi.me                               IS sts host
  //     |   `-- another.subdomain.pixi.me                   IS sts host
  //     `-- sibling.pixi.me                                 IS sts host
  do_check_true(gSTSService.isStsHost("subdomain.pixi.me", 0));
  do_check_true(gSTSService.isStsHost("sibling.pixi.me", 0));
  do_check_true(gSTSService.isStsHost("another.subdomain.pixi.me", 0));

  gSTSService.processStsHeader(uri, "max-age=1000", 0);
  // Here's what we have now:
  // |-- pixi.me (in preload list, includes subdomains)      IS sts host
  //     |-- subdomain.pixi.me (include subdomains is false) IS sts host
  //     |   `-- another.subdomain.pixi.me                   IS NOT sts host
  //     `-- sibling.pixi.me                                 IS sts host
  do_check_true(gSTSService.isStsHost("subdomain.pixi.me", 0));
  do_check_true(gSTSService.isStsHost("sibling.pixi.me", 0));
  do_check_false(gSTSService.isStsHost("another.subdomain.pixi.me", 0));

  // Simulate leaving private browsing mode
  Services.obs.notifyObservers(null, "last-pb-context-exited", null);
}

const IS_PRIVATE = Ci.nsISocketProvider.NO_PERMANENT_STORAGE;

function test_private_browsing1() {
  // sanity - crypto.cat is preloaded, includeSubdomains set
  do_check_true(gSTSService.isStsHost("crypto.cat", IS_PRIVATE));
  do_check_true(gSTSService.isStsHost("a.b.c.subdomain.crypto.cat", IS_PRIVATE));

  var uri = Services.io.newURI("http://crypto.cat", null, null);
  gSTSService.processStsHeader(uri, "max-age=0", IS_PRIVATE);
  do_check_false(gSTSService.isStsHost("crypto.cat", IS_PRIVATE));
  do_check_false(gSTSService.isStsHost("a.b.subdomain.crypto.cat", IS_PRIVATE));

  // check adding it back in
  gSTSService.processStsHeader(uri, "max-age=1000", IS_PRIVATE);
  do_check_true(gSTSService.isStsHost("crypto.cat", IS_PRIVATE));
  // but no includeSubdomains this time
  do_check_false(gSTSService.isStsHost("b.subdomain.crypto.cat", IS_PRIVATE));

  // do the hokey-pokey...
  gSTSService.processStsHeader(uri, "max-age=0", IS_PRIVATE);
  do_check_false(gSTSService.isStsHost("crypto.cat", IS_PRIVATE));
  do_check_false(gSTSService.isStsHost("subdomain.crypto.cat", IS_PRIVATE));

  // TODO unfortunately we don't have a good way to know when an entry
  // has expired in the permission manager, so we can't yet extend this test
  // to that case.
  // Test that an expired private browsing entry results in correctly
  // identifying a host that is on the preload list as no longer sts.
  // (This happens when we're in private browsing mode, we get a header from
  // a site on the preload list, and that header later expires. We need to
  // then treat that host as no longer an sts host.)
  // (sanity check first - this should be in the preload list)
  do_check_true(gSTSService.isStsHost("logentries.com", IS_PRIVATE));
  var uri = Services.io.newURI("http://logentries.com", null, null);
  // according to the rfc, max-age can't be negative, but this is a great
  // way to test an expired entry
  gSTSService.processStsHeader(uri, "max-age=-1000", IS_PRIVATE);
  do_check_false(gSTSService.isStsHost("logentries.com", IS_PRIVATE));

  // Simulate leaving private browsing mode
  Services.obs.notifyObservers(null, "last-pb-context-exited", null);
}

function test_private_browsing2() {
  // if this test gets this far, it means there's a private browsing service
  do_check_true(gSTSService.isStsHost("crypto.cat", 0));
  // the crypto.cat entry has includeSubdomains set
  do_check_true(gSTSService.isStsHost("subdomain.crypto.cat", 0));

  // Now that we're out of private browsing mode, we need to make sure
  // we've "forgotten" that we "forgot" this site's sts status.
  do_check_true(gSTSService.isStsHost("logentries.com", 0));

  run_next_test();
}
