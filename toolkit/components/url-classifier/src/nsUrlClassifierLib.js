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

// We wastefully reload the same JS files across components.  This puts all
// the common JS files used by safebrowsing and url-classifier into a
// single component.

const Cc = Components.classes;
const Ci = Components.interfaces;
const G_GDEBUG = false;

// TODO: get rid of application.js and filesystem.js (not used much)
#include ../content/js/arc4.js
#include ../content/js/lang.js

#include ../content/moz/preferences.js
#include ../content/moz/filesystem.js
#include ../content/moz/debug.js
#include ../content/moz/alarm.js
#include ../content/moz/base64.js
#include ../content/moz/cryptohasher.js
#include ../content/moz/lang.js
#include ../content/moz/objectsafemap.js
#include ../content/moz/observer.js
#include ../content/moz/protocol4.js

#include ../content/application.js
#include ../content/url-crypto.js
#include ../content/url-crypto-key-manager.js
#include ../content/xml-fetcher.js

// Expose this whole component.
var lib = this;

function UrlClassifierLib() {
  this.wrappedJSObject = lib;
}

// Module object
function UrlClassifierLibMod() {
  this.firstTime = true;
  this.cid = Components.ID("{26a4a019-2827-4a89-a85c-5931a678823a}");
  this.progid = "@mozilla.org/url-classifier/jslib;1";
}

UrlClassifierLibMod.prototype.registerSelf = function(compMgr, fileSpec, loc, type) {
  if (this.firstTime) {
    this.firstTime = false;
    throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
  }
  compMgr = compMgr.QueryInterface(Ci.nsIComponentRegistrar);
  compMgr.registerFactoryLocation(this.cid,
                                  "UrlClassifier JS Lib",
                                  this.progid,
                                  fileSpec,
                                  loc,
                                  type);
};

UrlClassifierLibMod.prototype.getClassObject = function(compMgr, cid, iid) {  
  if (!cid.equals(this.cid))
    throw Components.results.NS_ERROR_NO_INTERFACE;
  if (!iid.equals(Ci.nsIFactory))
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

  return this.factory;
}

UrlClassifierLibMod.prototype.canUnload = function(compMgr) {
  return true;
}

UrlClassifierLibMod.prototype.factory = {
  createInstance: function(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return new UrlClassifierLib();
  }
};

var LibModInst = new UrlClassifierLibMod();

function NSGetModule(compMgr, fileSpec) {
  return LibModInst;
}
