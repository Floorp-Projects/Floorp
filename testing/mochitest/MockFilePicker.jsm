/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
