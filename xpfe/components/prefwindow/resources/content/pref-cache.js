/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 */

const nsIFilePicker       = Components.interfaces.nsIFilePicker;
const nsILocalFile        = Components.interfaces.nsILocalFile;
const nsIFile             = Components.interfaces.nsIFile;
const nsIProperties       = Components.interfaces.nsIProperties;
const kCacheParentDirPref = "browser.cache.disk.parent_directory";

function Startup()
{
  var path = null;
  var pref = null;
  try 
  {
    pref = Components.classes["@mozilla.org/preferences-service;1"]
      .getService(Components.interfaces.nsIPrefBranch);
    path = pref.getComplexValue(kCacheParentDirPref, nsILocalFile);
  }
  catch (ex)
  {
    // parent_directory pref not set; path is null so we'll failover
    // to filling in the parent_directory as the profile directory
  }

  if (!path)
  {
    try
    {
      // no disk cache folder found; default to profile directory
      var dirSvc = Components.classes["@mozilla.org/file/directory_service;1"]
        .getService(nsIProperties);
      var ifile = dirSvc.get("ProfD", nsIFile);
      path = ifile.QueryInterface(nsILocalFile);

      // now remember the new assumption
      if (pref)
        pref.setComplexValue(kCacheParentDirPref, nsILocalFile, path);
    }
    catch (ex)
    {
    }
  }

  // if both pref and dir svc fail leave this field blank else show path
  if (path)
    document.getElementById("browserCacheDiskCacheFolder").value = 
      path.path;
}


function prefCacheSelectFolder()
{
  var fp = Components.classes["@mozilla.org/filepicker;1"]
                     .createInstance(nsIFilePicker);
  var pref = Components.classes["@mozilla.org/preferences-service;1"]
                     .getService(Components.interfaces.nsIPrefBranch);
  var prefutilitiesBundle = document.getElementById("bundle_prefutilities");
  var title = prefutilitiesBundle.getString("cachefolder");

  fp.init(window, title, nsIFilePicker.modeGetFolder);
  try 
  {
    var initialDir = pref.getComplexValue(kCacheParentDirPref, nsILocalFile);
    if (initialDir)
      fp.displayDirectory = initialDir;
  }
  catch (ex)
  {
    // ignore exception: file picker will open at default location
  }
  fp.appendFilters(nsIFilePicker.filterAll);
  var ret = fp.show();

  if (ret == nsIFilePicker.returnOK) {
    var localFile = fp.file.QueryInterface(nsILocalFile);
    var viewable = fp.file.path;
    var folderField = document.getElementById("browserCacheDiskCacheFolder");
    folderField.value = viewable;
    pref.setComplexValue(kCacheParentDirPref, nsILocalFile, localFile)
  }
}

function prefClearCache(aType)
{
    var classID = Components.classes["@mozilla.org/network/cache-service;1"];
    var cacheService = classID.getService(Components.interfaces.nsICacheService);
    cacheService.evictEntries(aType);
}

function prefClearDiskAndMemCache()
{
    prefClearCache(Components.interfaces.nsICache.STORE_ON_DISK);
    prefClearCache(Components.interfaces.nsICache.STORE_IN_MEMORY);
}
