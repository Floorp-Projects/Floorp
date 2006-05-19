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

#include ../content/listmanager.js
#include ../content/wireformat.js

var modScope = this;
function Init() {
  // Pull the library in.
  var jslib = Cc["@mozilla.org/url-classifier/jslib;1"]
              .getService().wrappedJSObject;
  Function.prototype.inherits = jslib.Function.prototype.inherits;
  modScope.G_Preferences = jslib.G_Preferences;
  modScope.G_PreferenceObserver = jslib.G_PreferenceObserver;
  modScope.G_ObserverServiceObserver = jslib.G_ObserverServiceObserver;
  modScope.G_Debug = jslib.G_Debug;
  modScope.G_Assert = jslib.G_Assert;
  modScope.G_debugService = jslib.G_debugService;
  modScope.G_Alarm = jslib.G_Alarm;
  modScope.BindToObject = jslib.BindToObject;
  modScope.PROT_XMLFetcher = jslib.PROT_XMLFetcher;

  // We only need to call Init once.
  modScope.Init = function() {};
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
    Init();
    return (new PROT_ListManager()).QueryInterface(iid);
  }
};

var ListManagerModInst = new UrlClassifierListManagerMod();

function NSGetModule(compMgr, fileSpec) {
  return ListManagerModInst;
}
