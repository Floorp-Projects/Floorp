/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Update Service.
 *
 * The Initial Developer of the Original Code is Ben Goodger.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Ben Goodger <ben@bengoodger.com>
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

const PREF_UPDATE_APP_ENABLED           = "update.app.enabled";
const PREF_UPDATE_APP_URI               = "update.app.uri";
const PREF_UPDATE_EXTENSIONS_ENABLED    = "update.extensions.enabled";
const PREF_UPDATE_EXTENSIONS_AUTOUPDATE = "update.extensions.autoUpdate";
const PREF_UPDATE_INTERVAL              = "update.interval";
const PREF_UPDATE_LASTUPDATEDATE        = "update.lastUpdateDate";
const PREF_UPDATE_UPDATESAVAILABLE      = "update.updatesAvailable";
const PREF_UPDATE_SEVERITY              = "update.severity";
const PREF_UPDATE_COUNT                 = "update.count";

function nsBackgroundUpdateService()
{
}

nsBackgroundUpdateService.prototype = {
  _timer: null,

  /////////////////////////////////////////////////////////////////////////////
  // nsIBackgroundUpdateService
  watchForUpdates: function ()
  {
    // This is called when the app starts, so check to see if the time interval
    // expired between now and the last time an automated update was performed.
    // now is the same one that was started last time. 
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    var appUpdatesEnabled = pref.getBoolPref(PREF_UPDATE_APP_ENABLED);
    var extUpdatesEnabled = pref.getBoolPref(PREF_UPDATE_EXTENSIONS_ENABLED);
    if (!appUpdatesEnabled && !extUpdatesEnabled)
      return;
      
    var interval = pref.getIntPref(PREF_UPDATE_INTERVAL);
    var lastUpdateTime = pref.getIntPref(PREF_LASTUPDATEDATE);
    var timeSinceLastCheck = Date.UTC() - lastUpdateTime;
    if (timeSinceLastCheck > interval)
      this._checkNow();
    else
      this._makeTimer(interval - timeSinceLastCheck);
  },

  /////////////////////////////////////////////////////////////////////////////
  // nsITimerCallback
  notify: function (aTimer)
  {
    this._checkNow();
  },

  /////////////////////////////////////////////////////////////////////////////
  // nsBackgroundUpdateService
  _makeTimer: function (aDelay)
  {
    if (this._timer) 
      this._timer.cancel();
      
    this._timer = Components.classes["@mozilla.org/timer;1"]
                            .createInstance(Components.interfaces.nsITimer);
    timer.initWithCallback(this, aDelay, 
                           Components.interfaces.nsITimer.TYPE_ONE_SHOT);
  },

  _checkNow: function ()
  {
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    var appUpdatesEnabled = pref.getBoolPref(PREF_UPDATE_APP_ENABLED);
    var extUpdatesEnabled = pref.getBoolPref(PREF_UPDATE_EXTENSIONS_ENABLED);

    if (appUpdatesEnabled) {
      var dsURI = pref.getComplexValue(PREF_UPDATE_APP_URI, 
                                       Components.interfaces.nsIPrefLocalizedString).data;
      
      var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                          .getService(Components.interfaces.nsIRDFService);
      var ds = rdf.GetDataSource(dsURI);
      ds = ds.QueryInterface(Components.interfaces.nsIRDFXMLSink);
      ds.addXMLSinkObserver(new nsAppUpdateXMLRDFDSObserver());
    }
    if (extUpdatesEnabled) {
      var em = Components.classes["@mozilla.org/extensions/manager;1"]
                         .getService(Components.interfaces.nsIExtensionManager);
      em.update([], 0);      
    }                             
                         
    this._makeTimer(pref.getIntPref(PREF_UPDATE_INTERVAL));
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // nsISupports
  QueryInterface: function (aIID) 
  {
    if (!aIID.equals(Components.interfaces.nsIBackgroundUpdateService) &&
        !aIID.equals(Components.interfaces.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

function nsAppUpdateXMLRDFDSObserver()
{
}

nsAppUpdateXMLRDFDSObserver.prototype = 
{ 
  /////////////////////////////////////////////////////////////////////////////
  // nsIRDFXMLSinkObserver
  onBeginLoad: function(aSink)
  {
  },

  onInterrupt: function(aSink)
  {
  },

  onResume: function(aSink)
  {
  },

  onEndLoad: function(aSink)
  {
    aSink.removeXMLSinkObserver(this);

    var ds = aSink.QueryInterface(Components.interfaces.nsIRDFDataSource);
    
    // do update checking here, parsing something like this format:
/*

<?xml version="1.0"?>
<RDF:RDF xmlns:RDF="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
         xmlns:NC="http://home.netscape.com/NC-rdf#">

  <RDF:Description about="urn:updates:latest">
    <NC:registryName>Browser</NC:registryName>
    <NC:version>1.0.0.0</NC:version>
    <NC:URL>http://home.netscape.com/computing/download/index.html</NC:URL>
    <NC:productName>Mozilla</NC:productName>
  </RDF:Description>

</RDF:RDF>

*/
  },

  onError: function(aSink, aStatus, aErrorMsg)
  {
    aSink.removeXMLSinkObserver(this);
  }
}

var gBackgroundUpdateService = null;

var gModule = {
  _firstTime: true,
  
  registerSelf: function (aComponentManager, aFileSpec, aLocation, aType) 
  {
    if (this._firstTime) {
      this._firstTime = false;
      throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
    }
    aComponentManager = aComponentManager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
    
    for (var key in this._objects) {
      var obj = this._objects[key];
      aComponentManager.registerFactoryLocation(obj.CID, obj.className, obj.contractID,
                                                aFileSpec, aLocation, aType);
    }
  },
  
  getClassObject: function (aComponentManager, aCID, aIID) 
  {
    if (!aIID.equals(Components.interfaces.nsIFactory))
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

    for (var key in this._objects) {
      if (aCID.equals(this._objects[key].CID))
        return this._objects[key].factory;
    }
    
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  
  _objects: {
    manager: { CID: Components.ID("{8A115FAA-7DCB-4e8f-979B-5F53472F51CF}"),
               contractID: "@mozilla.org/updates/background-update-service;1",
               className: "Background Update Service",
               factory: {
                          createInstance: function (aOuter, aIID) 
                          {
                            if (aOuter != null)
                              throw Components.results.NS_ERROR_NO_AGGREGATION;
                            
                            if (!gUpdateService)
                              gUpdateService = new nsBackgroundUpdateService();
                              
                            return gUpdateService.QueryInterface(aIID);
                          }
                        }
             },
   },
  
  canUnload: function (aComponentManager) 
  {
    return true;
  }
};

function NSGetModule(compMgr, fileSpec) 
{
  return gModule;
}
