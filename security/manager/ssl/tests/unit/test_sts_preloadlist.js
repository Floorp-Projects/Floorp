var Cc = Components.classes;
var Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/Services.jsm");

var _pbsvc;

// lazy getter, added for bug 785860 to avoid testing private browsing when the
// private browsing service doesn't exist
function getPBSvc() {
  if (_pbsvc)
    return _pbsvc;

  try {
    _pbsvc = Cc["@mozilla.org/privatebrowsing;1"]
               .getService(Ci.nsIPrivateBrowsingService);
    return _pbsvc;
  } catch (e) {}

  return null;
}

var gSTSService = Cc["@mozilla.org/stsservice;1"]
                  .getService(Ci.nsIStrictTransportSecurityService);

function Observer() {}
Observer.prototype = {
  observe: function(subject, topic, data) {
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
  Services.obs.removeObserver(gObserver, "private-browsing-transition-complete");
  if (getPBSvc())
    getPBSvc().privateBrowsingEnabled = false;

  for (var host of hosts) {
    var uri = Services.io.newURI(host, null, null);
    gSTSService.removeStsState(uri);
  }
}

function run_test() {
  do_register_cleanup(cleanup);
  Services.obs.addObserver(gObserver, "private-browsing-transition-complete", false);
  Services.prefs.setBoolPref("browser.privatebrowsing.keep_current_session", true);

  add_test(test_part1);
  if (getPBSvc()) {
    add_test(test_private_browsing1);
    add_test(test_private_browsing2);
  }

  run_next_test();
}

function test_part1() {
  // check that a host not in the list is not identified as an sts host
  do_check_false(gSTSService.isStsHost("nonexistent.mozilla.com"));

  // check that an ancestor domain is not identified as an sts host
  do_check_false(gSTSService.isStsHost("com"));

  // Note: the following were taken from the STS preload list
  // as of Sept. 2012. If the list changes, this test will need to be modified.
  // check that the pref to toggle using the preload list works
  Services.prefs.setBoolPref("network.stricttransportsecurity.preloadlist", false);
  do_check_false(gSTSService.isStsHost("factor.cc"));
  Services.prefs.setBoolPref("network.stricttransportsecurity.preloadlist", true);
  do_check_true(gSTSService.isStsHost("factor.cc"));

  // check that an entry at the beginning of the list is an sts host
  do_check_true(gSTSService.isStsHost("arivo.com.br"));

  // check that a subdomain is an sts host (includeSubdomains is set)
  do_check_true(gSTSService.isStsHost("subdomain.arivo.com.br"));

  // check that another subdomain is an sts host (includeSubdomains is set)
  do_check_true(gSTSService.isStsHost("a.b.c.subdomain.arivo.com.br"));

  // check that an entry in the middle of the list is an sts host
  do_check_true(gSTSService.isStsHost("neg9.org"));

  // check that a subdomain is not an sts host (includeSubdomains is not set)
  do_check_false(gSTSService.isStsHost("subdomain.neg9.org"));

  // check that an entry at the end of the list is an sts host
  do_check_true(gSTSService.isStsHost("www.noisebridge.net"));

  // check that a subdomain is not an sts host (includeSubdomains is not set)
  do_check_false(gSTSService.isStsHost("a.subdomain.www.noisebridge.net"));

  // check that a host with a dot on the end won't break anything
  do_check_false(gSTSService.isStsHost("notsts.nonexistent.mozilla.com."));

  // check that processing a header with max-age: 0 will remove a preloaded
  // site from the list
  var uri = Services.io.newURI("http://keyerror.com", null, null);
  gSTSService.processStsHeader(uri, "max-age=0");
  do_check_false(gSTSService.isStsHost("keyerror.com"));
  do_check_false(gSTSService.isStsHost("subdomain.keyerror.com"));
  // check that processing another header (with max-age non-zero) will
  // re-enable a site's sts status
  gSTSService.processStsHeader(uri, "max-age=1000");
  do_check_true(gSTSService.isStsHost("keyerror.com"));
  // but this time include subdomains was not set, so test for that
  do_check_false(gSTSService.isStsHost("subdomain.keyerror.com"));

  // check that processing a header with max-age: 0 from a subdomain of a site
  // will not remove that (ancestor) site from the list
  var uri = Services.io.newURI("http://subdomain.intercom.io", null, null);
  gSTSService.processStsHeader(uri, "max-age=0");
  do_check_true(gSTSService.isStsHost("intercom.io"));
  do_check_false(gSTSService.isStsHost("subdomain.intercom.io"));

  var uri = Services.io.newURI("http://subdomain.pixi.me", null, null);
  gSTSService.processStsHeader(uri, "max-age=0");
  // we received a header with "max-age=0", so we have "no information"
  // regarding the sts state of subdomain.pixi.me specifically, but
  // it is actually still an STS host, because of the preloaded pixi.me
  // including subdomains.
  // Here's a drawing:
  // |-- pixi.me (in preload list, includes subdomains)      IS sts host
  //     |-- subdomain.pixi.me                               IS sts host
  //     |   `-- another.subdomain.pixi.me                   IS sts host
  //     `-- sibling.pixi.me                                 IS sts host
  do_check_true(gSTSService.isStsHost("subdomain.pixi.me"));
  do_check_true(gSTSService.isStsHost("sibling.pixi.me"));
  do_check_true(gSTSService.isStsHost("another.subdomain.pixi.me"));

  gSTSService.processStsHeader(uri, "max-age=1000");
  // Here's what we have now:
  // |-- pixi.me (in preload list, includes subdomains)      IS sts host
  //     |-- subdomain.pixi.me (include subdomains is false) IS sts host
  //     |   `-- another.subdomain.pixi.me                   IS NOT sts host
  //     `-- sibling.pixi.me                                 IS sts host
  do_check_true(gSTSService.isStsHost("subdomain.pixi.me"));
  do_check_true(gSTSService.isStsHost("sibling.pixi.me"));
  do_check_false(gSTSService.isStsHost("another.subdomain.pixi.me"));

  // Test private browsing correctly interacts with removing preloaded sites.
  // If we don't have the private browsing service, don't run those tests
  // (which means we have to manually call run_next_test() instead of relying
  // on our observer to call it).
  if (getPBSvc()) {
    getPBSvc().privateBrowsingEnabled = true;
  } else {
    run_next_test();
  }
}

function test_private_browsing1() {
  // sanity - crypto.cat is preloaded, includeSubdomains set
  do_check_true(gSTSService.isStsHost("crypto.cat"));
  do_check_true(gSTSService.isStsHost("a.b.c.subdomain.crypto.cat"));

  var uri = Services.io.newURI("http://crypto.cat", null, null);
  gSTSService.processStsHeader(uri, "max-age=0");
  do_check_false(gSTSService.isStsHost("crypto.cat"));
  do_check_false(gSTSService.isStsHost("a.b.subdomain.crypto.cat"));

  // check adding it back in
  gSTSService.processStsHeader(uri, "max-age=1000");
  do_check_true(gSTSService.isStsHost("crypto.cat"));
  // but no includeSubdomains this time
  do_check_false(gSTSService.isStsHost("b.subdomain.crypto.cat"));

  // do the hokey-pokey...
  gSTSService.processStsHeader(uri, "max-age=0");
  do_check_false(gSTSService.isStsHost("crypto.cat"));
  do_check_false(gSTSService.isStsHost("subdomain.crypto.cat"));

  // TODO unfortunately we don't have a good way to know when an entry
  // has expired in the permission manager, so we can't yet extend this test
  // to that case.
  // Test that an expired private browsing entry results in correctly
  // identifying a host that is on the preload list as no longer sts.
  // (This happens when we're in private browsing mode, we get a header from
  // a site on the preload list, and that header later expires. We need to
  // then treat that host as no longer an sts host.)
  // (sanity check first - this should be in the preload list)
  do_check_true(gSTSService.isStsHost("logentries.com"));
  var uri = Services.io.newURI("http://logentries.com", null, null);
  // according to the rfc, max-age can't be negative, but this is a great
  // way to test an expired entry
  gSTSService.processStsHeader(uri, "max-age=-1000");
  do_check_false(gSTSService.isStsHost("logentries.com"));

  // if this test gets this far, it means there's a private browsing service
  getPBSvc().privateBrowsingEnabled = false;
}

function test_private_browsing2() {
  // if this test gets this far, it means there's a private browsing service
  do_check_true(gSTSService.isStsHost("crypto.cat"));
  // the crypto.cat entry has includeSubdomains set
  do_check_true(gSTSService.isStsHost("subdomain.crypto.cat"));

  // Now that we're out of private browsing mode, we need to make sure
  // we've "forgotten" that we "forgot" this site's sts status.
  do_check_true(gSTSService.isStsHost("logentries.com"));

  run_next_test();
}
