/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Extension Manager.
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

///////////////////////////////////////////////////////////////////////////////
//
// nsExtensionManager
//
function nsExtensionManager()
{
}

nsExtensionManager.prototype = {
  /////////////////////////////////////////////////////////////////////////////  
  // nsIExtensionManager
  installExtension: function (aXPIFile, aFlags)
  {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },
  
  installTheme: function (aJARFile, aFlags)
  {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // nsIClassInfo
  getInterfaces: function (aCount)
  {
    var interfaces = [Components.interfaces.nsIExtensionManager];
    aCount.value = interfaces.length;
    return interfaces;
  },
  
  getHelperForLanguage: function (aLanguage)
  {
    return null;
  },
  
  get contractID() 
  {
    return "@mozilla.org/extensions/manager;1";
  },
  
  get classDescription()
  {
    return "Extension Manager";
  },
  
  get classID() 
  {
    return Components.ID("{8A115FAA-7DCB-4e8f-979B-5F53472F51CF}");
  },
  
  get implementationLanguage()
  {
    return Components.interfaces.nsIProgrammingLanguage.JAVASCRIPT;
  },
  
  get flags()
  {
    return Components.interfaces.nsIClassInfo.SINGLETON;
  },

  /////////////////////////////////////////////////////////////////////////////
  // nsISupports
  QueryInterface: function (aIID) 
  {
    if (!aIID.equals(Components.interfaces.nsIExtensionManager))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

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
    manager: { CID        : nsExtensionManager.prototype.classID,
               contractID : nsExtensionManager.prototype.contractID,
               className  : nsExtensionManager.prototype.classDescription,
               factory    : {
                              createInstance: function (aOuter, aIID) 
                              {
                                if (aOuter != null)
                                  throw Components.results.NS_ERROR_NO_AGGREGATION;
                                
                                return (new nsExtensionManager()).QueryInterface(aIID);
                              }
                            }
             }
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
