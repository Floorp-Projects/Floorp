/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/Services.jsm");


function run_test() {
  // Test must run at startup for migration to occur, so we can only test
  // one migration per test file
  initApp();

  // NOTE: none of the manifests here can have a workerURL set, or we attempt
  // to create a FrameWorker and that fails under xpcshell...
  let manifest = { // normal provider
    name: "provider 1",
    origin: "https://example1.com",
  };

  MANIFEST_PREFS.setCharPref(manifest.origin, JSON.stringify(manifest));

  // Set both providers active and flag the first one as "current"
  let activeVal = Cc["@mozilla.org/supports-string;1"].
             createInstance(Ci.nsISupportsString);
  let active = {};
  active[manifest.origin] = 1;
  activeVal.data = JSON.stringify(active);
  Services.prefs.setComplexValue("social.activeProviders",
                                 Ci.nsISupportsString, activeVal);

  // social.enabled pref is the key focus of this test. We set the user pref,
  // and then migration should a) remove the provider from activeProviders and
  // b) unset social.enabled
  Services.prefs.setBoolPref("social.enabled", false);

  Cu.import("resource://gre/modules/SocialService.jsm");

  let runner = new AsyncRunner();
  let next = runner.next.bind(runner);
  runner.appendIterator(testMigration(manifest, next));
  runner.next();
}

function* testMigration(manifest, next) {
  // look at social.activeProviders, we should have migrated into that, and
  // we should be set as a user level pref after migration
  do_check_true(Services.prefs.prefHasUserValue("social.enabled"));
  do_check_true(MANIFEST_PREFS.prefHasUserValue(manifest.origin));
  // we need to access the providers for everything to initialize
  yield SocialService.getProviderList(next);
  do_check_false(SocialService.enabled);
  do_check_false(Services.prefs.prefHasUserValue("social.enabled"));
  do_check_true(Services.prefs.prefHasUserValue("social.activeProviders"));

  let activeProviders;
  let pref = Services.prefs.getComplexValue("social.activeProviders",
                                            Ci.nsISupportsString).data;
  activeProviders = JSON.parse(pref);
  do_check_true(activeProviders[manifest.origin] == undefined);
  do_check_true(MANIFEST_PREFS.prefHasUserValue(manifest.origin));
}
