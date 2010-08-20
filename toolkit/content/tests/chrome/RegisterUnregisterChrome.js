/* This code is mostly copied from chrome/test/unit/head_crtestutils.js */

const NS_CHROME_MANIFESTS_FILE_LIST = "ChromeML";
const XUL_CACHE_PREF = "nglayout.debug.disable_xul_cache";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

let gDirSvc    = Cc["@mozilla.org/file/directory_service;1"].
  getService(Ci.nsIDirectoryService).QueryInterface(Ci.nsIProperties);
let gChromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].
                    getService(Ci.nsIXULChromeRegistry);
let gPrefs     = Cc["@mozilla.org/preferences-service;1"].
                    getService(Ci.nsIPrefBranch);

// Create the temporary file in the profile, instead of in TmpD, because
// we know the mochitest harness kills off the profile when it's done.
function copyToTemporaryFile(f)
{
  let tmpd = gDirSvc.get("ProfD", Ci.nsIFile);
  tmpf = tmpd.clone();
  tmpf.append("temp.manifest");
  tmpf.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0600);
  tmpf.remove(false);

  f.copyTo(tmpd, tmpf.leafName);
  return tmpf;
}

function convertChromeURI(chromeURI)
{
  let uri = Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService).newURI(chromeURI, null, null);
  return gChromeReg.convertChromeURL(uri);
}

function chromeURIToFile(chromeURI)
{
  return convertChromeURI(chromeURI).
    QueryInterface(Ci.nsIFileURL).file;
}  

// Register a chrome manifest temporarily and return a function which un-does
// the registrarion when no longer needed.
function registerManifestTemporarily(manifestURI)
{
  gPrefs.setBoolPref(XUL_CACHE_PREF, true);

  let file = chromeURIToFile(manifestURI);
  let tempfile = copyToTemporaryFile(file);
  Components.manager.QueryInterface(Ci.nsIComponentRegistrar).
    autoRegister(tempfile);

  gChromeReg.refreshSkins();

  return function() {
    tempfile.fileSize = 0; // truncate the manifest
    gChromeReg.checkForNewChrome();
    gChromeReg.refreshSkins();
    gPrefs.clearUserPref(XUL_CACHE_PREF);
  }
}

function registerManifestPermanently(manifestURI)
{
  Components.manager.QueryInterface(Ci.nsIComponentRegistrar).
    autoRegister(chromeURIToFile(manifestURI));
}
