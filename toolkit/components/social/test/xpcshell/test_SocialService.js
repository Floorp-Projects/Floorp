/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  do_register_cleanup(function () MANIFEST_PREFS.deleteBranch(""));

  Cu.import("resource://gre/modules/SocialService.jsm");

  let runner = new AsyncRunner();
  let next = runner.next.bind(runner);
  runner.appendIterator(testGetProvider(manifests, next));
  runner.appendIterator(testGetProviderList(manifests, next));
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
    do_check_eq(provider.workerURL, manifests[i].workerURL);
    do_check_eq(provider.name, manifests[i].name);
  }
}
