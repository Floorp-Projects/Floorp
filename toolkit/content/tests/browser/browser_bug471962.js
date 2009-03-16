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
 * The Original Code is Mozilla XUL Toolkit Testing Code.
 *
 * The Initial Developer of the Original Code is
 * Paolo Amadini <http://www.amadzone.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2009
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

function test()
{
  const Cc = Components.classes;
  const Ci = Components.interfaces;
  const Cu = Components.utils;
  const Cr = Components.results;
  const Cm = Components.manager;

  Cu.import("resource://gre/modules/XPCOMUtils.jsm");

  // --- Mock nsIFilePicker implementation ---

  var componentRegistrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);

  var originalFilePickerFactory;
  var mockFilePickerFactory;
  var destFile;

  // Note: The original class name is platform-dependent, however it is not
  // important that we restore the exact class name when we restore the
  // original factory.
  const kFilePickerCID = "{bd57cee8-1dd1-11b2-9fe7-95cf4709aea3}";
  const kFilePickerContractID = "@mozilla.org/filepicker;1";
  const kFilePickerPossibleClassName = "File Picker";

  function registerMockFilePickerFactory() {
    // This file picker implementation is tailored for this test and returns
    // the file specified in destFile, and a filter index of kSaveAsType_URL.
    // The file is overwritten if it exists.
    var mockFilePicker = {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIFilePicker]),
      init: function(aParent, aTitle, aMode) { },
      appendFilters: function(aFilterMask) { },
      appendFilter: function(aTitle, aFilter) { },
      defaultString: "",
      defaultExtension: "",
      set filterIndex() { },
      get filterIndex() {
        return 1; // kSaveAsType_URL
      },
      displayDirectory: null,
      get file() {
        return destFile.clone();
      },
      get fileURL() {
        return Cc["@mozilla.org/network/io-service;1"].
               getService(Ci.nsIIOService).newFileURI(destFile);
      },
      get files() {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      show: function() {
        // Assume we overwrite the file if it exists
        return (destFile.exists() ?
                Ci.nsIFilePicker.returnReplace :
                Ci.nsIFilePicker.returnOK);
      }
    };

    mockFilePickerFactory = {
      createInstance: function(aOuter, aIid) {
        if (aOuter != null)
          throw Cr.NS_ERROR_NO_AGGREGATION;
        return mockFilePicker.QueryInterface(aIid);
      }
    };

    // Preserve the original factory
    originalFilePickerFactory = Cm.getClassObject(Cc[kFilePickerContractID],
                                                  Ci.nsIFactory);

    // Register the mock factory
    componentRegistrar.registerFactory(
      Components.ID(kFilePickerCID),
      "Mock File Picker Implementation",
      kFilePickerContractID,
      mockFilePickerFactory
    );
  }

  function unregisterMockFilePickerFactory() {
    // Free references to the mock factory
    componentRegistrar.unregisterFactory(
      Components.ID(kFilePickerCID),
      mockFilePickerFactory
    );

    // Restore the original factory
    componentRegistrar.registerFactory(
      Components.ID(kFilePickerCID),
      kFilePickerPossibleClassName,
      kFilePickerContractID,
      originalFilePickerFactory
    );
  }

  // --- Download progress listener ---

  var downloadManager = Cc["@mozilla.org/download-manager;1"].
                        getService(Ci.nsIDownloadManager);

  var downloadListener = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDownloadProgressListener]),
    document: null,
    onDownloadStateChange: function(aState, aDownload) {
      switch (aDownload.state) {
        // The download finished successfully, continue the test
        case Ci.nsIDownloadManager.DOWNLOAD_FINISHED:
          onDownloadFinished(true);
          break;

        // The download finished with a failure, abort the test
        case Ci.nsIDownloadManager.DOWNLOAD_DIRTY:
        case Ci.nsIDownloadManager.DOWNLOAD_FAILED:
        case Ci.nsIDownloadManager.DOWNLOAD_CANCELED:
          onDownloadFinished(false);
          break;
      }
    },
    onStateChange: function(aWebProgress, aRequest, aState, aStatus,
     aDownload) { },
    onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress,
     aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress, aDownload) { },
    onSecurityChange: function(aWebProgress, aRequest, aState) { }
  }

  // --- Download Manager UI manual control ---

  var mainPrefBranch = Cc["@mozilla.org/preferences-service;1"].
                       getService(Ci.nsIPrefBranch);

  const kShowWhenStartingPref = "browser.download.manager.showWhenStarting";
  const kRetentionPref = "browser.download.manager.retention";

  function tweakDownloadManagerPrefs() {
    // Prevent the Download Manager UI from showing automatically
    mainPrefBranch.setBoolPref(kShowWhenStartingPref, false);
    // Prevent the download from remaining in the Download Manager UI
    mainPrefBranch.setIntPref(kRetentionPref, 0);
  }

  function restoreDownloadManagerPrefs() {
    mainPrefBranch.clearUserPref(kShowWhenStartingPref);
    mainPrefBranch.clearUserPref(kRetentionPref);
  }

  // --- Test procedure ---

  var innerFrame;

  function startTest() {
    waitForExplicitFinish();

    // Display the outer page
    gBrowser.addEventListener("pageshow", onPageShow, false);
    gBrowser.loadURI("http://localhost:8888/browser/toolkit/content/tests/browser/bug471962_testpage_outer.sjs");
  }

  function onPageShow() {
    gBrowser.removeEventListener("pageshow", onPageShow, false);

    // Submit the form in the outer page, then wait for both the outer
    //  document and the inner frame to be loaded again
    gBrowser.addEventListener("DOMContentLoaded", waitForTwoReloads, false);
    gBrowser.contentDocument.getElementById("postForm").submit();
  }

  var isFirstReload = true;

  function waitForTwoReloads() {
    // The first time this function is called, do nothing
    if (isFirstReload) {
      isFirstReload = false;
      return;
    }

    // The second time, go on with the normal test flow
    gBrowser.removeEventListener("DOMContentLoaded", waitForTwoReloads, false);

    // Save a reference to the inner frame in the reloaded page for later
    innerFrame = gBrowser.contentDocument.getElementById("innerFrame");

    // Submit the form in the inner page
    gBrowser.addEventListener("DOMContentLoaded", onInnerSubmitted, false);
    innerFrame.contentDocument.getElementById("postForm").submit();
  }

  function onInnerSubmitted() {
    gBrowser.removeEventListener("DOMContentLoaded", onInnerSubmitted, false);

    // Determine the path where the inner page will be saved
    destFile = Cc["@mozilla.org/file/directory_service;1"].
     getService(Ci.nsIProperties).get("TmpD", Ci.nsIFile);
    destFile.append("testsave_bug471962.html");

    // Wait for the download to finish
    downloadManager.addListener(downloadListener);

    // Prevent the test from showing the Download Manager UI
    tweakDownloadManagerPrefs();

    // Call the internal save function defined in contentAreaUtils.js, while
    // replacing the file picker component with a mock implementation that
    // returns the path of the temporary file
    registerMockFilePickerFactory();
    var docToSave = innerFrame.contentDocument;
    // We call internalSave instead of saveDocument to bypass the history cache
    internalSave(docToSave.location.href, docToSave, null, null,
                 docToSave.contentType, false, null, null,
                 docToSave.referrer ? makeURI(docToSave.referrer) : null,
                 false, null);
    unregisterMockFilePickerFactory();
  }

  function onDownloadFinished(aSuccess) {
    downloadManager.removeListener(downloadListener);
    restoreDownloadManagerPrefs();

    // Abort the test if the download wasn't successful
    if (!aSuccess) {
      ok(false, "Unexpected failure, the inner frame couldn't be saved!");
      finish();
      return;
    }

    // Read the entire file
    var inputStream = Cc["@mozilla.org/network/file-input-stream;1"].
                      createInstance(Ci.nsIFileInputStream);
    inputStream.init(destFile, -1, 0, 0);
    var scrInputStream = Cc["@mozilla.org/scriptableinputstream;1"].
                         createInstance(Ci.nsIScriptableInputStream);
    scrInputStream.init(inputStream);
    var fileContents = scrInputStream.
                       read(1048576); // The file is much shorter than 1 MB
    scrInputStream.close();
    inputStream.close();

    // Check if outer POST data is found
    const searchPattern = "inputfield=outer";
    ok(fileContents.indexOf(searchPattern) === -1,
       "The saved inner frame does not contain outer POST data");

    // Replace the current tab with a clean one
    gBrowser.addTab();
    gBrowser.removeCurrentTab();

    // Clean up and exit
    destFile.remove(false);
    finish();
  }

  // Start the test now that all the inner functions are defined
  startTest();
}
