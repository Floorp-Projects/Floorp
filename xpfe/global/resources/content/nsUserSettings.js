
/**
 * nsPreferences - a wrapper around nsIPref. Provides built in
 *                 exception handling to make preferences access simpler.
 **/
var nsPreferences = {
  get mPrefService()
    {
      return nsJSComponentManager.getService("component://netscape/preferences", "nsIPref");
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
    },
};