
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
          var str = Components.classes["@mozilla.org/supports-wstring;1"]
                              .createInstance(Components.interfaces.nsISupportsWString);
          str.data = aPrefValue;
          this.mPrefService.setComplexValue(aPrefName,
                                            Components.interfaces.nsISupportsWString, str);
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
                                                   Components.interfaces.nsISupportsWString);
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
                                                   Components.interfaces.nsIPrefLocalizedString);
        }
      catch(e)
        {
          return aDefVal != undefined ? aDefVal : null;
        }
      return null;        // quiet warnings
    }
};

