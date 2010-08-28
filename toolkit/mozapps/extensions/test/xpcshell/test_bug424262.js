/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *      Dave Townsend <dtownsend@oxymoronical.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 */
Components.utils.import("resource://gre/modules/AddonRepository.jsm");

const PREF_GETADDONS_GETRECOMMENDED      = "extensions.getAddons.recommended.url";

do_load_httpd_js();
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

  server = new nsHttpServer();
  server.registerDirectory("/", do_get_file("data"));
  server.start(4444);

  // Point the addons repository to the test server
  Services.prefs.setCharPref(PREF_GETADDONS_GETRECOMMENDED, "http://localhost:4444/test_bug424262.xml");
  
  do_check_neq(AddonRepository, null);

  do_test_pending();
  // Pull some results.
  AddonRepository.retrieveRecommendedAddons(RESULTS.length, RecommendedCallback);
}

