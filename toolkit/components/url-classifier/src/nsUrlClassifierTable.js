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

const Cc = Components.classes;
const Ci = Components.interfaces;
const G_GDEBUG = false;

// Use subscript loader to load files.  The files in ../content get mapped
// to chrome://global/content/url-classifier/.  Order matters if one file depends
// on another file during initialization.
const LIB_FILES = [
  "chrome://global/content/url-classifier/js/arc4.js",
  "chrome://global/content/url-classifier/js/lang.js",

  "chrome://global/content/url-classifier/moz/preferences.js",
  "chrome://global/content/url-classifier/moz/filesystem.js",
  "chrome://global/content/url-classifier/moz/debug.js", // req js/lang.js moz/prefs.js moz/filesystem.js
  "chrome://global/content/url-classifier/moz/lang.js",

  "chrome://global/content/url-classifier/moz/base64.js",
  "chrome://global/content/url-classifier/moz/cryptohasher.js",
  "chrome://global/content/url-classifier/enchash-decrypter.js",
  "chrome://global/content/url-classifier/multi-querier.js",
  "chrome://global/content/url-classifier/url-canonicalizer.js",
  "chrome://global/content/url-classifier/trtable.js"
];

for (var i = 0, libFile; libFile = LIB_FILES[i]; ++i) {
  //dump('*** loading subscript ' + libFile + '\n');
  Cc["@mozilla.org/moz/jssubscript-loader;1"]
    .getService(Ci.mozIJSSubScriptLoader)
    .loadSubScript(libFile);
}

function UrlClassifierTableMod() {
  this.components = {};
  this.addComponent({
      cid: "{43399ee0-da0b-46a8-9541-08721265981c}",
      name: "UrlClassifier Table Url Module",
      progid: "@mozilla.org/url-classifier/table;1?type=url",
      factory: new UrlClassifierTableFactory(UrlClassifierTableUrl)
    });
  this.addComponent({
      cid: "{3b5004c6-3fcd-4b12-b311-a4dfbeaf27aa}",
      name: "UrlClassifier Table Domain Module",
      progid: "@mozilla.org/url-classifier/table;1?type=domain",
      factory: new UrlClassifierTableFactory(UrlClassifierTableDomain)
    });
  this.addComponent({
      cid: "{04f15d1d-2db8-4b8e-91d7-82f30308b434}",
      name: "UrlClassifier Table Enchash Module",
      progid: "@mozilla.org/url-classifier/table;1?type=enchash",
      factory: new UrlClassifierTableFactory(UrlClassifierTableEnchash)
    });
}

UrlClassifierTableMod.prototype.addComponent = function(comp) {
  this.components[comp.cid] = comp;
};

UrlClassifierTableMod.prototype.registerSelf = function(compMgr, fileSpec, loc, type) {
  compMgr = compMgr.QueryInterface(Ci.nsIComponentRegistrar);
  // Register all the components
  for (var cid in this.components) {
    var comp = this.components[cid];
    compMgr.registerFactoryLocation(Components.ID(comp.cid),
                                    comp.name,
                                    comp.progid,
                                    fileSpec,
                                    loc,
                                    type);
  }
};

UrlClassifierTableMod.prototype.getClassObject = function(compMgr, cid, iid) {
  var comp = this.components[cid.toString()];

  if (!comp)
    throw Components.results.NS_ERROR_NO_INTERFACE;
  if (!iid.equals(Ci.nsIFactory))
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

  return comp.factory;
};

UrlClassifierTableMod.prototype.canUnload = function(compMgr) {
  return true;
};

/**
 * Create a factory.
 * @param ctor Function constructor for the object we're creating.
 */
function UrlClassifierTableFactory(ctor) {
  this.ctor = ctor;
}

UrlClassifierTableFactory.prototype.createInstance = function(outer, iid) {
  if (outer != null)
    throw Components.results.NS_ERROR_NO_AGGREGATION;
  return (new this.ctor()).QueryInterface(iid);
};

var modInst = new UrlClassifierTableMod();

function NSGetModule(compMgr, fileSpec) {
  return modInst;
}
