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

// TODO: Rename this file to nsUrlClassifierListManager.js

const Cc = Components.classes;
const Ci = Components.interfaces;
const G_GDEBUG = false;

// Use subscript loader to load files.  The files in ../content get mapped
// to chrome://global/content/url-classifier/.  Order matters if one file depends
// on another file during initialization.
const LIB_FILES = [
  "chrome://global/content/url-classifier/js/arc4.js",
  "chrome://global/content/url-classifier/js/lang.js",
  "chrome://global/content/url-classifier/js/thread-queue.js",

  "chrome://global/content/url-classifier/moz/preferences.js",
  "chrome://global/content/url-classifier/moz/filesystem.js",
  "chrome://global/content/url-classifier/moz/debug.js", // dep js/lang.js moz/prefs.js moz/filesystem.js
  "chrome://global/content/url-classifier/moz/alarm.js",
  "chrome://global/content/url-classifier/moz/base64.js",
  "chrome://global/content/url-classifier/moz/cryptohasher.js",
  "chrome://global/content/url-classifier/moz/lang.js",
  "chrome://global/content/url-classifier/moz/observer.js",
  "chrome://global/content/url-classifier/moz/protocol4.js",

  "chrome://global/content/url-classifier/application.js",
  "chrome://global/content/url-classifier/listmanager.js",
  "chrome://global/content/url-classifier/url-crypto.js",
  "chrome://global/content/url-classifier/url-crypto-key-manager.js", // dep url-crypto.js
  "chrome://global/content/url-classifier/wireformat.js",
  "chrome://global/content/url-classifier/xml-fetcher.js",
];

for (var i = 0, libFile; libFile = LIB_FILES[i]; ++i) {
  //dump('*** loading subscript ' + libFile + '\n');
  Cc["@mozilla.org/moz/jssubscript-loader;1"]
    .getService(Ci.mozIJSSubScriptLoader)
    .loadSubScript(libFile);
}

// Module object
function UrlClassifierListManagerMod() {
  this.firstTime = true;
  this.cid = Components.ID("{ca168834-cc00-48f9-b83c-fd018e58cae3}");
  this.progid = "@mozilla.org/url-classifier/listmanager;1";
}

UrlClassifierListManagerMod.prototype.registerSelf = function(compMgr, fileSpec, loc, type) {
  if (this.firstTime) {
    this.firstTime = false;
    throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
  }
  compMgr = compMgr.QueryInterface(Ci.nsIComponentRegistrar);
  compMgr.registerFactoryLocation(this.cid,
                                  "UrlClassifier List Manager Module",
                                  this.progid,
                                  fileSpec,
                                  loc,
                                  type);
};

UrlClassifierListManagerMod.prototype.getClassObject = function(compMgr, cid, iid) {  
  if (!cid.equals(this.cid))
    throw Components.results.NS_ERROR_NO_INTERFACE;
  if (!iid.equals(Ci.nsIFactory))
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

  return this.factory;
}

UrlClassifierListManagerMod.prototype.canUnload = function(compMgr) {
  return true;
}

UrlClassifierListManagerMod.prototype.factory = {
  createInstance: function(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return (new PROT_ListManager()).QueryInterface(iid);
  }
};

var ListManagerModInst = new UrlClassifierListManagerMod();

function NSGetModule(compMgr, fileSpec) {
  return ListManagerModInst;
}
