/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * No magic constructor behaviour, as is de rigeur for XPCOM.
 * If you must perform some initialization, and it could possibly fail (even
 * due to an out-of-memory condition), you should use an Init method, which
 * can convey failure appropriately (thrown exception in JS,
 * NS_FAILED(nsresult) return in C++).
 *
 * In JS, you can actually cheat, because a thrown exception will cause the
 * CreateInstance call to fail in turn, but not all languages are so lucky.
 * (Though ANSI C++ provides exceptions, they are verboten in Mozilla code
 * for portability reasons -- and even when you're building completely
 * platform-specific code, you can't throw across an XPCOM method boundary.)
 */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const DEBUG = false; /* set to true to enable debug messages */

const LOCAL_FILE_CONTRACTID = "@mozilla.org/file/local;1";
const APPSHELL_SERV_CONTRACTID  = "@mozilla.org/appshell/appShellService;1";
const STRBUNDLE_SERV_CONTRACTID = "@mozilla.org/intl/stringbundle;1";

const nsIAppShellService    = Components.interfaces.nsIAppShellService;
const nsILocalFile          = Components.interfaces.nsILocalFile;
const nsIFileURL            = Components.interfaces.nsIFileURL;
const nsISupports           = Components.interfaces.nsISupports;
const nsIFactory            = Components.interfaces.nsIFactory;
const nsIFilePicker         = Components.interfaces.nsIFilePicker;
const nsIInterfaceRequestor = Components.interfaces.nsIInterfaceRequestor;
const nsIDOMWindow          = Components.interfaces.nsIDOMWindow;
const nsIStringBundleService = Components.interfaces.nsIStringBundleService;
const nsIWebNavigation      = Components.interfaces.nsIWebNavigation;
const nsIDocShellTreeItem   = Components.interfaces.nsIDocShellTreeItem;
const nsIBaseWindow         = Components.interfaces.nsIBaseWindow;

var   titleBundle           = null;
var   filterBundle          = null;
var   lastDirectory         = null;

function nsFilePicker()
{
  if (!titleBundle)
    titleBundle = srGetStrBundle("chrome://global/locale/filepicker.properties");
  if (!filterBundle)
    filterBundle = srGetStrBundle("chrome://global/content/filepicker.properties");

  /* attributes */
  this.mDefaultString = "";
  this.mFilterIndex = 0;
  this.mFilterTitles = new Array();
  this.mFilters = new Array();
  this.mDisplayDirectory = null;
  if (lastDirectory) {
    try {
      var dir = Components.classes[LOCAL_FILE_CONTRACTID].createInstance(nsILocalFile);
      dir.initWithPath(lastDirectory);
      this.mDisplayDirectory = dir;
    } catch (e) {}
  }
}

nsFilePicker.prototype = {
  classID: Components.ID("{54ae32f8-1dd2-11b2-a209-df7c505370f8}"),

  QueryInterface: function(iid) {
    if (iid.equals(nsIFilePicker) ||
        iid.equals(nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },


  /* attribute nsILocalFile displayDirectory; */
  set displayDirectory(a) {
    this.mDisplayDirectory = a &&
      a.clone().QueryInterface(nsILocalFile);
  },
  get displayDirectory()  {
    return this.mDisplayDirectory &&
           this.mDisplayDirectory.clone()
               .QueryInterface(nsILocalFile);
  },

  /* readonly attribute nsILocalFile file; */
  get file()  { return this.mFilesEnumerator.mFiles[0]; },

  /* readonly attribute nsISimpleEnumerator files; */
  get files()  { return this.mFilesEnumerator; },

  /* readonly attribute nsIDOMFile domfile; */
  get domfile()  {
    let enumerator = this.domfiles;
    return enumerator ? enumerator.mFiles[0] : null;
  },

  /* readonly attribute nsISimpleEnumerator domfiles; */
  get domfiles()  {
    if (!this.mFilesEnumerator) {
      return null;
    }

    if (!this.mDOMFilesEnumerator) {
      this.mDOMFilesEnumerator = {
        QueryInterface: XPCOMUtils.generateQI([Components.interfaces.nsISimpleEnumerator]),

        mFiles: [],
        mIndex: 0,

        hasMoreElements: function() {
          return (this.mIndex < this.mFiles.length);
        },

        getNext: function() {
          if (this.mIndex >= this.mFiles.length) {
            throw Components.results.NS_ERROR_FAILURE;
          }
          return this.mFiles[this.mIndex++];
        }
      };

      var utils = this.mParentWindow.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                                    .getInterface(Components.interfaces.nsIDOMWindowUtils);

      for (var i = 0; i < this.mFilesEnumerator.mFiles.length; ++i) {
        var file = utils.wrapDOMFile(this.mFilesEnumerator.mFiles[i]);
        this.mDOMFilesEnumerator.mFiles.push(file);
      }
    }

    return this.mDOMFilesEnumerator;
  },

  /* readonly attribute nsIURI fileURL; */
  get fileURL()  {
    if (this.mFileURL)
      return this.mFileURL;

    if (!this.mFilesEnumerator)
      return null;

      var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                    .getService(Components.interfaces.nsIIOService);

    return this.mFileURL = ioService.newFileURI(this.file);
  },

  /* attribute wstring defaultString; */
  set defaultString(a) { this.mDefaultString = a; },
  get defaultString()  { return this.mDefaultString; },

  /* attribute wstring defaultExtension */
  set defaultExtension(ext) { },
  get defaultExtension() { return ""; },

  /* attribute long filterIndex; */
  set filterIndex(a) { this.mFilterIndex = a; },
  get filterIndex() { return this.mFilterIndex; },

  /* attribute boolean addToRecentDocs; */
  set addToRecentDocs(a) {},
  get addToRecentDocs()  { return false; },

  /* readonly attribute short mode; */
  get mode() { return this.mMode; },

  /* members */
  mFilesEnumerator: undefined,
  mDOMFilesEnumerator: undefined,
  mParentWindow: null,

  /* methods */
  init: function(parent, title, mode) {
    this.mParentWindow = parent;
    this.mTitle = title;
    this.mMode = mode;
  },

  appendFilters: function(filterMask) {
    if (filterMask & nsIFilePicker.filterHTML) {
      this.appendFilter(titleBundle.GetStringFromName("htmlTitle"),
                        filterBundle.GetStringFromName("htmlFilter"));
    }
    if (filterMask & nsIFilePicker.filterText) {
      this.appendFilter(titleBundle.GetStringFromName("textTitle"),
                        filterBundle.GetStringFromName("textFilter"));
    }
    if (filterMask & nsIFilePicker.filterImages) {
      this.appendFilter(titleBundle.GetStringFromName("imageTitle"),
                        filterBundle.GetStringFromName("imageFilter"));
    }
    if (filterMask & nsIFilePicker.filterXML) {
      this.appendFilter(titleBundle.GetStringFromName("xmlTitle"),
                        filterBundle.GetStringFromName("xmlFilter"));
    }
    if (filterMask & nsIFilePicker.filterXUL) {
      this.appendFilter(titleBundle.GetStringFromName("xulTitle"),
                        filterBundle.GetStringFromName("xulFilter"));
    }
    this.mAllowURLs = !!(filterMask & nsIFilePicker.filterAllowURLs);
    if (filterMask & nsIFilePicker.filterApps) {
      // We use "..apps" as a special filter for executable files
      this.appendFilter(titleBundle.GetStringFromName("appsTitle"),
                        "..apps");
    }
    if (filterMask & nsIFilePicker.filterAudio) {
      this.appendFilter(titleBundle.GetStringFromName("audioTitle"),
                        filterBundle.GetStringFromName("audioFilter"));
    }
    if (filterMask & nsIFilePicker.filterVideo) {
      this.appendFilter(titleBundle.GetStringFromName("videoTitle"),
                        filterBundle.GetStringFromName("videoFilter"));
    }
    if (filterMask & nsIFilePicker.filterAll) {
      this.appendFilter(titleBundle.GetStringFromName("allTitle"),
                        filterBundle.GetStringFromName("allFilter"));
    }
  },

  appendFilter: function(title, extensions) {
    this.mFilterTitles.push(title);
    this.mFilters.push(extensions);
  },

  open: function(aFilePickerShownCallback) {
    var tm = Components.classes["@mozilla.org/thread-manager;1"]
                       .getService(Components.interfaces.nsIThreadManager);
    tm.mainThread.dispatch(function() {
      let result = Components.interfaces.nsIFilePicker.returnCancel;
      try {
        result = this.show();
      } catch(ex) {
      }
      if (aFilePickerShownCallback) {
        aFilePickerShownCallback.done(result);
      }
    }.bind(this), Components.interfaces.nsIThread.DISPATCH_NORMAL);
  },

  show: function() {
    var o = new Object();
    o.title = this.mTitle;
    o.mode = this.mMode;
    o.displayDirectory = this.mDisplayDirectory;
    o.defaultString = this.mDefaultString;
    o.filterIndex = this.mFilterIndex;
    o.filters = new Object();
    o.filters.titles = this.mFilterTitles;
    o.filters.types = this.mFilters;
    o.allowURLs = this.mAllowURLs;
    o.retvals = new Object();

    var parent;
    if (this.mParentWindow) {
      parent = this.mParentWindow;
    } else if (typeof(window) == "object" && window != null) {
      parent = window;
    } else {
      try {
        var appShellService = Components.classes[APPSHELL_SERV_CONTRACTID].getService(nsIAppShellService);
        parent = appShellService.hiddenDOMWindow;
      } catch(ex) {
        debug("Can't get parent.  xpconnect hates me so we can't get one from the appShellService.\n");
        debug(ex + "\n");
      }
    }

    var parentWin = null;
    try {
      parentWin = parent.QueryInterface(nsIInterfaceRequestor)
                        .getInterface(nsIWebNavigation)
                        .QueryInterface(nsIDocShellTreeItem)
                        .treeOwner
                        .QueryInterface(nsIInterfaceRequestor)
                        .getInterface(nsIBaseWindow);
    } catch(ex) {
      dump("file picker couldn't get base window\n"+ex+"\n");
    }
    try {
      parent.openDialog("chrome://global/content/filepicker.xul",
                        "",
                        "chrome,modal,titlebar,resizable=yes,dependent=yes",
                        o);

      this.mFilterIndex = o.retvals.filterIndex;
      this.mFilesEnumerator = o.retvals.files;
      this.mFileURL = o.retvals.fileURL;
      lastDirectory = o.retvals.directory;
      return o.retvals.buttonStatus;
    } catch(ex) { dump("unable to open file picker\n" + ex + "\n"); }

    return null;
  }
}

if (DEBUG)
  debug = function (s) { dump("-*- filepicker: " + s + "\n"); };
else
  debug = function (s) {};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([nsFilePicker]);

/* crap from strres.js that I want to use for string bundles since I can't include another .js file.... */

var strBundleService = null;

function srGetStrBundle(path)
{
  var strBundle = null;

  if (!strBundleService) {
    try {
      strBundleService = Components.classes[STRBUNDLE_SERV_CONTRACTID].getService(nsIStringBundleService);
    } catch (ex) {
      dump("\n--** strBundleService createInstance failed **--\n");
      return null;
    }
  }

  strBundle = strBundleService.createBundle(path);
  if (!strBundle) {
	dump("\n--** strBundle createInstance failed **--\n");
  }
  return strBundle;
}

