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
 * The Original Code is Reusable Mock File Picker.
 *
 * The Initial Developer of the Original Code is
 * Geoff Lankow <geoff@darktrojan.net>.
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

var EXPORTED_SYMBOLS = ["MockFilePicker"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cm = Components.manager;
const Cu = Components.utils;

const CONTRACT_ID = "@mozilla.org/filepicker;1";

Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

var registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
var oldClassID, oldFactory;
var newClassID = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator).generateUUID();
var newFactory = {
  createInstance: function(aOuter, aIID) {
    if (aOuter)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return new MockFilePickerInstance().QueryInterface(aIID);
  },
  lockFactory: function(aLock) {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFactory])
};

var MockFilePicker = {
  returnOK: Ci.nsIFilePicker.returnOK,
  returnCancel: Ci.nsIFilePicker.returnCancel,
  returnReplace: Ci.nsIFilePicker.returnReplace,

  filterAll: Ci.nsIFilePicker.filterAll,
  filterHTML: Ci.nsIFilePicker.filterHTML,
  filterText: Ci.nsIFilePicker.filterText,
  filterImages: Ci.nsIFilePicker.filterImages,
  filterXML: Ci.nsIFilePicker.filterXML,
  filterXUL: Ci.nsIFilePicker.filterXUL,
  filterApps: Ci.nsIFilePicker.filterApps,
  filterAllowURLs: Ci.nsIFilePicker.filterAllowURLs,
  filterAudio: Ci.nsIFilePicker.filterAudio,
  filterVideo: Ci.nsIFilePicker.filterVideo,

  init: function() {
    this.reset();
    if (!registrar.isCIDRegistered(newClassID)) {
      oldClassID = registrar.contractIDToCID(CONTRACT_ID);
      oldFactory = Cm.getClassObject(Cc[CONTRACT_ID], Ci.nsIFactory);
      registrar.unregisterFactory(oldClassID, oldFactory);
      registrar.registerFactory(newClassID, "", CONTRACT_ID, newFactory);
    }
  },
  
  reset: function() {
    this.appendFilterCallback = null;
    this.appendFiltersCallback = null;
    this.displayDirectory = null;
    this.filterIndex = 0;
    this.mode = null;
    this.returnFiles = [];
    this.returnValue = null;
    this.showCallback = null;
    this.shown = false;
  },
  
  cleanup: function() {
    this.reset();
    if (oldFactory) {
      registrar.unregisterFactory(newClassID, newFactory);
      registrar.registerFactory(oldClassID, "", CONTRACT_ID, oldFactory);
    }
  },

  useAnyFile: function() {
    var file = FileUtils.getFile("TmpD", ["testfile"]);
    file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0644);
    this.returnFiles = [file];
  }
};

function MockFilePickerInstance() { };
MockFilePickerInstance.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFilePicker]),
  init: function(aParent, aTitle, aMode) {
    MockFilePicker.mode = aMode;
    this.filterIndex = MockFilePicker.filterIndex;
  },
  appendFilter: function(aTitle, aFilter) {
    if (typeof MockFilePicker.appendFilterCallback == "function")
      MockFilePicker.appendFilterCallback(this, aTitle, aFilter);
  },
  appendFilters: function(aFilterMask) {
    if (typeof MockFilePicker.appendFiltersCallback == "function")
      MockFilePicker.appendFiltersCallback(this, aFilterMask);
  },
  defaultString: "",
  defaultExtension: "",
  filterIndex: 0,
  displayDirectory: null,
  get file() {
    if (MockFilePicker.returnFiles.length >= 1)
      return MockFilePicker.returnFiles[0];
    return null;
  },
  get fileURL() {
    if (MockFilePicker.returnFiles.length >= 1)
      return Services.io.newFileURI(MockFilePicker.returnFiles[0]);
    return null;
  },
  get files() {
    return {
      index: 0,
      QueryInterface: XPCOMUtils.generateQI([Ci.nsISimpleEnumerator]),
      hasMoreElements: function() {
        return this.index < MockFilePicker.returnFiles.length;
      },
      getNext: function() {
        return MockFilePicker.returnFiles[this.index++];
      }
    };
  },
  show: function() {
    MockFilePicker.displayDirectory = this.displayDirectory;
    MockFilePicker.shown = true;
    if (typeof MockFilePicker.showCallback == "function") {
      var returnValue = MockFilePicker.showCallback(this);
      if (typeof returnValue != "undefined")
        return returnValue;
    }
    return MockFilePicker.returnValue;
  }
};
