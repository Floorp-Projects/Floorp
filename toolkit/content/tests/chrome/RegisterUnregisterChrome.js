/* This code is mostly copied from chrome/test/unit/head_crtestutils.js */

const NS_CHROME_MANIFESTS_FILE_LIST = "ChromeML";
const XUL_CACHE_PREF = "nglayout.debug.disable_xul_cache";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

let gDirSvc    = Cc["@mozilla.org/file/directory_service;1"].
                    getService(Ci.nsIDirectoryService);
let gChromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].
                    getService(Ci.nsIXULChromeRegistry);
let gPrefs     = Cc["@mozilla.org/preferences-service;1"].
                    getService(Ci.nsIPrefBranch);
var gProvider, gHasChrome, gHasSkins;

function ArrayEnumerator(array)
{
  this.array = array;
}

ArrayEnumerator.prototype = {
  pos: 0,
        
  hasMoreElements: function() {
    return this.pos < this.array.length;
  },
        
  getNext: function() {
    if (this.pos < this.array.length)
      return this.array[this.pos++];
    throw Cr.NS_ERROR_FAILURE;
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsISimpleEnumerator)
        || iid.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

function ChromeProvider(manifests)
{
  this._manifests = manifests;
}

ChromeProvider.prototype = {
  getFile: function(prop, persistent) {
    throw Cr.NS_ERROR_FAILURE;
  },

  getFiles: function(prop) {
    if (prop == NS_CHROME_MANIFESTS_FILE_LIST) {
      return new ArrayEnumerator(this._manifests);
    }
    throw Cr.NS_ERROR_FAILURE;
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIDirectoryServiceProvider)
        || iid.equals(Ci.nsIDirectoryServiceProvider2)
        || iid.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

function registerManifests(manifests)
{
  let provider = new ChromeProvider(manifests);
  gDirSvc.registerProvider(provider);
  return provider;
}

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

  // Register our manifest
  let manifests = [];
  let currentManifests = gDirSvc.QueryInterface(Ci.nsIProperties)
                         .get(NS_CHROME_MANIFESTS_FILE_LIST,
                              Ci.nsISimpleEnumerator);
  while (currentManifests.hasMoreElements())
    manifests.push(currentManifests.getNext());
  let uri = Cc["@mozilla.org/network/io-service;1"].
               getService(Ci.nsIIOService).newURI(chromedir, null, null);
  uri = gChromeReg.convertChromeURL(uri);
  let newChromePath = uri.QueryInterface(Ci.nsIFileURL).file;
  manifests.push(newChromePath);
  gProvider = registerManifests(manifests);
  refreshChrome();
  return uri;
}

function cleanupCustomChrome()
{
  gDirSvc.unregisterProvider(gProvider);
  refreshChrome();
  gPrefs.clearUserPref(XUL_CACHE_PREF);
}
