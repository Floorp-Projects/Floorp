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
const nsIFilePicker       = Components.interfaces.nsIFilePicker;
const bundle              = srGetStrBundle("chrome://global/locale/filepicker.properties");
var   lastDirectory       = "/";

function nsFilePicker()
{
  /* attributes */
  this.mSelectedFilter = 0;
  this.mDefaultString = "";
  this.mDisplayDirectory = lastDirectory;
  this.mFilterTitles = new Array();
  this.mFilters = new Array();
}

nsFilePicker.prototype = {

  /* attribute nsILocalFile displayDirectory; */
  set displayDirectory(a) { this.mDisplayDirectory = a; },
  get displayDirectory()  { return this.mDisplayDirectory; },

  /* readonly attribute nsILocalFile file; */
  set file(a) { throw "readonly property"; },
  get file()  { debug("getter called " + this.mFile); return this.mFile; },

  /* attribute wstring defaultString; */
  set defaultString(a) { throw "readonly property"; },
  get defaultString()  { return this.mSelectedFilter; },

  /* methods */
  init: function(parent, title, mode) {
    this.mParentWindow = parent;
    this.mTitle = title;
    this.mMode = mode;
  },

  setFilters: function(filterMask) {
    dump(filterMask + "\n");
    if (filterMask & nsIFilePicker.filterAll) {
      dump("filterAll\n");
      this.mFilterTitles.push(bundle.GetStringFromName("allTitle"));
      this.mFilters.push(bundle.GetStringFromName("allFilter"));
    }
    if (filterMask & nsIFilePicker.filterHTML) {
      this.mFilterTitles.push(bundle.GetStringFromName("htmlTitle"));
      this.mFilters.push(bundle.GetStringFromName("htmlFilter"));
    }
    if (filterMask & nsIFilePicker.filterText) {
      this.mFilterTitles.push(bundle.GetStringFromName("textTitle"));
      this.mFilters.push(bundle.GetStringFromName("textFilter"));
    }
    if (filterMask & nsIFilePicker.filterImages) {
      this.mFilterTitles.push(bundle.GetStringFromName("imageTitle"));
      this.mFilters.push(bundle.GetStringFromName("imageFilter"));
    }
    if (filterMask & nsIFilePicker.filterXML) {
      this.mFilterTitles.push(bundle.GetStringFromName("xmlTitle"));
      this.mFilters.push(bundle.GetStringFromName("xmlFilter"));
    }
    if (filterMask & nsIFilePicker.filterXUL) {
      this.mFilterTitles.push(bundle.GetStringFromName("xulTitle"));
      this.mFilters.push(bundle.GetStringFromName("xulFilter"));
    }
  },

  appendFilter: function(title, extentions) {
    this.mFilterTitles.push(title);
    this.mFilters.push(extentions);
  },

  show: function() {
    var o = new Object();
    o.title = this.mTitle;
    o.mode = this.mMode;
    o.displayDirectory = this.mDisplayDirectory;
    o.defaultString = this.mDefaultString;
    o.filters = new Object();
    o.filters.titles = this.mFilterTitles;
    o.filters.types = this.mFilters;
    o.retvals = new Object();

    this.mParentWindow.openDialog("chrome://global/content/filepicker.xul",
                                 "",
                                 "chrome,modal,resizeable=yes,dependent=yes",
                                 o);

    this.mFile = o.retvals.file;
    lastDirectory = o.retvals.directory;
    return o.retvals.buttonStatus;
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






/* crap from strres.js that I want to use for string bundles since I can't include another .js file.... */

var strBundleService = null;

function srGetAppLocale()
{
  var localeService = null;
  var applicationLocale = null;

  localeService = Components.classes["component://netscape/intl/nslocaleservice"].createInstance();
  if (!localeService) {
    dump("\n--** localeService createInstance 1 failed **--\n");
	return null;
  }

  localeService = localeService.QueryInterface(Components.interfaces.nsILocaleService);
  if (!localeService) {
    dump("\n--** localeService createInstance 2 failed **--\n");
	return null;
  }
  applicationLocale = localeService.GetApplicationLocale();
  if (!applicationLocale) {
    dump("\n--** localeService.GetApplicationLocale failed **--\n");
  }
  return applicationLocale;
}

function srGetStrBundleWithLocale(path, locale)
{
  var strBundle = null;

  strBundleService =
    Components.classes["component://netscape/intl/stringbundle"].createInstance(); 

  if (!strBundleService) {
    dump("\n--** strBundleService createInstance 1 failed **--\n");
	return null;
  }

  strBundleService = 
  	strBundleService.QueryInterface(Components.interfaces.nsIStringBundleService);

  if (!strBundleService) {
	dump("\n--** strBundleService createInstance 2 failed **--\n");
	return null;
  }	

  strBundle = strBundleService.CreateBundle(path, locale); 
  if (!strBundle) {
	dump("\n--** strBundle createInstance failed **--\n");
  }
  return strBundle;
}

function srGetStrBundle(path)
{
  var appLocale = srGetAppLocale();
  return srGetStrBundleWithLocale(path, appLocale);
}


function localeSwitching(winType, baseDirectory, providerName)
{
  dump("\n ** Enter localeSwitching() ** \n");
  dump("\n ** winType=" +  winType + " ** \n");
  dump("\n ** baseDirectory=" +  baseDirectory + " ** \n");
  dump("\n ** providerName=" +  providerName + " ** \n");

  //
  var rdf;
  if(document.rdf) {
    rdf = document.rdf;
    dump("\n ** rdf = document.rdf ** \n");
  }
  else if(Components) {
    var isupports = Components.classes['component://netscape/rdf/rdf-service'].getService();
    rdf = isupports.QueryInterface(Components.interfaces.nsIRDFService);
    dump("\n ** rdf = Components... ** \n");
  }
  else {
    dump("can't find nuthin: no document.rdf, no Components. \n");
  }
  //

  var ds = rdf.GetDataSource("rdf:chrome");

  // For M4 builds, use this line instead.
  // var ds = rdf.GetDataSource("resource:/chrome/registry.rdf");
  var srcURL = "chrome://";
  srcURL += winType + "/locale/";
  dump("\n** srcURL=" + srcURL + " **\n");
  var sourceNode = rdf.GetResource(srcURL);
  var baseArc = rdf.GetResource("http://chrome.mozilla.org/rdf#base");
  var nameArc = rdf.GetResource("http://chrome.mozilla.org/rdf#name");
                      
  // Get the old targets
  var oldBaseTarget = ds.GetTarget(sourceNode, baseArc, true);
  dump("\n** oldBaseTarget=" + oldBaseTarget + "**\n");
  var oldNameTarget = ds.GetTarget(sourceNode, nameArc, true);
  dump("\n** oldNameTarget=" + oldNameTarget + "**\n");

  // Get the new targets 
  // file:/u/tao/gila/mozilla-org/html/projects/intl/chrome/
  // da-DK
  if (baseDirectory == "") {
	baseDirectory =  "resource:/chrome/";
  }

  var finalBase = baseDirectory;
  if (baseDirectory != "") {
	finalBase += winType + "/locale/" + providerName + "/";
  }
  dump("\n** finalBase=" + finalBase + "**\n");

  var newBaseTarget = rdf.GetLiteral(finalBase);
  var newNameTarget = rdf.GetLiteral(providerName);
  
  // Unassert the old relationships
  if (baseDirectory != "") {
  	ds.Unassert(sourceNode, baseArc, oldBaseTarget);
  }

  ds.Unassert(sourceNode, nameArc, oldNameTarget);
  
  // Assert the new relationships (note that we want a reassert rather than
  // an unassert followed by an assert, once reassert is implemented)
  if (baseDirectory != "") {
  	ds.Assert(sourceNode, baseArc, newBaseTarget, true);
  }
  ds.Assert(sourceNode, nameArc, newNameTarget, true);
  
  // Flush the modified data source to disk
  // (Note: crashes in M4 builds, so don't use Flush() until fix checked in)
  ds.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource).Flush();

  // Open up a new window to see your new chrome, since changes aren't yet dynamically
  // applied to the current window

  // BrowserOpenWindow('chrome://addressbook/content');
  dump("\n ** Leave localeSwitching() ** \n");
}

function localeTo(baseDirectory, localeName)
{
  dump("\n ** Enter localeTo() ** \n");

  localeSwitching("addressbook", baseDirectory, localeName); 
  localeSwitching("bookmarks", baseDirectory, localeName); 
  localeSwitching("directory", baseDirectory, localeName); 
  localeSwitching("editor", baseDirectory, localeName); 
  localeSwitching("global", baseDirectory, localeName); 
  localeSwitching("history", baseDirectory, localeName); 
  localeSwitching("messenger", baseDirectory, localeName); 
  localeSwitching("messengercompose", baseDirectory, localeName); 
  localeSwitching("navigator", baseDirectory, localeName); 
  localeSwitching("pref", baseDirectory, localeName); 
  localeSwitching("profile", baseDirectory, localeName); 
  localeSwitching("regviewer", baseDirectory, localeName); 
  localeSwitching("related", baseDirectory, localeName); 
  localeSwitching("sidebar", baseDirectory, localeName); 
  localeSwitching("wallet", baseDirectory, localeName); 
  localeSwitching("xpinstall", baseDirectory, localeName); 
  
  dump("\n ** Leave localeTo() ** \n");
}
