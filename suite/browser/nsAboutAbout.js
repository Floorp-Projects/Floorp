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
 * The Original Code is the SeaMonkey internet suite code.
 *
 * The Initial Developer of the Original Code is
 * Mark Banner <bugzilla@standard8.demon.co.uk>
 *
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

const nsISupports           = Components.interfaces.nsISupports;
const nsIAboutModule        = Components.interfaces.nsIAboutModule;
const nsIFactory            = Components.interfaces.nsIFactory;
const nsIIOService          = Components.interfaces.nsIIOService;
const nsIComponentRegistrar = Components.interfaces.nsIComponentRegistrar;

const ABOUTABOUT_URI = "chrome://navigator/content/aboutAbout.html";

var nsAboutAbout = {
  /* nsISupports */
  QueryInterface: function QueryInterface(iid) {
    if (iid.equals(nsISupports) ||
        iid.equals(nsIAboutModule) ||
        iid.equals(nsIFactory))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  /* nsIAboutModule */
  newChannel: function newChannel(aURI) {
    var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(nsIIOService);

    var channel = ioService.newChannel(ABOUTABOUT_URI, null, null);

    channel.originalURI = aURI;

    return channel;
  },

  getURIFlags: function getURIFlags(aURI) {
    return 0;
  },

  /* nsIFactory */
  createInstance: function createInstance(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;

    return this.QueryInterface(iid);
  },

  lockFactory: function lockFactory(lock) {
    /* no-op */
  }
};

const NS_ABOUT_MODULE_CONTRACTID = "@mozilla.org/network/protocol/about;1";
const NS_ABOUT_MODULE_CONTRACTID_PREFIX = NS_ABOUT_MODULE_CONTRACTID + "?what=";
const ABOUTABOUT_CONTRACTID = NS_ABOUT_MODULE_CONTRACTID_PREFIX + "about";

const ABOUTABOUT_CID = Components.ID("{2a57d8af-8728-4e63-b01e-29f4a4ebf9a7}");

var Module = {
  /* nsISupports */
  QueryInterface: function QueryInterface(iid) {
    if (iid.equals(Components.interfaces.nsIModule) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  /* nsIModule */
  getClassObject: function getClassObject(compMgr, cid, iid) {
    if (cid.equals(ABOUTABOUT_CID))
      return nsAboutAbout.QueryInterface(iid);

    throw Components.results.NS_ERROR_FACTORY_NOT_REGISTERED;
  },
    
  registerSelf: function registerSelf(compMgr, fileSpec, location, type) {
    var compReg = compMgr.QueryInterface(nsIComponentRegistrar);

    compReg.registerFactoryLocation(ABOUTABOUT_CID,
                                    "about:about",
                                    ABOUTABOUT_CONTRACTID,
                                    fileSpec,
                                    location,
                                    type);
  },
    
  unregisterSelf: function unregisterSelf(compMgr, location, type) {
    var compReg = compMgr.QueryInterface(nsIComponentRegistrar);
    compReg.unregisterFactoryLocation(ABOUTABOUT_CID, location);
  },

  canUnload: function canUnload(compMgr) {
    return true;
  }
};

// NSGetModule: Return the nsIModule object.
function NSGetModule(compMgr, fileSpec) {
  return Module;
}
