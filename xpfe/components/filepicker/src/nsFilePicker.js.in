/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Stuart Parmenter <pavlov@netscape.com>
 */

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


const DEBUG = true; /* set to false to suppress debug messages */
const FILEPICKER_PROGID   = "component://mozilla/filepicker";
const FILEPICKER_CID      = Components.ID("{54ae32f8-1dd2-11b2-a209-df7c505370f8}");
const nsILocalFile        = Components.interfaces.nsILocalFile;
const nsISupports         = Components.interfaces.nsISupports;
const nsIFactory          = Components.interfaces.nsIFactory;
const nsIFile             = Components.interfaces.nsIFile;
const nsIFilePicker       = Components.interfaces.nsIFilePicker;

function nsFilePicker()
{
  /* attributes */
  this.mSelectedFilter = 0;
  this.mDefaultString = "";
  this.mDisplayDirectory = "";
}

nsFilePicker.prototype = {

  /* attribute nsIFile displayDirectory; */
  set displayDirectory(a) { this.mDisplayDirectory = a; },
  get displayDirectory()  { return this.mDisplayDirectory; },

  /* readonly attribute nsIFile file; */
  set file(a) { throw "readonly property"; },
  get file()  { debug("getter called " + this.mFile); return this.mFile; },

  /* readonly attribute long selectedFilter; */
  set selectedFilter(a) { throw "readonly property"; },
  get selectedFilter()  { return this.mSelectedFilter; },

  /* attribute wstring defaultString; */
  set defaultString(a) { throw "readonly property"; },
  get defaultString()  { return this.mSelectedFilter; },

  /* methods */
  create: function(parent, title, mode) {
    this.mParentWindow = parent;
    this.mTitle = title;
    this.mMode = mode;
  },

  setFilterList: function(numFilters, titles, types) {
    this.mNumFilters = numFilters;
    this.mFilterTitles = titles;
    this.mFilterTypes = types;
  },

  show: function() {
    var o = new Object();
    o.title = this.mTitle;
    o.mode = this.mMode;
    o.filters = new Object();
    o.filters.length = this.mNumFilters;
    o.filters.titles = this.mFilterTitles;
    o.filters.types = this.mFilterTypes;
    o.retvals = new Object();

    this.mParentWindow.openDialog("chrome://global/content/filepicker.xul",
                                 "",
                                 "chrome,modal,resizeable=yes,dependent=yes",
                                 o);

    this.mFile = o.retvals.file;
  }
}

if (DEBUG)
    debug = function (s) { dump("-*- filepicker: " + s + "\n"); }
else
    debug = function (s) {}

/* module foo */

var filePickerModule = new Object();

filePickerModule.registerSelf =
function (compMgr, fileSpec, location, type)
{
    debug("registering (all right -- a JavaScript module!)");
    compMgr.registerComponentWithType(FILEPICKER_CID, "FilePicker JS Component",
                                      FILEPICKER_PROGID, fileSpec, location,
                                      true, true, type);
}

filePickerModule.getClassObject =
function (compMgr, cid, iid) {
    if (!cid.equals(FILEPICKER_CID))
        throw Components.results.NS_ERROR_NO_INTERFACE;
    
    if (!iid.equals(Components.interfaces.nsIFactory))
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    
    return filePickerFactory;
}

filePickerModule.canUnload =
function(compMgr)
{
    debug("Unloading component.");
    return true;
}
    
/* factory object */
filePickerFactory = new Object();

filePickerFactory.CreateInstance =
function (outer, iid) {
    debug("CI: " + iid);
    debug("IID:" + nsIFilePicker);
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsIFilePicker) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new nsFilePicker();
}

/* entrypoint */
function NSGetModule(compMgr, fileSpec) {
    return filePickerModule;
}
