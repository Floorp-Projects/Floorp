/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/Services.jsm");

function run_test() {
  let manifests = [
    { // normal provider
      name: "provider 1",
      origin: "https://example1.com",
      workerURL: "https://example1.com/worker.js"
    },
    { // provider without workerURL
      name: "provider 2",
      origin: "https://example2.com"
    }
  ];

  manifests.forEach(function (manifest) {
    MANIFEST_PREFS.setCharPref(manifest.origin, JSON.stringify(manifest));
  });

  // Enable the service for this test
  Services.prefs.setBoolPref("social.enabled", true);
  Cu.import("resource://gre/modules/SocialService.jsm");

  let runner = new AsyncRunner();
  let next = runner.next.bind(runner);
  runner.appendIterator(testGetProvider(manifests, next));
  runner.appendIterator(testGetProviderList(manifests, next));
  runner.appendIterator(testEnabled(manifests, next));
  runner.appendIterator(testAddRemoveProvider(manifests, next));
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
    do_check_true(provider.enabled);
    do_check_eq(provider.workerURL, manifests[i].workerURL);
    do_check_eq(provider.name, manifests[i].name);
  }
}

function testEnabled(manifests, next) {
  let providers = yield SocialService.getProviderList(next);
  do_check_true(providers.length >= manifests.length);
  do_check_true(SocialService.enabled);
  providers.forEach(function (provider) {
    do_check_true(provider.enabled);
  });

  let notificationDisabledCorrect = false;
  Services.obs.addObserver(function obs1(subj, topic, data) {
    Services.obs.removeObserver(obs1, "social:pref-changed");
    notificationDisabledCorrect = data == "disabled";
  }, "social:pref-changed", false);

  SocialService.enabled = false;
  do_check_true(notificationDisabledCorrect);
  do_check_true(!Services.prefs.getBoolPref("social.enabled"));
  do_check_true(!SocialService.enabled);
  providers.forEach(function (provider) {
    do_check_true(!provider.enabled);
  });

  // Check that setting the pref directly updates things accordingly
  let notificationEnabledCorrect = false;
  Services.obs.addObserver(function obs2(subj, topic, data) {
    Services.obs.removeObserver(obs2, "social:pref-changed");
    notificationEnabledCorrect = data == "enabled";
  }, "social:pref-changed", false);

  Services.prefs.setBoolPref("social.enabled", true);

  do_check_true(notificationEnabledCorrect);
  do_check_true(SocialService.enabled);
  providers.forEach(function (provider) {
    do_check_true(provider.enabled);
  });
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
