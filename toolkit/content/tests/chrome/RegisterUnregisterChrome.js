/* This code is mostly copied from chrome/test/unit/head_crtestutils.js */

const NS_CHROME_MANIFESTS_FILE_LIST = "ChromeML";
const XUL_CACHE_PREF = "nglayout.debug.disable_xul_cache";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

let gChromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].
                    getService(Ci.nsIXULChromeRegistry);
let gPrefs     = Cc["@mozilla.org/preferences-service;1"].
                    getService(Ci.nsIPrefBranch);

function refreshChrome()
{
  if (gHasChrome)
    gChromeReg.checkForNewChrome();
  if (gHasSkins)
    gChromeReg.refreshSkins();
}

function registerCustomChrome(chromedir, hasChrome, hasSkins)
{
  gHasChrome = hasChrome;
  gHasSkins = hasSkins;

  // Disable XUL cache temporarily
  gPrefs.setBoolPref(XUL_CACHE_PREF, true);

  let uri = Cc["@mozilla.org/network/io-service;1"].
               getService(Ci.nsIIOService).newURI(chromedir, null, null);
  uri = gChromeReg.convertChromeURL(uri);
  let newChromePath = uri.QueryInterface(Ci.nsIFileURL).file;
  Components.manager.QueryInterface(Ci.nsIComponentRegistrar).
    autoRegister(newChromePath);
  refreshChrome();
  return uri;
}

function cleanupCustomChrome()
{
  refreshChrome();
  gPrefs.clearUserPref(XUL_CACHE_PREF);
}
