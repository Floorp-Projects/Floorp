/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/DownloadLastDir.jsm");
Cu.import("resource://gre/modules/Services.jsm");

do_get_profile();

function run_test() {
  function clearHistory() {
    // simulate clearing the private data
    Services.obs.notifyObservers(null, "browser:purge-session-history", "");
  }

  do_check_eq(typeof gDownloadLastDir, "object");
  do_check_eq(gDownloadLastDir.file, null);
  
  let tmpDir = Services.dirsvc.get("TmpD", Ci.nsILocalFile);
  
  let uri1 = Services.io.newURI("http://test1.com/", null, null);
  let uri2 = Services.io.newURI("http://test2.com/", null, null);
  let uri3 = Services.io.newURI("http://test3.com/", null, null);
  let uri4 = Services.io.newURI("http://test4.com/", null, null);

  function newDir() {
    let dir = tmpDir.clone();
    dir.append("testdir" + Math.floor(Math.random() * 10000));
    dir.QueryInterface(Ci.nsILocalFile);
    dir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0700);
    return dir;
  }
  
  let dir1 = newDir();
  let dir2 = newDir();
  let dir3 = newDir();

  try {
    { // set up last dir
      gDownloadLastDir.setFile(null, tmpDir);
      do_check_eq(gDownloadLastDir.file.path, tmpDir.path);
      do_check_neq(gDownloadLastDir.file, tmpDir);
    }

    { // set uri1 to dir1, all should now return dir1
      // also check that a new object is returned
      gDownloadLastDir.setFile(uri1, dir1);
      do_check_eq(gDownloadLastDir.file.path, dir1.path);
      do_check_neq(gDownloadLastDir.file, dir1);
      do_check_eq(gDownloadLastDir.getFile(uri1).path, dir1.path); // set in CPS
      do_check_neq(gDownloadLastDir.getFile(uri1), dir1);
      do_check_eq(gDownloadLastDir.getFile(uri2).path, dir1.path); // fallback
      do_check_neq(gDownloadLastDir.getFile(uri2), dir1);
      do_check_eq(gDownloadLastDir.getFile(uri3).path, dir1.path); // fallback
      do_check_neq(gDownloadLastDir.getFile(uri3), dir1);
      do_check_eq(gDownloadLastDir.getFile(uri4).path, dir1.path); // fallback
      do_check_neq(gDownloadLastDir.getFile(uri4), dir1);
    }

    { // set uri2 to dir2, all except uri1 should now return dir2
      gDownloadLastDir.setFile(uri2, dir2);
      do_check_eq(gDownloadLastDir.file.path, dir2.path);
      do_check_eq(gDownloadLastDir.getFile(uri1).path, dir1.path); // set in CPS
      do_check_eq(gDownloadLastDir.getFile(uri2).path, dir2.path); // set in CPS
      do_check_eq(gDownloadLastDir.getFile(uri3).path, dir2.path); // fallback
      do_check_eq(gDownloadLastDir.getFile(uri4).path, dir2.path); // fallback
    }

    { // set uri3 to dir3, all except uri1 and uri2 should now return dir3
      gDownloadLastDir.setFile(uri3, dir3);
      do_check_eq(gDownloadLastDir.file.path, dir3.path);
      do_check_eq(gDownloadLastDir.getFile(uri1).path, dir1.path); // set in CPS
      do_check_eq(gDownloadLastDir.getFile(uri2).path, dir2.path); // set in CPS
      do_check_eq(gDownloadLastDir.getFile(uri3).path, dir3.path); // set in CPS
      do_check_eq(gDownloadLastDir.getFile(uri4).path, dir3.path); // fallback
    }

    { // set uri1 to dir2, all except uri3 should now return dir2
      gDownloadLastDir.setFile(uri1, dir2);
      do_check_eq(gDownloadLastDir.file.path, dir2.path);
      do_check_eq(gDownloadLastDir.getFile(uri1).path, dir2.path); // set in CPS
      do_check_eq(gDownloadLastDir.getFile(uri2).path, dir2.path); // set in CPS
      do_check_eq(gDownloadLastDir.getFile(uri3).path, dir3.path); // set in CPS
      do_check_eq(gDownloadLastDir.getFile(uri4).path, dir2.path); // fallback
    }

    { // check clearHistory removes all data
      clearHistory();
      do_check_eq(gDownloadLastDir.file, null);
      do_check_eq(Services.contentPrefs.hasPref(uri1, "browser.download.lastDir"), false);
      do_check_eq(gDownloadLastDir.getFile(uri1), null);
      do_check_eq(gDownloadLastDir.getFile(uri2), null);
      do_check_eq(gDownloadLastDir.getFile(uri3), null);
      do_check_eq(gDownloadLastDir.getFile(uri4), null);
    }

    let pb;
    try {
      pb = Cc["@mozilla.org/privatebrowsing;1"].getService(Ci.nsIPrivateBrowsingService);
    } catch (e) {
      print("PB service is not available, bail out");
      return;
    }

    Services.prefs.setBoolPref("browser.privatebrowsing.keep_current_session", true);
    
    { // check data set outside PB mode is remembered
      gDownloadLastDir.setFile(null, tmpDir);
      pb.privateBrowsingEnabled = true;
      do_check_eq(gDownloadLastDir.file.path, tmpDir.path);
      do_check_eq(gDownloadLastDir.getFile(uri1).path, tmpDir.path);

      pb.privateBrowsingEnabled = false;
      do_check_eq(gDownloadLastDir.file.path, tmpDir.path);
      do_check_eq(gDownloadLastDir.getFile(uri1).path, tmpDir.path);
      
      clearHistory();
    }

    { // check data set using CPS outside PB mode is remembered
      gDownloadLastDir.setFile(uri1, dir1);
      pb.privateBrowsingEnabled = true;
      do_check_eq(gDownloadLastDir.file.path, dir1.path);
      do_check_eq(gDownloadLastDir.getFile(uri1).path, dir1.path);

      pb.privateBrowsingEnabled = false;
      do_check_eq(gDownloadLastDir.file.path, dir1.path);
      do_check_eq(gDownloadLastDir.getFile(uri1).path, dir1.path);

      clearHistory();
    }
    
    { // check data set inside PB mode is forgotten
      pb.privateBrowsingEnabled = true;
      gDownloadLastDir.setFile(null, tmpDir);
      do_check_eq(gDownloadLastDir.file.path, tmpDir.path);
      do_check_eq(gDownloadLastDir.getFile(uri1).path, tmpDir.path);

      pb.privateBrowsingEnabled = false;
      do_check_eq(gDownloadLastDir.file, null);
      do_check_eq(gDownloadLastDir.getFile(uri1), null);
      
      clearHistory();
    }
    
    { // check data set using CPS inside PB mode is forgotten
      pb.privateBrowsingEnabled = true;
      gDownloadLastDir.setFile(uri1, dir1);
      do_check_eq(gDownloadLastDir.file.path, dir1.path);
      do_check_eq(gDownloadLastDir.getFile(uri1).path, dir1.path);

      pb.privateBrowsingEnabled = false;
      do_check_eq(gDownloadLastDir.file, null);
      do_check_eq(gDownloadLastDir.getFile(uri1), null);

      clearHistory();
    }

    { // check data set outside PB mode but changed inside is remembered correctly
      gDownloadLastDir.setFile(uri1, dir1);
      pb.privateBrowsingEnabled = true;
      gDownloadLastDir.setFile(uri1, dir2);
      do_check_eq(gDownloadLastDir.file.path, dir2.path);
      do_check_eq(gDownloadLastDir.getFile(uri1).path, dir2.path);

      pb.privateBrowsingEnabled = false;
      do_check_eq(gDownloadLastDir.file.path, dir1.path);
      do_check_eq(gDownloadLastDir.getFile(uri1).path, dir1.path);

      // check that the last dir store got cleared
      pb.privateBrowsingEnabled = true;
      do_check_eq(gDownloadLastDir.file.path, dir1.path);
      do_check_eq(gDownloadLastDir.getFile(uri1).path, dir1.path);
      
      pb.privateBrowsingEnabled = false;
      clearHistory();
    }
    
    { // check clearHistory inside PB mode clears data outside PB mode
      pb.privateBrowsingEnabled = true;
      gDownloadLastDir.setFile(uri1, dir2);

      clearHistory();
      do_check_eq(gDownloadLastDir.file, null);
      do_check_eq(gDownloadLastDir.getFile(uri1), null);

      pb.privateBrowsingEnabled = false;
      do_check_eq(gDownloadLastDir.file, null);
      do_check_eq(gDownloadLastDir.getFile(uri1), null);
    }

    { // check that disabling CPS works
      Services.prefs.setBoolPref("browser.download.lastDir.savePerSite", false);

      gDownloadLastDir.setFile(uri1, dir1);
      do_check_eq(gDownloadLastDir.file.path, dir1.path);
      do_check_eq(gDownloadLastDir.getFile(uri1).path, dir1.path);
      do_check_eq(gDownloadLastDir.getFile(uri2).path, dir1.path);
      do_check_eq(gDownloadLastDir.getFile(uri3).path, dir1.path);
      do_check_eq(gDownloadLastDir.getFile(uri4).path, dir1.path);

      gDownloadLastDir.setFile(uri2, dir2);
      do_check_eq(gDownloadLastDir.file.path, dir2.path);
      do_check_eq(gDownloadLastDir.getFile(uri1).path, dir2.path);
      do_check_eq(gDownloadLastDir.getFile(uri2).path, dir2.path);
      do_check_eq(gDownloadLastDir.getFile(uri3).path, dir2.path);
      do_check_eq(gDownloadLastDir.getFile(uri4).path, dir2.path);

      Services.prefs.clearUserPref("browser.download.lastDir.savePerSite");
    }

    { // check that passing null to setFile clears the stored value
      gDownloadLastDir.setFile(uri3, dir3);
      do_check_eq(gDownloadLastDir.getFile(uri3).path, dir3.path);
      gDownloadLastDir.setFile(uri3, null);
      do_check_eq(gDownloadLastDir.getFile(uri3), null);
    }
  } finally {
    dir1.remove(true);
    dir2.remove(true);
    dir3.remove(true);
    Services.prefs.clearUserPref("browser.download.lastDir.savePerSite");
  }
}
