
/**
 * nsPreferences - a wrapper around nsIPref. Provides built in
 *                 exception handling to make preferences access simpler.
 **/
var nsPreferences = {
  get mPrefService()
    {
      return nsJSComponentManager.getService("@mozilla.org/preferences;1", "nsIPref");
    },

  setBoolPref: function (aPrefName, aPrefValue)
    {
      try 
        {
          this.mPrefService.SetBoolPref(aPrefName, aPrefValue);
        }
      catch(e)
        {
        }
    },
  
  getBoolPref: function (aPrefName, aDefVal)
    {
      try
        {
          return this.mPrefService.GetBoolPref(aPrefName);
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
          this.mPrefService.SetUnicharPref(aPrefName, aPrefValue);
        }
      catch(e)
        {
        }
    },
  
  copyUnicharPref: function (aPrefName, aDefVal)
    {
      try
        {
          return this.mPrefService.CopyUnicharPref(aPrefName);
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
          this.mPrefService.SetIntPref(aPrefName, aPrefValue);
        }
      catch(e)
        {
        }
    },
  
  getIntPref: function (aPrefName, aDefVal)
    {
      try
        {
          return this.mPrefService.GetIntPref(aPrefName);
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
          return this.mPrefService.getLocalizedUnicharPref(aPrefName);
        }
      catch(e)
        {
          return aDefVal != undefined ? aDefVal : null;
        }
      return null;        // quiet warnings
    }
};

