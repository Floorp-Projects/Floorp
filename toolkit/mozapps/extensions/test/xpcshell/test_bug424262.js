/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
Components.utils.import("resource://gre/modules/AddonRepository.jsm");

const PREF_GETADDONS_GETRECOMMENDED = "extensions.getAddons.recommended.url";

Components.utils.import("resource://testing-common/httpd.js");
var server;
var RESULTS = [
  null,
  null,
  0,
  2,
  4,
  5,
  5,
  5
];

var RecommendedCallback = {
  searchSucceeded: function(addons, length, total) {
    dump("loaded");
    // Search is complete
    do_check_eq(length, RESULTS.length);

    for (var i = 0; i < length; i++) {
      if (addons[i].averageRating != RESULTS[i])
        do_throw("Rating for " + addons[i].id + " was " + addons[i].averageRating + ", should have been " + RESULTS[i]);
    }
    server.stop(do_test_finished);
  },

  searchFailed: function() {
    server.stop(do_test_finished);
    do_throw("Recommended results failed");
  }
};

function run_test()
{
  // EM needs to be running.
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");
  startupManager();

  server = new HttpServer();
  server.start(-1);
  gPort = server.identity.primaryPort;
  mapFile("/data/test_bug424262.xml", server);

  // Point the addons repository to the test server
  Services.prefs.setCharPref(PREF_GETADDONS_GETRECOMMENDED, "http://localhost:" +
                             gPort + "/data/test_bug424262.xml");

  do_check_neq(AddonRepository, null);

  do_test_pending();
  // Pull some results.
  AddonRepository.retrieveRecommendedAddons(RESULTS.length, RecommendedCallback);
}

