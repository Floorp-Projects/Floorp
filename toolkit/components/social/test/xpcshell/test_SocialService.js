/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm");

function run_test() {
  initApp();

  // NOTE: none of the manifests here can have a workerURL set, or we attempt
  // to create a FrameWorker and that fails under xpcshell...
  let manifests = [
    { // normal provider
      name: "provider 1",
      origin: "https://example1.com",
      shareURL: "https://example1.com/share/",
    },
    { // provider without workerURL
      name: "provider 2",
      origin: "https://example2.com",
      shareURL: "https://example2.com/share/",
    }
  ];

  Cu.import("resource://gre/modules/SocialService.jsm");
  Cu.import("resource://gre/modules/MozSocialAPI.jsm");

  let runner = new AsyncRunner();
  let next = runner.next.bind(runner);
  runner.appendIterator(testAddProviders(manifests, next));
  runner.appendIterator(testGetProvider(manifests, next));
  runner.appendIterator(testGetProviderList(manifests, next));
  runner.appendIterator(testAddRemoveProvider(manifests, next));
  runner.appendIterator(testIsSameOrigin(manifests, next));
  runner.appendIterator(testResolveUri  (manifests, next));
  runner.appendIterator(testOrderedProviders(manifests, next));
  runner.appendIterator(testRemoveProviders(manifests, next));
  runner.next();
}

function* testAddProviders(manifests, next) {
  do_check_false(SocialService.enabled);
  let provider = yield SocialService.addProvider(manifests[0], next);
  do_check_true(SocialService.enabled);
  do_check_true(MozSocialAPI._enabled);
  do_check_false(provider.enabled);
  provider = yield SocialService.addProvider(manifests[1], next);
  do_check_false(provider.enabled);
}

function* testRemoveProviders(manifests, next) {
  do_check_true(SocialService.enabled);
  yield SocialService.disableProvider(manifests[0].origin, next);
  yield SocialService.disableProvider(manifests[1].origin, next);
  do_check_false(SocialService.enabled);
}

function* testGetProvider(manifests, next) {
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

function* testGetProviderList(manifests, next) {
  let providers = yield SocialService.getProviderList(next);
  do_check_true(providers.length >= manifests.length);
  for (let i = 0; i < manifests.length; i++) {
    let providerIdx = providers.map(p => p.origin).indexOf(manifests[i].origin);
    let provider = providers[providerIdx];
    do_check_true(!!provider);
    do_check_false(provider.enabled);
    do_check_eq(provider.workerURL, manifests[i].workerURL);
    do_check_eq(provider.name, manifests[i].name);
  }
}

function* testAddRemoveProvider(manifests, next) {
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
  yield SocialService.disableProvider(newProvider.origin, next);
  providersAfter = yield SocialService.getProviderList(next);
  do_check_eq(providersAfter.length, originalProviders.length);
  do_check_eq(providersAfter.indexOf(newProvider), -1);
  newProvider = yield SocialService.getProvider(newProvider.origin, next);
  do_check_true(!newProvider);
}

function* testIsSameOrigin(manifests, next) {
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

function* testResolveUri(manifests, next) {
  let providers = yield SocialService.getProviderList(next);
  let provider = providers[0];
  do_check_eq(provider.resolveUri(provider.origin).spec, provider.origin + "/");
  do_check_eq(provider.resolveUri("foo.html").spec, provider.origin + "/foo.html");
  do_check_eq(provider.resolveUri("/foo.html").spec, provider.origin + "/foo.html");
  do_check_eq(provider.resolveUri("http://somewhereelse.com/foo.html").spec, "http://somewhereelse.com/foo.html");
  do_check_eq(provider.resolveUri("data:text/html,<p>hi").spec, "data:text/html,<p>hi");
}

function* testOrderedProviders(manifests, next) {
  let providers = yield SocialService.getProviderList(next);

  // add visits for only one of the providers
  let visits = [];
  let startDate = Date.now() * 1000;
  for (let i = 0; i < 10; i++) {
    visits.push({
      uri: Services.io.newURI(providers[1].shareURL + i, null, null),
      visitDate: startDate + i
    });
  }

  PlacesTestUtils.addVisits(visits).then(next);
  yield;
  let orderedProviders = yield SocialService.getOrderedProviderList(next);
  do_check_eq(orderedProviders[0], providers[1]);
  do_check_eq(orderedProviders[1], providers[0]);
  do_check_true(orderedProviders[0].frecency > orderedProviders[1].frecency);
  PlacesTestUtils.clearHistory().then(next);
  yield;
}
