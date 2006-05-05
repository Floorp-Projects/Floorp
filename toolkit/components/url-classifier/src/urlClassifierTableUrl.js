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
 * The Original Code is Url Classifier code
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tony Chang <tony@ponderer.org>
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

// TODO: Combine this file with other urlClassifierTable files and rename to
// nsUrlClassifierTable.js

const Cc = Components.classes;
const Ci = Components.interfaces;
const G_GDEBUG = false;

// Use subscript loader to load files.  The files in ../content get mapped
// to chrome://global/content/url-classifier/.  Order matters if one file depends
// on another file during initialization.
const LIB_FILES = [
  "chrome://global/content/url-classifier/js/lang.js",

  "chrome://global/content/url-classifier/moz/preferences.js",
  "chrome://global/content/url-classifier/moz/filesystem.js",
  "chrome://global/content/url-classifier/moz/debug.js", // req js/lang.js moz/prefs.js moz/filesystem.js

  "chrome://global/content/url-classifier/url-canonicalizer.js"
];

for (var i = 0, libFile; libFile = LIB_FILES[i]; ++i) {
  //dump('*** loading subscript ' + libFile + '\n');
  Cc["@mozilla.org/moz/jssubscript-loader;1"]
    .getService(Ci.mozIJSSubScriptLoader)
    .loadSubScript(libFile);
}

function UrlClassifierTableUrl() {
  this.debugZone = "trtable-url";
  this.dbservice_ = Cc["@mozilla.org/url-classifier/dbservice;1"]
                    .getService(Ci.nsIUrlClassifierDBService);
  this.name = '';
  this.needsUpdate = false;
}

UrlClassifierTableUrl.prototype.QueryInterface = function(iid) {
  if (iid.equals(Components.interfaces.nsISupports) ||
      iid.equals(Components.interfaces.nsIUrlClassifierTable))
    return this;                                              
  Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
  return null;
}

/**
 * Look up a URL in a URL table
 */
UrlClassifierTableUrl.prototype.exists = function(url, callback) {
  var canonicalized = PROT_URLCanonicalizer.canonicalizeURL_(url);
  G_Debug(this, "Looking up: " + url + " (" + canonicalized + ")");

  function dbCallback(v) {
    G_Debug("dbCallback", "Looked up " + url + ": " + v + "|");
    callback.handleEvent(v.length > 0);
  }
  this.dbservice_.exists(this.name, url, dbCallback);
}


// Module object
function UrlClassifierTableUrlMod() {
  this.firstTime = true;
  this.cid = Components.ID("{43399ee0-da0b-46a8-9541-08721265981c}");
  this.progid = "@mozilla.org/url-classifier/table;1?type=url";
}

UrlClassifierTableUrlMod.prototype.registerSelf = function(compMgr, fileSpec, loc, type) {
  if (this.firstTime) {
    this.firstTime = false;
    throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
  }
  compMgr = compMgr.QueryInterface(Ci.nsIComponentRegistrar);
  compMgr.registerFactoryLocation(this.cid,
                                  "UrlClassifier Table Url Module",
                                  this.progid,
                                  fileSpec,
                                  loc,
                                  type);
};

UrlClassifierTableUrlMod.prototype.getClassObject = function(compMgr, cid, iid) {  
  if (!cid.equals(this.cid))
    throw Components.results.NS_ERROR_NO_INTERFACE;
  if (!iid.equals(Ci.nsIFactory))
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

  return this.factory;
}

UrlClassifierTableUrlMod.prototype.canUnload = function(compMgr) {
  return true;
}

UrlClassifierTableUrlMod.prototype.factory = {
  createInstance: function(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return (new UrlClassifierTableUrl()).QueryInterface(iid);
  }
};

var UrlModInst = new UrlClassifierTableUrlMod();

function NSGetModule(compMgr, fileSpec) {
  return UrlModInst;
}
