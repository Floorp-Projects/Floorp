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
