var strBundleService = null;

function srGetStrBundle(path)
{
  var strBundle = null;

  if (!strBundleService) {
      try {
          strBundleService =
              Components.classes["@mozilla.org/intl/stringbundle;1"].getService(); 
          strBundleService = 
              strBundleService.QueryInterface(Components.interfaces.nsIStringBundleService);
      } catch (ex) {
          dump("\n--** strBundleService failed: " + ex + "\n");
          return null;
      }
  }

  strBundle = strBundleService.createBundle(path); 
  if (!strBundle) {
	dump("\n--** strBundle createInstance failed **--\n");
  }
  return strBundle;
}

function selectLocale(event)
{
  try {
    var node = event.target;
    var langcode = node.getAttribute('value');
    if (langcode)
    {
      var chromeRegistry = Components.classes["@mozilla.org/chrome/chrome-registry;1"].getService(Components.interfaces.nsIChromeRegistry);
      chromeRegistry.selectLocale(langcode, true);
      var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
      observerService.notifyObservers(null, "locale-selected", null);
      var prefUtilBundle = srGetStrBundle("chrome://communicator/locale/pref/prefutilities.properties");
      var brandBundle = srGetStrBundle("chrome://global/locale/brand.properties");
      var alertText = prefUtilBundle.GetStringFromName("languageAlert");
      var titleText = prefUtilBundle.GetStringFromName("languageTitle");
      alertText = alertText.replace(/%brand%/g, brandBundle.GetStringFromName("brandShortName"));
      var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
      promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService)
      promptService.alert(window, titleText, alertText);
    }
  }
  catch(e) {
    return false;
  }
  return true;	
}

