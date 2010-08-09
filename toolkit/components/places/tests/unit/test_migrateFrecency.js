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
 * The Original Code is Places Test Code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Edward Lee <edilee@mozilla.com> (Original Author)
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
 * ***** END LICENSE BLOCK ***** */

/**
 * Make sure all places get their frecency calculated on migrate for bug 476300.
 */

function _(msg) {
  dump(".-* DEBUG *-. " + msg + "\n");
}

function run_test() {
  _("Copy the history file with plenty of data to migrate");
  let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
    getService(Ci.nsIProperties);
  let deleteMork = function() {
    let mork = dirSvc.get("ProfD", Ci.nsIFile);
    mork.append("history.dat");
    if (mork.exists())
      mork.remove(false);
  };

  deleteMork();
  let file = do_get_file("migrateFrecency.dat");
  file.copyTo(dirSvc.get("ProfD", Ci.nsIFile), "history.dat");

  _("Wait until places is done initializing to check migration");
  let places = null;
  const NS_PLACES_INIT_COMPLETE_TOPIC = "places-init-complete";
  let os = Cc["@mozilla.org/observer-service;1"].
    getService(Ci.nsIObserverService);
  let observer = {
    observe: function (subject, topic, data) {
      switch (topic) {
        case NS_PLACES_INIT_COMPLETE_TOPIC:
          _("Clean up after ourselves: remove observer and mork file");
          os.removeObserver(observer, NS_PLACES_INIT_COMPLETE_TOPIC);
          deleteMork();

          _("Now that places has migrated, check that it calculated frecencies");
          var stmt = places.DBConnection.createStatement(
              "SELECT COUNT(*) FROM moz_places WHERE frecency < 0");
          stmt.executeAsync({
              handleResult: function(results) {
                _("Should always get a result from COUNT(*)");
                let row = results.getNextRow();
                do_check_true(!!row);

                _("We should have no negative frecencies after migrate");
                do_check_eq(row.getResultByIndex(0), 0);
              },
              handleCompletion: do_test_finished,
              handleError: do_throw,
            });
          stmt.finalize();
          break;
      }
    },
  };
  os.addObserver(observer, NS_PLACES_INIT_COMPLETE_TOPIC, false);

  _("Start places to make it migrate");
  places = Cc["@mozilla.org/browser/nav-history-service;1"].
    getService(Ci.nsPIPlacesDatabase);

  do_test_pending();
}
