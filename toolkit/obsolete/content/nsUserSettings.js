/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 * nsPreferences - a wrapper around nsIPrefService. Provides built in
 *                 exception handling to make preferences access simpler.
 **/
var nsPreferences = {
  get mPrefService()
    {
      return Components.classes["@mozilla.org/preferences-service;1"]
                       .getService(Components.interfaces.nsIPrefBranch);
    },

  setBoolPref: function (aPrefName, aPrefValue)
    {
      try 
        {
          this.mPrefService.setBoolPref(aPrefName, aPrefValue);
        }
      catch(e)
        {
        }
    },
  
  getBoolPref: function (aPrefName, aDefVal)
    {
      try
        {
          return this.mPrefService.getBoolPref(aPrefName);
        }
      catch(e)
        {
          return aDefVal != undefined ? aDefVal : null;
        }
      return null;        // quiet warnings
    },
  
  setUnicharPref: function (aPrefName, aPrefValue)
    {
      try
        {
          var str = Components.classes["@mozilla.org/supports-string;1"]
                              .createInstance(Components.interfaces.nsISupportsString);
          str.data = aPrefValue;
          this.mPrefService.setComplexValue(aPrefName,
                                            Components.interfaces.nsISupportsString, str);
        }
      catch(e)
        {
        }
    },
  
  copyUnicharPref: function (aPrefName, aDefVal)
    {
      try
        {
          return this.mPrefService.getComplexValue(aPrefName,
                                                   Components.interfaces.nsISupportsString).data;
        }
      catch(e)
        {
          return aDefVal != undefined ? aDefVal : null;
        }
      return null;        // quiet warnings
    },
    
  setIntPref: function (aPrefName, aPrefValue)
    {
      try
        {
          this.mPrefService.setIntPref(aPrefName, aPrefValue);
        }
      catch(e)
        {
        }
    },
  
  getIntPref: function (aPrefName, aDefVal)
    {
      try
        {
          return this.mPrefService.getIntPref(aPrefName);
        }
      catch(e)
        {
          return aDefVal != undefined ? aDefVal : null;
        }
      return null;        // quiet warnings
    },

  getLocalizedUnicharPref: function (aPrefName, aDefVal)
    {
      try
        {
          return this.mPrefService.getComplexValue(aPrefName,
                                                   Components.interfaces.nsIPrefLocalizedString).data;
        }
      catch(e)
        {
          return aDefVal != undefined ? aDefVal : null;
        }
      return null;        // quiet warnings
    }
};

