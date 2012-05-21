/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test()
{
  let Cc = Components.classes;
  let Ci = Components.interfaces;
  let Cu = Components.utils;
  do_get_profile();
  Cu.import("resource://gre/modules/DownloadLastDir.jsm");

  function clearHistory() {
    // simulate clearing the private data
    Cc["@mozilla.org/observer-service;1"].
    getService(Ci.nsIObserverService).
    notifyObservers(null, "browser:purge-session-history", "");
  }

  do_check_eq(typeof gDownloadLastDir, "object");
  do_check_eq(gDownloadLastDir.file, null);

  let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIProperties);
  let tmpDir = dirSvc.get("TmpD", Ci.nsILocalFile);
  let newDir = tmpDir.clone();
  newDir.append("testdir" + Math.floor(Math.random() * 10000));
  newDir.QueryInterface(Ci.nsILocalFile);
  newDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0700);

  gDownloadLastDir.file = tmpDir;
  do_check_eq(gDownloadLastDir.file.path, tmpDir.path);
  do_check_neq(gDownloadLastDir.file, tmpDir);

  gDownloadLastDir.file = 1; // not an nsIFile
  do_check_eq(gDownloadLastDir.file, null);
  gDownloadLastDir.file = tmpDir;

  clearHistory();
  do_check_eq(gDownloadLastDir.file, null);
  gDownloadLastDir.file = tmpDir;

  let pb;
  try {
    pb = Cc["@mozilla.org/privatebrowsing;1"].
         getService(Ci.nsIPrivateBrowsingService);
  } catch (e) {
    print("PB service is not available, bail out");
    return;
  }

  let prefs = Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefBranch);
  prefs.setBoolPref("browser.privatebrowsing.keep_current_session", true);

  pb.privateBrowsingEnabled = true;
  do_check_eq(gDownloadLastDir.file.path, tmpDir.path);
  do_check_neq(gDownloadLastDir.file, tmpDir);

  pb.privateBrowsingEnabled = false;
  do_check_eq(gDownloadLastDir.file.path, tmpDir.path);
  pb.privateBrowsingEnabled = true;

  gDownloadLastDir.file = newDir;
  do_check_eq(gDownloadLastDir.file.path, newDir.path);
  do_check_neq(gDownloadLastDir.file, newDir);

  pb.privateBrowsingEnabled = false;
  do_check_eq(gDownloadLastDir.file.path, tmpDir.path);
  do_check_neq(gDownloadLastDir.file, tmpDir);

  pb.privateBrowsingEnabled = true;
  do_check_neq(gDownloadLastDir.file, null);
  clearHistory();
  do_check_eq(gDownloadLastDir.file, null);

  pb.privateBrowsingEnabled = false;
  do_check_eq(gDownloadLastDir.file, null);

  prefs.clearUserPref("browser.privatebrowsing.keep_current_session");
  newDir.remove(true);
}
