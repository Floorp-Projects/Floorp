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
    var chromeRegistry = Components.classes["@mozilla.org/chrome/chrome-registry;1"].getService();
    if ( chromeRegistry ) {
      chromeRegistry = chromeRegistry.QueryInterface( Components.interfaces.nsIChromeRegistry );
    }
    var node = event.target;
    var langcode = node.getAttribute('value');
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

