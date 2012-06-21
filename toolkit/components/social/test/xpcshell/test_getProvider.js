/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  let manifests = [0, 1, 2].map(function (i) {
    return {
      origin: "http://example" + i + ".com",
      name: "provider " + i,
    };
  });
  manifests.forEach(function (manifest) {
    MANIFEST_PREFS.setCharPref(manifest.origin, JSON.stringify(manifest));
  });
  do_register_cleanup(function () MANIFEST_PREFS.deleteBranch(""));

  Cu.import("resource://gre/modules/SocialService.jsm");

  let runner = new AsyncRunner();
  runner.appendIterator(test(manifests, runner.next.bind(runner)));
  runner.next();
}

function test(manifests, next) {
  for (let i = 0; i < manifests.length; i++) {
    let manifest = manifests[i];
    let provider = yield SocialService.getProvider(manifest.origin, next);
    do_check_neq(provider, null);
    do_check_eq(provider.origin, manifest.origin);
    do_check_eq(provider.name, manifest.name);
  }
  do_check_eq((yield SocialService.getProvider("bogus", next)), null);
}
