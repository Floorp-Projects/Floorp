var strBundleService = null;
var localeService = null;

function srGetAppLocale()
{
  var applicationLocale = null;

  if (!localeService) {
      try {
          localeService = Components.classes["@mozilla.org/intl/nslocaleservice;1"].getService();
      
          localeService = localeService.QueryInterface(Components.interfaces.nsILocaleService);
      } catch (ex) {
          dump("\n--** localeService failed: " + ex + "\n");
          return null;
      }
  }
  
  applicationLocale = localeService.GetApplicationLocale();
  if (!applicationLocale) {
    dump("\n--** localeService.GetApplicationLocale failed **--\n");
  }
  return applicationLocale;
}

function srGetStrBundleWithLocale(path, locale)
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

  strBundle = strBundleService.CreateBundle(path, locale); 
  if (!strBundle) {
	dump("\n--** strBundle createInstance failed **--\n");
  }
  return strBundle;
}

function srGetStrBundle(path)
{
  var appLocale = srGetAppLocale();
  return srGetStrBundleWithLocale(path, appLocale);
}

function selectLocale(event)
{
  try {
    var chromeRegistry = Components.classes["@mozilla.org/chrome/chrome-registry;1"].getService();
    if ( chromeRegistry ) {
      chromeRegistry = chromeRegistry.QueryInterface( Components.interfaces.nsIChromeRegistry );
    }
    var node = event.target;
    var langcode = node.getAttribute('data');
    //var old_lang = chromeRegistry.getSelectedLocale("navigator");
    //dump("\n-->old_lang=" + old_lang + "--");
    chromeRegistry.selectLocale(langcode, true);
	dump("\n-->set new lang, langcode=" + langcode + "--");
    var sbundle = srGetStrBundle("chrome://global/locale/brand.properties");
	var alertstr = sbundle.GetStringFromName("langAlert");
	alert(alertstr);
  }
  catch(e) {
    dump("\n--> strres.js: selectLocale() failed!\n");
    return false;
  }
  return true;	
}

