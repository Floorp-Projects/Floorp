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
 * The Original Code is the Application Update Service.
 *
 * The Initial Developer of the Original Code is
 * Robert Strong <robert.bugzilla@gmail.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Mozilla Foundation <http://www.mozilla.org/>. All Rights Reserved.
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

/* General MAR File Tests */

const DIR_DATA = "data"
const URL_PREFIX = "http://localhost:4444/" + DIR_DATA + "/";

const PREF_APP_UPDATE_URL_OVERRIDE = "app.update.url.override";

var gUpdates;
var gUpdateCount;
var gStatus;
var gExpectedStatus;
var gCheckFunc;
var gNextRunFunc;

var gTestDir;
var gUpdater;
var gUpdatesDir;
var gUpdatesDirPath;

function run_test() {
  do_test_pending();

  var fileLocator = AUS_Cc["@mozilla.org/file/directory_service;1"]
                      .getService(AUS_Ci.nsIProperties);

  // The directory the updates will be applied to is the current working
  // directory (e.g. obj-dir/toolkit/mozapps/update/test) and not dist/bin
  gTestDir = fileLocator.get("CurWorkD", AUS_Ci.nsIFile);
  // The mar files were created with all files in a subdirectory named
  // mar_test... clear it out of the way if it exists and recreate it.
  gTestDir.append("mar_test");
  if (gTestDir.exists())
    gTestDir.remove(true);
  gTestDir.create(AUS_Ci.nsIFile.DIRECTORY_TYPE, 0755);

  // Create an empty test file to test the complete mar's ability to replace an
  // existing file.
  var testFile = gTestDir.clone();
  testFile.append("text1");
  testFile.create(AUS_Ci.nsIFile.NORMAL_FILE_TYPE, 0644);

  var binDir = fileLocator.get("XCurProcD", AUS_Ci.nsIFile);

  // The updater binary file
  gUpdater = binDir.clone();
  gUpdater.append("updater.app");
  if (!gUpdater.exists()) {
    gUpdater = binDir.clone();
    gUpdater.append("updater.exe");
    if (!gUpdater.exists()) {
      gUpdater = binDir.clone();
      gUpdater.append("updater");
      if (!gUpdater.exists()) {
        do_throw("Unable to find updater binary!");
      }
    }
  }

  // The dir where the mar file is located
  gUpdatesDir = binDir.clone();
  gUpdatesDir.append("updates");
  gUpdatesDir.append("0");

  // Quoted path to the dir where the mar file is located
  gUpdatesDirPath = gUpdatesDir.path;
  if (/ /.test(gUpdatesDirPath))
    gUpdatesDirPath = '"' + gUpdatesDirPath + '"';

  startAUS();
  start_httpserver(DIR_DATA);
  do_timeout(0, "run_test_pt1()");
}

function end_test() {
  stop_httpserver();
  if (gTestDir.exists())
    gTestDir.remove(true);
  do_test_finished();
}

// Helper functions for testing mar downloads that have the correct size
// specified in the update xml.
function run_test_helper(aUpdateXML, aMsg, aResult, aNextRunFunc) {
  gUpdates = null;
  gUpdateCount = null;
  gStatus = null;
  gCheckFunc = check_test_helper_pt1;
  gNextRunFunc = aNextRunFunc;
  gExpectedStatus = aResult;
  var url = URL_PREFIX + aUpdateXML;
  dump("Testing: " + aMsg + " - " + url + "\n");
  gPrefs.setCharPref(PREF_APP_UPDATE_URL_OVERRIDE, url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_helper_pt1() {
  do_check_eq(gUpdateCount, 1);
  gCheckFunc = check_test_helper_pt2;
  var bestUpdate = gAUS.selectUpdate(gUpdates, gUpdateCount);
  var state = gAUS.downloadUpdate(bestUpdate, false);
  if (state == "null" || state == "failed")
    do_throw("nsIApplicationUpdateService:downloadUpdate returned " + state);
  gAUS.addDownloadListener(downloadListener);
}

function check_test_helper_pt2() {
  do_check_eq(gStatus, gExpectedStatus);
  gAUS.removeDownloadListener(downloadListener);
  gNextRunFunc();
}

// Launches the updater binary to apply a mar file
function runUpdate() {
  // Copy the updater binary to the update directory so the updater.ini is not
  // in the same directory as it is. This prevents ui from displaying and the
  // PostUpdate executable which is defined in the updater.ini from launching.
  gUpdater.copyTo(gUpdatesDir, gUpdater.leafName);
  var updateBin = gUpdatesDir.clone();
  updateBin.append(gUpdater.leafName);
  if (updateBin.leafName == "updater.app") {
    updateBin.append("Contents");
    updateBin.append("MacOS");
    updateBin.append("updater");
    if (!updateBin.exists())
      do_throw("Unable to find the updater executable!");
  }

  var process = AUS_Cc["@mozilla.org/process/util;1"]
                  .createInstance(AUS_Ci.nsIProcess);
  process.init(updateBin);
  var args = [gUpdatesDirPath];
  process.run(true, args, args.length);
  return process.exitValue;
}

// Gets a file in the directory (the current working directory) where the mar
// will be applied.
function getTestFile(leafName) {
  var file = gTestDir.clone();
  file.append(leafName);
  if (!(file instanceof AUS_Ci.nsILocalFile))
    do_throw("File must be a nsILocalFile for this test! File: " + leafName);

  return file;
}

// Returns the binary contents of a file
function getFileBytes(file) {
  var fis = AUS_Cc["@mozilla.org/network/file-input-stream;1"]
              .createInstance(AUS_Ci.nsIFileInputStream);
  fis.init(file, -1, -1, false);
  var bis = AUS_Cc["@mozilla.org/binaryinputstream;1"]
              .createInstance(AUS_Ci.nsIBinaryInputStream);
  bis.setInputStream(fis);
  var data = bis.readBytes(bis.available());
  bis.close();
  fis.close();
  return data;
}

// applying a complete mar and replacing an existing file
function run_test_pt1() {
  run_test_helper("aus-0110_general-1.xml", "applying a complete mar",
                  AUS_Cr.NS_OK, run_test_pt2);
}

// apply the complete mar and check the innards of the files
function run_test_pt2() {
  var exitValue = runUpdate();
  do_check_eq(exitValue, 0);

  dump("Testing: contents of files added\n");
  do_check_eq(getFileBytes(getTestFile("text1")), "ToBeModified\n");
  do_check_eq(getFileBytes(getTestFile("text2")), "ToBeDeleted\n");

  var refImage = do_get_file("toolkit/mozapps/update/test/unit/data/aus-0110_general_ref_image1.png");
  var srcImage = getTestFile("image1.png");
  do_check_eq(getFileBytes(srcImage), getFileBytes(refImage));

  remove_dirs_and_files();
  run_test_pt3();
}

// applying a partial mar and remove an existing file
function run_test_pt3() {
  run_test_helper("aus-0110_general-2.xml", "applying a partial mar",
                  AUS_Cr.NS_OK, run_test_pt4);
}

// apply the partial mar and check the innards of the files
function run_test_pt4() {
  var exitValue = runUpdate();
  do_check_eq(exitValue, 0);

  dump("Testing: removal of a file and contents of added / modified files\n");
  do_check_eq(getFileBytes(getTestFile("text1")), "Modified\n");
  do_check_false(getTestFile("text2").exists()); // file removed
  do_check_eq(getFileBytes(getTestFile("text3")), "Added\n");

  var refImage = do_get_file("toolkit/mozapps/update/test/unit/data/aus-0110_general_ref_image2.png");
  var srcImage = getTestFile("image1.png");
  do_check_eq(getFileBytes(srcImage), getFileBytes(refImage));

  end_test();
}

// Update check listener
const updateCheckListener = {
  onProgress: function(request, position, totalSize) {
  },

  onCheckComplete: function(request, updates, updateCount) {
    gUpdateCount = updateCount;
    gUpdates = updates;
    dump("onCheckComplete url = " + request.channel.originalURI.spec + "\n\n");
    // Use a timeout to allow the XHR to complete
    do_timeout(0, "gCheckFunc()");
  },

  onError: function(request, update) {
    dump("onError url = " + request.channel.originalURI.spec + "\n\n");
    // Use a timeout to allow the XHR to complete
    do_timeout(0, "gCheckFunc()");
  },

  QueryInterface: function(aIID) {
    if (!aIID.equals(AUS_Ci.nsIUpdateCheckListener) &&
        !aIID.equals(AUS_Ci.nsISupports))
      throw AUS_Cr.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

/* Update download listener - nsIRequestObserver */
const downloadListener = {
  onStartRequest: function(request, context) {
  },

  onProgress: function(request, context, progress, maxProgress) {
  },

  onStatus: function(request, context, status, statusText) {
  },

  onStopRequest: function(request, context, status) {
    gStatus = status;
    // Use a timeout to allow the request to complete
    do_timeout(0, "gCheckFunc()");
  },

  QueryInterface: function(iid) {
    if (!iid.equals(AUS_Ci.nsIRequestObserver) &&
        !iid.equals(AUS_Ci.nsIProgressEventSink) &&
        !iid.equals(AUS_Ci.nsISupports))
      throw AUS_Cr.NS_ERROR_NO_INTERFACE;
    return this;
  }
};
