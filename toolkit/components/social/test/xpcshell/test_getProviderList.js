/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  let manifests = [0, 1, 2].map(function (i) {
    return {
      name: "provider " + i,
      workerURL: "http://example" + i + ".com/worker.js",
      origin: "http://example" + i + ".com"
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
  let providers = yield SocialService.getProviderList(next);
  do_check_true(providers.length >= 3);
  for (let i = 0; i < manifests.length; i++) {
    do_check_neq(providers.map(function (p) p.origin).indexOf(manifests[i].origin), -1);
    do_check_neq(providers.map(function (p) p.workerURL).indexOf(manifests[i].workerURL), -1);
    do_check_neq(providers.map(function (p) p.name).indexOf(manifests[i].name), -1);
  }
}
