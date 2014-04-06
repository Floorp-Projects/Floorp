/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cu = Components.utils;
const Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

function FilePicker() {
}

FilePicker.prototype = {
  _mimeTypeFilter: 0,
  _extensionsFilter: "",
  _defaultString: "",
  _domWin: null,
  _defaultExtension: null,
  _displayDirectory: null,
  _filePath: null,
  _promptActive: false,
  _filterIndex: 0,
  _addToRecentDocs: false,

  init: function(aParent, aTitle, aMode) {
    this._domWin = aParent;
    this._mode = aMode;
    Services.obs.addObserver(this, "FilePicker:Result", false);

    let idService = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator); 
    this.guid = idService.generateUUID().toString();

    if (aMode != Ci.nsIFilePicker.modeOpen && aMode != Ci.nsIFilePicker.modeOpenMultiple)
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },

  appendFilters: function(aFilterMask) {
    if (aFilterMask & Ci.nsIFilePicker.filterAudio) {
      this._mimeTypeFilter = "audio/*";
      return;
    }

    if (aFilterMask & Ci.nsIFilePicker.filterImages) {
      this._mimeTypeFilter = "image/*";
      return;
    }

    if (aFilterMask & Ci.nsIFilePicker.filterVideo) {
      this._mimeTypeFilter = "video/*";
      return;
    }

    if (aFilterMask & Ci.nsIFilePicker.filterAll) {
      this._mimeTypeFilter = "*/*";
      return;
    }

    /* From BaseFilePicker.cpp */
    if (aFilterMask & Ci.nsIFilePicker.filterHTML) {
      this.appendFilter("*.html; *.htm; *.shtml; *.xhtml");
    }
    if (aFilterMask & Ci.nsIFilePicker.filterText) {
      this.appendFilter("*.txt; *.text");
    }

    if (aFilterMask & Ci.nsIFilePicker.filterXML) {
      this.appendFilter("*.xml");
    }

    if (aFilterMask & Ci.nsIFilePicker.xulFilter) {
      this.appendFilter("*.xul");
    }

    if (aFilterMask & Ci.nsIFilePicker.xulFilter) {
      this.appendFilter("..apps");
    }
  },

  appendFilter: function(title, filter) {
    if (this._extensionsFilter)
        this._extensionsFilter += ", ";
    this._extensionsFilter += filter;
  },

  get defaultString() {
    return this._defaultString;
  },

  set defaultString(defaultString) {
    this._defaultString = defaultString;
  },

  get defaultExtension() {
    return this._defaultExtension;
  },

  set defaultExtension(defaultExtension) {
    this._defaultExtension = defaultExtension;
  },

  get filterIndex() {
    return this._filterIndex;
  },

  set filterIndex(val) {
    this._filterIndex = val;
  },
  
  get displayDirectory() {
    return this._displayDirectory;
  },

  set displayDirectory(dir) {
    this._displayDirectory = dir;
  },

  get file() {
    if (!this._filePath) {
        return null;
    }

    return new FileUtils.File(this._filePath);
  },

  get fileURL() {
    let file = this.getFile();
    return Services.io.newFileURI(file);
  },

  get files() {
    return this.getEnumerator([this.file], function(file) {
      return file;
    });
  },

  get domfile() {
    let f = this.file;
    if (!f) {
        return null;
    }
    return File(f);
  },

  get domfiles() {
    return this.getEnumerator([this.file], function(file) {
      return File(file);
    });
  },

  get addToRecentDocs() {
    return this._addToRecentDocs;
  },

  set addToRecentDocs(val) {
    this._addToRecentDocs = val;
  },

  get mode() {
    return this._mode;
  },

  show: function() {
    if (this._domWin) {
      this.fireDialogEvent(this._domWin, "DOMWillOpenModalDialog");
      let winUtils = this._domWin.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
      winUtils.enterModalState();
    }

    this._promptActive = true;
    this._sendMessage();

    let thread = Services.tm.currentThread;
    while (this._promptActive)
      thread.processNextEvent(true);
    delete this._promptActive;

    if (this._domWin) {
      let winUtils = this._domWin.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
      winUtils.leaveModalState();
      this.fireDialogEvent(this._domWin, "DOMModalDialogClosed");
    }

    if (this._filePath)
      return Ci.nsIFilePicker.returnOK;

    return Ci.nsIFilePicker.returnCancel;
  },

  open: function(callback) {
    this._callback = callback;
    this._sendMessage();
  },

  _sendMessage: function() {
    let msg = {
      type: "FilePicker:Show",
      guid: this.guid,
    };

    // Knowing the window lets us destroy any temp files when the tab is closed
    // Other consumers of the file picker may have to either wait for Android
    // to clean up the temp dir (not guaranteed) or clean up after themselves.
    let win = Services.wm.getMostRecentWindow('navigator:browser');
    let tab = win.BrowserApp.getTabForWindow(this._domWin.top)
    if (tab) {
      msg.tabId = tab.id;
    }

    if (!this._extensionsFilter && !this._mimeTypeFilter) {
      // If neither filters is set show anything we can.
      msg.mode = "mimeType";
      msg.mimeType = "*/*";
    } else if (this._extensionsFilter) {
      msg.mode = "extension";
      msg.extensions = this._extensionsFilter;
    } else {
      msg.mode = "mimeType";
      msg.mimeType = this._mimeTypeFilter;
    }

    this.sendMessageToJava(msg);
  },

  sendMessageToJava: function(aMsg) {
    Services.androidBridge.handleGeckoMessage(aMsg);
  },

  observe: function(aSubject, aTopic, aData) {
    let data = JSON.parse(aData);
    if (data.guid != this.guid)
      return;

    this._filePath = null;
    if (data.file)
      this._filePath = data.file;

    this._promptActive = false;

    if (this._callback) {
      this._callback.done(this._filePath ? Ci.nsIFilePicker.returnOK : Ci.nsIFilePicker.returnCancel);
    }
    delete this._callback;
  },

  getEnumerator: function(files, mapFunction) {
    return {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsISimpleEnumerator]),
      mFiles: files,
      mIndex: 0,
      hasMoreElements: function() {
        return (this.mIndex < this.mFiles.length);
      },
      getNext: function() {
        if (this.mIndex >= this.mFiles.length) {
          throw Components.results.NS_ERROR_FAILURE;
        }
        return mapFunction(this.mFiles[this.mIndex++]);
      }
    };
  },

  fireDialogEvent: function(aDomWin, aEventName) {
    // accessing the document object can throw if this window no longer exists. See bug 789888.
    try {
      if (!aDomWin.document)
        return;
      let event = aDomWin.document.createEvent("Events");
      event.initEvent(aEventName, true, true);
      let winUtils = aDomWin.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
      winUtils.dispatchEventToChromeOnly(aDomWin, event);
    } catch(ex) {
    }
  },

  classID: Components.ID("{18a4e042-7c7c-424b-a583-354e68553a7f}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFilePicker, Ci.nsIObserver])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([FilePicker]);
