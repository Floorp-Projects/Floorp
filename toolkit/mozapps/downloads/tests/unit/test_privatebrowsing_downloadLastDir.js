/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

do_get_profile();

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/DownloadLastDir.jsm");

let context = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIInterfaceRequestor]),
  getInterface: XPCOMUtils.generateQI([Ci.nsIDOMWindow])
};

let launcher = {
  source: Services.io.newURI("http://test1.com/file", null, null)
};

Cu.import("resource://test/MockFilePicker.jsm");
MockFilePicker.init();
MockFilePicker.returnValue = Ci.nsIFilePicker.returnOK;

function run_test()
{
  let pb;
  try {
    pb = Cc["@mozilla.org/privatebrowsing;1"].
         getService(Ci.nsIPrivateBrowsingService);
  } catch (e) {
    print("PB service is not available, bail out");
    return;
  }

  let prefsService = Cc["@mozilla.org/preferences-service;1"].
                     getService(Ci.nsIPrefService).
                     QueryInterface(Ci.nsIPrefBranch);
  prefsService.setBoolPref("browser.privatebrowsing.keep_current_session", true);
  let prefs = prefsService.getBranch("browser.download.");
  let launcherDialog = Cc["@mozilla.org/helperapplauncherdialog;1"].
                       getService(Ci.nsIHelperAppLauncherDialog);
  let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIProperties);
  let tmpDir = dirSvc.get("TmpD", Ci.nsILocalFile);
  function newDirectory() {
    let dir = tmpDir.clone();
    dir.append("testdir" + Math.floor(Math.random() * 10000));
    dir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0700);
    return dir;
  }
  function newFileInDirectory(dir) {
    let file = dir.clone();
    file.append("testfile" + Math.floor(Math.random() * 10000));
    file.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0600);
    return file;
  }
  let dir1 = newDirectory();
  let dir2 = newDirectory();
  let dir3 = newDirectory();
  let file1 = newFileInDirectory(dir1);
  let file2 = newFileInDirectory(dir2);
  let file3 = newFileInDirectory(dir3);

  prefs.setComplexValue("lastDir", Ci.nsILocalFile, tmpDir);

  MockFilePicker.returnFiles = [file1];
  let file = launcherDialog.promptForSaveToFile(launcher, context, null, null, null);
  do_check_true(!!file);
  // file picker should start with browser.download.lastDir
  do_check_eq(MockFilePicker.displayDirectory.path, tmpDir.path);
  // browser.download.lastDir should be modified before entering the private browsing mode
  do_check_eq(prefs.getComplexValue("lastDir", Ci.nsILocalFile).path, dir1.path);
  // gDownloadLastDir should be usable outside of the private browsing mode
  do_check_eq(gDownloadLastDir.file.path, dir1.path);

  pb.privateBrowsingEnabled = true;
  do_check_eq(prefs.getComplexValue("lastDir", Ci.nsILocalFile).path, dir1.path);
  MockFilePicker.returnFiles = [file2];
  MockFilePicker.displayDirectory = null;
  file = launcherDialog.promptForSaveToFile(launcher, context, null, null, null);
  do_check_true(!!file);
  // file picker should start with browser.download.lastDir as set before entering the private browsing mode
  do_check_eq(MockFilePicker.displayDirectory.path, dir1.path);
  // browser.download.lastDir should not be modified inside the private browsing mode
  do_check_eq(prefs.getComplexValue("lastDir", Ci.nsILocalFile).path, dir1.path);
  // but gDownloadLastDir should be modified
  do_check_eq(gDownloadLastDir.file.path, dir2.path);

  pb.privateBrowsingEnabled = false;
  // gDownloadLastDir should be cleared after leaving the private browsing mode
  do_check_eq(gDownloadLastDir.file.path, dir1.path);
  MockFilePicker.returnFiles = [file3];
  MockFilePicker.displayDirectory = null;
  file = launcherDialog.promptForSaveToFile(launcher, context, null, null, null);
  do_check_true(!!file);
  // file picker should start with browser.download.lastDir as set before entering the private browsing mode
  do_check_eq(MockFilePicker.displayDirectory.path, dir1.path);
  // browser.download.lastDir should be modified after leaving the private browsing mode
  do_check_eq(prefs.getComplexValue("lastDir", Ci.nsILocalFile).path, dir3.path);
  // gDownloadLastDir should be usable after leaving the private browsing mode
  do_check_eq(gDownloadLastDir.file.path, dir3.path);

  // cleanup
  prefsService.clearUserPref("browser.privatebrowsing.keep_current_session");
  [dir1, dir2, dir3].forEach(function(dir) dir.remove(true));

  MockFilePicker.cleanup();
}
