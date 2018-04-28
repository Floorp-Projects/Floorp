/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/FileUtils.jsm");
ChromeUtils.import("resource://gre/modules/Messaging.jsm");

Cu.importGlobalProperties(["File"]);

function FilePicker() {
}

FilePicker.prototype = {
  _mimeTypeFilter: 0,
  _extensionsFilter: "",
  _defaultString: "",
  _domWin: null,
  _domFile: null,
  _defaultExtension: null,
  _displayDirectory: null,
  _displaySpecialDirectory: null,
  _filePath: null,
  _promptActive: false,
  _filterIndex: 0,
  _addToRecentDocs: false,
  _title: "",

  init: function(aParent, aTitle, aMode) {
    this._domWin = aParent;
    this._mode = aMode;
    this._title = aTitle;

    let idService = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
    this.guid = idService.generateUUID().toString();

    if (aMode != Ci.nsIFilePicker.modeOpen && aMode != Ci.nsIFilePicker.modeOpenMultiple)
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;
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

  get displaySpecialDirectory() {
    return this._displaySpecialDirectory;
  },

  set displaySpecialDirectory(dir) {
    this._displaySpecialDirectory = dir;
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
    return this.getEnumerator([this.file]);
  },

  // We don't support directory selection yet.
  get domFileOrDirectory() {
    return this._domFile;
  },

  get domFileOrDirectoryEnumerator() {
    return this.getEnumerator([this._domFile]);
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

    Services.tm.spinEventLoopUntil(() => !this._promptActive);
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
      title: this._title,
    };

    // Knowing the window lets us destroy any temp files when the tab is closed
    // Other consumers of the file picker may have to either wait for Android
    // to clean up the temp dir (not guaranteed) or clean up after themselves.
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    let tab = win && win.BrowserApp.getTabForWindow(this._domWin.top);
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
    if (this._mode) {
        msg.modeOpenAttribute = this._mode;
    }

    EventDispatcher.instance.sendRequestForResult(msg).then(file => {
      this._filePath = file || null;
      this._promptActive = false;

      if (!file) {
        return;
      }

      if (this._domWin) {
        return this._domWin.File.createFromNsIFile(this.file, { existenceCheck: false });
      }

      return File.createFromNsIFile(this.file, { existenceCheck: false });
    }).then(domFile => {
      this._domFile = domFile;
    }, () => {
    }).then(() => {
      if (this._callback) {
        this._callback.done(this._filePath ?
          Ci.nsIFilePicker.returnOK : Ci.nsIFilePicker.returnCancel);
      }
      delete this._callback;
    });
  },

  getEnumerator: function(files) {
    return {
      QueryInterface: ChromeUtils.generateQI([Ci.nsISimpleEnumerator]),
      mFiles: files,
      mIndex: 0,
      hasMoreElements: function() {
        return (this.mIndex < this.mFiles.length);
      },
      getNext: function() {
        if (this.mIndex >= this.mFiles.length) {
          throw Cr.NS_ERROR_FAILURE;
        }
        return this.mFiles[this.mIndex++];
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
    } catch (ex) {
    }
  },

  classID: Components.ID("{18a4e042-7c7c-424b-a583-354e68553a7f}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIFilePicker, Ci.nsIObserver])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([FilePicker]);
