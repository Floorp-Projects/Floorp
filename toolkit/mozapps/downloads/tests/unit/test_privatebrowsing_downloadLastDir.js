/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is bug 464795 unit test.
 *
 * The Initial Developer of the Original Code is
 * Ehsan Akhgari.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Ehsan Akhgari <ehsan.akhgari@gmail.com>
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

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

// Code borrowed from toolkit/components/downloadmgr/test/unit/head_download_manager.js
var dirSvc = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties);
var profileDir = null;
try {
  profileDir = dirSvc.get("ProfD", Ci.nsIFile);
} catch (e) { }
if (!profileDir) {
  // Register our own provider for the profile directory.
  // It will simply return the current directory.
  var provider = {
    getFile: function(prop, persistent) {
      persistent.value = true;
      if (prop == "ProfD") {
        return dirSvc.get("CurProcD", Ci.nsILocalFile);
      } else if (prop == "DLoads") {
        var file = dirSvc.get("CurProcD", Ci.nsILocalFile);
        file.append("downloads.rdf");
        return file;
      }
      print("*** Throwing trying to get " + prop);
      throw Cr.NS_ERROR_FAILURE;
    },
    QueryInterface: function(iid) {
      if (iid.equals(Ci.nsIDirectoryServiceProvider) ||
          iid.equals(Ci.nsISupports)) {
        return this;
      }
      throw Cr.NS_ERROR_NO_INTERFACE;
    }
  };
  dirSvc.QueryInterface(Ci.nsIDirectoryService).registerProvider(provider);
}

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
MockFilePicker.reset();
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
  dirSvc.QueryInterface(Ci.nsIDirectoryService).unregisterProvider(provider);

  MockFilePicker.reset();
}
