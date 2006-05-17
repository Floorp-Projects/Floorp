/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 */

const nsIFilePicker          = Components.interfaces.nsIFilePicker;

const FILEPICKER_CONTRACTID  = "@mozilla.org/filepicker;1";

function prefCacheSelectFolder()
{
  var fp = Components.classes[FILEPICKER_CONTRACTID]
                     .createInstance(nsIFilePicker);

  var prefutilitiesBundle = document.getElementById("bundle_prefutilities");
  var title = prefutilitiesBundle.getString("cachefolder");
  fp.init(window, title, nsIFilePicker.modeGetFolder);
  fp.appendFilters(nsIFilePicker.filterAll);

  var ret = fp.show();
  if (ret == nsIFilePicker.returnOK) {
    var folderField = document.getElementById("browserCacheDirectory");
    folderField.value = fp.file.unicodePath;
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
