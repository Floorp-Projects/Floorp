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
 * Portions created by the Initial Developer are Copyright (C) 2007
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
 * the terms of any one of the MPL, the GPL or the LGPL
 *
 * ***** END LICENSE BLOCK *****
 */

const PREF_MATCH_OS_LOCALE = "intl.locale.matchOS";
const PREF_SELECTED_LOCALE = "general.useragent.locale";

// Disables security checking our updates which haven't been signed
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

do_load_httpd_js();

// This is the data we expect to see sent as part of the update url.
var EXPECTED = [
  {
    id: "bug335238_1@tests.mozilla.org",
    version: "1.3.4",
    maxAppVersion: "5",
    status: "userEnabled",
    appId: "xpcshell@tests.mozilla.org",
    appVersion: "1",
    appOs: "XPCShell",
    appAbi: "noarch-spidermonkey",
    locale: "en-US",
    reqVersion: "2"
  },
  {
    id: "bug335238_2@tests.mozilla.org",
    version: "28at",
    maxAppVersion: "7",
    status: "userDisabled",
    appId: "xpcshell@tests.mozilla.org",
    appVersion: "1",
    appOs: "XPCShell",
    appAbi: "noarch-spidermonkey",
    locale: "en-US",
    reqVersion: "2"
  }
];

var ADDONS = [
  {id: "bug335238_1@tests.mozilla.org",
   addon: "test_bug335238_1"},
  {id: "bug335238_2@tests.mozilla.org",
   addon: "test_bug335238_2"}
];

var server;

var updateListener = {
  pendingCount: 0,

  onUpdateAvailable: function(aAddon) {
    do_throw("Should not have seen an update for " + aAddon.id);
  },

  onUpdateFinished: function() {
    if (--this.pendingCount == 0)
      server.stop(do_test_finished);
  }
}

var requestHandler = {
  handle: function(metadata, response)
  {
    var expected = EXPECTED[metadata.path.substring(1)];
    var params = metadata.queryString.split("&");
    do_check_eq(params.length, 10);
    for (var k in params) {
      var pair = params[k].split("=");
      var name = decodeURIComponent(pair[0]);
      var value = decodeURIComponent(pair[1]);
      do_check_eq(expected[name], value);
    }
    response.setStatusLine(metadata.httpVersion, 404, "Not Found");
  }
}

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  server = new nsHttpServer();
  server.registerPathHandler("/0", requestHandler);
  server.registerPathHandler("/1", requestHandler);
  server.start(4444);

  Services.prefs.setBoolPref(PREF_MATCH_OS_LOCALE, false);
  Services.prefs.setCharPref(PREF_SELECTED_LOCALE, "en-US");

  startupManager();
  installAllFiles([do_get_addon(a.addon) for each (a in ADDONS)], function() {

    restartManager();
    AddonManager.getAddon(ADDONS[1].id, function(addon) {
      addon.userDisabled = true;
      restartManager();

      AddonManager.getAddons([a.id for each (a in ADDONS)], function(installedItems) {
        installedItems.forEach(function(item) {
          updateListener.pendingCount++;
          item.findUpdates(updateListener, AddonManager.UPDATE_WHEN_USER_REQUESTED);
        });
      });
    });
  });
}
