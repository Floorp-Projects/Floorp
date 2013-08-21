/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");

function run_test() {
  initApp();

  // NOTE: none of the manifests here can have a workerURL set, or we attempt
  // to create a FrameWorker and that fails under xpcshell...
  let manifests = [
    { // normal provider
      name: "provider 1",
      origin: "https://example1.com",
      sidebarURL: "https://example1.com/sidebar/",
    },
    { // provider without workerURL
      name: "provider 2",
      origin: "https://example2.com",
      sidebarURL: "https://example2.com/sidebar/",
    }
  ];

  manifests.forEach(function (manifest) {
    MANIFEST_PREFS.setCharPref(manifest.origin, JSON.stringify(manifest));
  });
  // Set both providers active and flag the first one as "current"
  let activeVal = Cc["@mozilla.org/supports-string;1"].
             createInstance(Ci.nsISupportsString);
  let active = {};
  for (let m of manifests)
    active[m.origin] = 1;
  activeVal.data = JSON.stringify(active);
  Services.prefs.setComplexValue("social.activeProviders",
                                 Ci.nsISupportsString, activeVal);
  Services.prefs.setCharPref("social.provider.current", manifests[0].origin);

  // Enable the service for this test
  Services.prefs.setBoolPref("social.enabled", true);
  Cu.import("resource://gre/modules/SocialService.jsm");

  let runner = new AsyncRunner();
  let next = runner.next.bind(runner);
  runner.appendIterator(testGetProvider(manifests, next));
  runner.appendIterator(testGetProviderList(manifests, next));
  runner.appendIterator(testEnabled(manifests, next));
  runner.appendIterator(testAddRemoveProvider(manifests, next));
  runner.appendIterator(testIsSameOrigin(manifests, next));
  runner.appendIterator(testResolveUri  (manifests, next));
  runner.appendIterator(testOrderedProviders(manifests, next));
  runner.next();
}

function testGetProvider(manifests, next) {
  for (let i = 0; i < manifests.length; i++) {
    let manifest = manifests[i];
    let provider = yield SocialService.getProvider(manifest.origin, next);
    do_check_neq(provider, null);
    do_check_eq(provider.name, manifest.name);
    do_check_eq(provider.workerURL, manifest.workerURL);
    do_check_eq(provider.origin, manifest.origin);
  }
  do_check_eq((yield SocialService.getProvider("bogus", next)), null);
}

function testGetProviderList(manifests, next) {
  let providers = yield SocialService.getProviderList(next);
  do_check_true(providers.length >= manifests.length);
  for (let i = 0; i < manifests.length; i++) {
    let providerIdx = providers.map(function (p) p.origin).indexOf(manifests[i].origin);
    let provider = providers[providerIdx];
    do_check_true(!!provider);
    do_check_false(provider.enabled);
    do_check_eq(provider.workerURL, manifests[i].workerURL);
    do_check_eq(provider.name, manifests[i].name);
  }
}


function testEnabled(manifests, next) {
  // Check that providers are disabled by default
  let providers = yield SocialService.getProviderList(next);
  do_check_true(providers.length >= manifests.length);
  do_check_true(SocialService.enabled, "social enabled at test start");
  providers.forEach(function (provider) {
    do_check_false(provider.enabled);
  });

  do_test_pending();
  function waitForEnableObserver(cb) {
    Services.prefs.addObserver("social.enabled", function prefObserver(subj, topic, data) {
      Services.prefs.removeObserver("social.enabled", prefObserver);
      cb();
    }, false);
  }

  // enable one of the added providers
  providers[providers.length-1].enabled = true;

  // now disable the service and check that it disabled that provider (and all others for good measure)
  waitForEnableObserver(function() {
    do_check_true(!SocialService.enabled);
    providers.forEach(function (provider) {
      do_check_false(provider.enabled);
    });
    waitForEnableObserver(function() {
      do_check_true(SocialService.enabled);
      // Enabling the service should not enable providers
      providers.forEach(function (provider) {
        do_check_false(provider.enabled);
      });
      do_test_finished();
    });
    SocialService.enabled = true;
  });
  SocialService.enabled = false;
}

function testAddRemoveProvider(manifests, next) {
  var threw;
  try {
    // Adding a provider whose origin already exists should fail
    SocialService.addProvider(manifests[0]);
  } catch (ex) {
    threw = ex;
  }
  do_check_neq(threw.toString().indexOf("SocialService.addProvider: provider with this origin already exists"), -1);

  let originalProviders = yield SocialService.getProviderList(next);

  // Check that provider installation succeeds
  let newProvider = yield SocialService.addProvider({
    name: "foo",
    origin: "http://example3.com"
  }, next);
  let retrievedNewProvider = yield SocialService.getProvider(newProvider.origin, next);
  do_check_eq(newProvider, retrievedNewProvider);

  let providersAfter = yield SocialService.getProviderList(next);
  do_check_eq(providersAfter.length, originalProviders.length + 1);
  do_check_neq(providersAfter.indexOf(newProvider), -1);

  // Now remove the provider
  yield SocialService.removeProvider(newProvider.origin, next);
  providersAfter = yield SocialService.getProviderList(next);
  do_check_eq(providersAfter.length, originalProviders.length);
  do_check_eq(providersAfter.indexOf(newProvider), -1);
  newProvider = yield SocialService.getProvider(newProvider.origin, next);
  do_check_true(!newProvider);
}

function testIsSameOrigin(manifests, next) {
  let providers = yield SocialService.getProviderList(next);
  let provider = providers[0];
  // provider.origin is a string.
  do_check_true(provider.isSameOrigin(provider.origin));
  do_check_true(provider.isSameOrigin(Services.io.newURI(provider.origin, null, null)));
  do_check_true(provider.isSameOrigin(provider.origin + "/some-sub-page"));
  do_check_true(provider.isSameOrigin(Services.io.newURI(provider.origin + "/some-sub-page", null, null)));
  do_check_false(provider.isSameOrigin("http://something.com"));
  do_check_false(provider.isSameOrigin(Services.io.newURI("http://something.com", null, null)));
  do_check_false(provider.isSameOrigin("data:text/html,<p>hi"));
  do_check_true(provider.isSameOrigin("data:text/html,<p>hi", true));
  do_check_false(provider.isSameOrigin(Services.io.newURI("data:text/html,<p>hi", null, null)));
  do_check_true(provider.isSameOrigin(Services.io.newURI("data:text/html,<p>hi", null, null), true));
  // we explicitly handle null and return false
  do_check_false(provider.isSameOrigin(null));
}

function testResolveUri(manifests, next) {
  let providers = yield SocialService.getProviderList(next);
  let provider = providers[0];
  do_check_eq(provider.resolveUri(provider.origin).spec, provider.origin + "/");
  do_check_eq(provider.resolveUri("foo.html").spec, provider.origin + "/foo.html");
  do_check_eq(provider.resolveUri("/foo.html").spec, provider.origin + "/foo.html");
  do_check_eq(provider.resolveUri("http://somewhereelse.com/foo.html").spec, "http://somewhereelse.com/foo.html");
  do_check_eq(provider.resolveUri("data:text/html,<p>hi").spec, "data:text/html,<p>hi");
}

function testOrderedProviders(manifests, next) {
  let providers = yield SocialService.getProviderList(next);

  // add visits for only one of the providers
  let visits = [];
  let startDate = Date.now() * 1000;
  for (let i = 0; i < 10; i++) {
    visits.push({
      uri: Services.io.newURI(providers[1].sidebarURL + i, null, null),
      visitDate: startDate + i
    });
  }

  promiseAddVisits(visits).then(next);
  yield;
  let orderedProviders = yield SocialService.getOrderedProviderList(next);
  do_check_eq(orderedProviders[0], providers[1]);
  do_check_eq(orderedProviders[1], providers[0]);
  do_check_true(orderedProviders[0].frecency > orderedProviders[1].frecency);
  promiseClearHistory().then(next);
  yield;
}
