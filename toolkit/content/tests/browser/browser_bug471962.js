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

  // --- Mock nsITransfer implementation ---

  var originalTransferFactory;
  var mockTransferFactory;
  var downloadIsSuccessful = true;

  const kDownloadCID = "{e3fa9d0a-1dd1-11b2-bdef-8c720b597445}";
  const kTransferContractID = "@mozilla.org/transfer;1";
  const kDownloadClassName = "Download";

  function registerMockTransferFactory() {
    // This "transfer" object implementation is tailored for this test, and
    // continues the test when the download is completed.
    var mockTransfer = {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                             Ci.nsIWebProgressListener2,
                                             Ci.nsITransfer]),

      // --- nsIWebProgressListener interface functions ---

      onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {
        // If at least one notification reported an error, the download failed
        if (aStatus != Cr.NS_OK)
          downloadIsSuccessful = false;

        // If the download is finished
        if ((aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) &&
            (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK))
          // Continue the test, reporting the success or failure condition
          onDownloadFinished(downloadIsSuccessful);
      },
      onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress,
       aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress) { },
      onLocationChange: function(aWebProgress, aRequest, aLocation) { },
      onStatusChange: function(aWebProgress, aRequest, aStatus, aMessage) {
        // If at least one notification reported an error, the download failed
        if (aStatus != Cr.NS_OK)
          downloadIsSuccessful = false;
      },
      onSecurityChange: function(aWebProgress, aRequest, aState) { },

      // --- nsIWebProgressListener2 interface functions ---

      onProgressChange64: function(aWebProgress, aRequest, aCurSelfProgress,
       aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress) { },
      onRefreshAttempted: function(aWebProgress, aRefreshURI, aMillis,
       aSameURI) { },

      // --- nsITransfer interface functions ---

      init: function(aSource, aTarget, aDisplayName, aMIMEInfo, aStartTime,
       aTempFile, aCancelable) { }
    };

    mockTransferFactory = {
      createInstance: function(aOuter, aIid) {
        if (aOuter != null)
          throw Cr.NS_ERROR_NO_AGGREGATION;
        return mockTransfer.QueryInterface(aIid);
      }
    };

    // Preserve the original factory
    originalTransferFactory = Cm.getClassObject(Cc[kTransferContractID],
                                                Ci.nsIFactory);

    // Register the mock factory
    componentRegistrar.registerFactory(
      Components.ID(kDownloadCID),
      "Mock Transfer Implementation",
      kTransferContractID,
      mockTransferFactory
    );
  }

  function unregisterMockTransferFactory() {
    // Free references to the mock factory
    componentRegistrar.unregisterFactory(
      Components.ID(kDownloadCID),
      mockTransferFactory
    );

    // Restore the original factory
    componentRegistrar.registerFactory(
      Components.ID(kDownloadCID),
      kDownloadClassName,
      kTransferContractID,
      originalTransferFactory
    );
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

    // Call the internal save function defined in contentAreaUtils.js, while
    // replacing the file picker component with a mock implementation that
    // returns the path of the temporary file, and the download component with
    // an implementation that does not depend on the download manager
    registerMockFilePickerFactory();
    registerMockTransferFactory();
    var docToSave = innerFrame.contentDocument;
    // We call internalSave instead of saveDocument to bypass the history cache
    internalSave(docToSave.location.href, docToSave, null, null,
                 docToSave.contentType, false, null, null,
                 docToSave.referrer ? makeURI(docToSave.referrer) : null,
                 false, null);
    unregisterMockTransferFactory();
    unregisterMockFilePickerFactory();
  }

  function onDownloadFinished(aSuccess) {
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
