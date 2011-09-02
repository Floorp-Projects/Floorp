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
 * The Original Code is Content Preference Service Tests.
 *
 * The Initial Developer of the Original Code is
 * Arno Renevier.
 * Portions created by the Initial Developer are Copyright (C) 2011
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
 * ***** END LICENSE BLOCK ***** */


var prefObserver = {
    setCalledNum: 0,
    onContentPrefSet: function(aGroup, aName, aValue) {
        this.setCalledNum++;
    },
    removedCalledNum: 0,
    onContentPrefRemoved: function(aGroup, aName) {
        this.removedCalledNum++;
    }
};

function run_test() {
  var pbs;
  try {
    pbs = Cc["@mozilla.org/privatebrowsing;1"].getService(Ci.nsIPrivateBrowsingService);
  } catch(e) {
    // Private Browsing might not be available
    return;
  }

  Cc["@mozilla.org/preferences-service;1"].
    getService(Ci.nsIPrefBranch).
      setBoolPref("browser.privatebrowsing.keep_current_session", true);

  var cps = Cc["@mozilla.org/content-pref/service;1"].getService(Ci.nsIContentPrefService);
  cps.removeGroupedPrefs();

  var uri = ContentPrefTest.getURI("http://www.example.com/");
  var group = cps.grouper.group(uri);

  // first, set a pref in normal mode
  cps.setPref(uri, "value", "foo");
  cps.setPref(null, "value-global", "foo-global");

  var num;
  cps.addObserver("value", prefObserver);
  cps.addObserver("value-global", prefObserver);

  pbs.privateBrowsingEnabled = true;

  // test setPref
  num = prefObserver.setCalledNum;
  cps.setPref(uri, "value", "foo-private-browsing");
  do_check_eq(cps.hasPref(uri, "value"), true);
  do_check_eq(cps.getPref(uri, "value"), "foo-private-browsing");
  do_check_eq(prefObserver.setCalledNum, num + 1);

  num = prefObserver.setCalledNum;
  cps.setPref(null, "value-global", "foo-private-browsing-global");
  do_check_eq(cps.hasPref(null, "value-global"), true);
  do_check_eq(cps.getPref(null, "value-global"), "foo-private-browsing-global");
  do_check_eq(prefObserver.setCalledNum, num + 1);

  // test removePref
  num = prefObserver.removedCalledNum;
  cps.removePref(uri, "value");
  do_check_eq(cps.hasPref(uri, "value"), true);
  // fallback to non private mode value
  do_check_eq(cps.getPref(uri, "value"), "foo"); 
  do_check_eq(prefObserver.removedCalledNum, num + 1);

  num = prefObserver.removedCalledNum;
  cps.removePref(null, "value-global");
  do_check_eq(cps.hasPref(null, "value-global"), true);
  // fallback to non private mode value
  do_check_eq(cps.getPref(null, "value-global"), "foo-global") ;
  do_check_eq(prefObserver.removedCalledNum, num + 1);

  // test removeGroupedPrefs
  cps.setPref(uri, "value", "foo-private-browsing");
  cps.removeGroupedPrefs();
  do_check_eq(cps.hasPref(uri, "value"), false);
  do_check_eq(cps.getPref(uri, "value"), undefined);

  cps.setPref(null, "value-global", "foo-private-browsing-global");
  cps.removeGroupedPrefs();
  do_check_eq(cps.hasPref(null, "value-global"), true);
  do_check_eq(cps.getPref(null, "value-global"), "foo-private-browsing-global");

  // test removePrefsByName
  num = prefObserver.removedCalledNum;
  cps.setPref(uri, "value", "foo-private-browsing");
  cps.removePrefsByName("value");
  do_check_eq(cps.hasPref(uri, "value"), false);
  do_check_eq(cps.getPref(uri, "value"), undefined);
  do_check_true(prefObserver.removedCalledNum > num);

  num = prefObserver.removedCalledNum;
  cps.setPref(null, "value-global", "foo-private-browsing");
  cps.removePrefsByName("value-global");
  do_check_eq(cps.hasPref(null, "value-global"), false);
  do_check_eq(cps.getPref(null, "value-global"), undefined);
  do_check_true(prefObserver.removedCalledNum > num);

  // test getPrefs
  cps.setPref(uri, "value", "foo-private-browsing");
  do_check_eq(cps.getPrefs(uri).getProperty("value"), "foo-private-browsing");

  cps.setPref(null, "value-global", "foo-private-browsing-global");
  do_check_eq(cps.getPrefs(null).getProperty("value-global"), "foo-private-browsing-global");

  // test getPrefsByName
  do_check_eq(cps.getPrefsByName("value").getProperty(group), "foo-private-browsing");
  do_check_eq(cps.getPrefsByName("value-global").getProperty(null), "foo-private-browsing-global");

  cps.removeObserver("value", prefObserver);
  cps.removeObserver("value-global", prefObserver);
}
