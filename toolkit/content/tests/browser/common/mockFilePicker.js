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

Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript("chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockObjects.js",
                 this);

var mockFilePickerSettings = {
  /**
   * File object pointing to the directory where the files will be saved.
   * The files will be saved with the default name, and will be overwritten
   * if they exist.
   */
  destDir: null,

  /**
   * Index of the filter to be returned by the file picker, or -1 to return
   * the filter proposed by the caller.
   */
  filterIndex: -1
};

var mockFilePickerResults = {
  /**
   * File object corresponding to the last automatically selected file.
   */
  selectedFile: null,

  /**
   * Index of the filter that was set on the file picker by the caller.
   */
  proposedFilterIndex: -1
};

/**
 * This file picker implementation uses the global settings defined in
 * mockFilePickerSettings, and updates the mockFilePickerResults object
 * when its "show" method is called.
 */
function MockFilePicker() { };
MockFilePicker.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFilePicker]),
  init: function () {},
  appendFilters: function () {},
  appendFilter: function () {},
  defaultString: "",
  defaultExtension: "",
  filterIndex: 0,
  displayDirectory: null,
  file: null,
  get fileURL() {
    return Cc["@mozilla.org/network/io-service;1"].
           getService(Ci.nsIIOService).newFileURI(this.file);
  },
  get files() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
  show: function MFP_show() {
    // Select the destination file with the specified default file name. If the
    // default file name was never specified or was set to an empty string by
    // the caller, ensure that a fallback file name is used.
    this.file = mockFilePickerSettings.destDir.clone();
    this.file.append(this.defaultString || "no_default_file_name");
    // Store the current file picker settings for testing them later.
    mockFilePickerResults.selectedFile = this.file.clone();
    mockFilePickerResults.proposedFilterIndex = this.filterIndex;
    // Select a different file filter if required.
    if (mockFilePickerSettings.filterIndex != -1)
      this.filterIndex = mockFilePickerSettings.filterIndex;
    // Assume we overwrite the file if it exists.
    return (this.file.exists() ?
            Ci.nsIFilePicker.returnReplace :
            Ci.nsIFilePicker.returnOK);
  }
};

// Create an instance of a MockObjectRegisterer whose methods can be used to
// temporarily replace the default "@mozilla.org/filepicker;1" object factory
// with one that provides the mock implementation above. To activate the mock
// object factory, call the "register" method. Starting from that moment, all
// the file picker objects that are requested will be mock objects, until the
// "unregister" method is called.
var mockFilePickerRegisterer =
  new MockObjectRegisterer("@mozilla.org/filepicker;1",
                           MockFilePicker);
