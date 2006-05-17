/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 */

const nsIFilePicker          = Components.interfaces.nsIFilePicker;
const nsILocalFile           = Components.interfaces.nsILocalFile;

function Startup()
{
  var pref = Components.classes["@mozilla.org/preferences-service;1"]
                       .getService(Components.interfaces.nsIPrefBranch);
  var path = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(nsILocalFile);
  
  path =  pref.getComplexValue("browser.cache.disk.parent_directory", nsILocalFile);
  document.getElementById("browserCacheDiskCacheFolder").value = path.unicodePath;
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
  fp.appendFilters(nsIFilePicker.filterAll);
  var ret = fp.show();

  if (ret == nsIFilePicker.returnOK) {
    var localFile = fp.file.QueryInterface(nsILocalFile);
    var viewable = fp.file.unicodePath;
    var folderField = document.getElementById("browserCacheDiskCacheFolder");
    folderField.value = viewable;
    pref.setComplexValue("browser.cache.disk.parent_directory", nsILocalFile, localFile)
  }
}

function prefClearCache(aType)
{
    var classID = Components.classes["@mozilla.org/network/cache-service;1"];
    var cacheService = classID.getService(Components.interfaces.nsICacheService);
    cacheService.evictEntries(aType);
}

function prefClearMemCache()
{
    prefClearCache(Components.interfaces.nsICache.STORE_IN_MEMORY);
}

function prefClearDiskCache()
{
    prefClearCache(Components.interfaces.nsICache.STORE_ON_DISK);
}
