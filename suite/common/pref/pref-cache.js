/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 */

const nsIFilePicker          = Components.interfaces.nsIFilePicker;
const nsINetDataCacheManager = Components.interfaces.nsINetDataCacheManager;

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
  var cacheService = Components.classes['@mozilla.org/network/cache;1?name=manager']
                               .getService(nsINetDataCacheManager);
  cacheService.clear(aType);
}

function prefClearMemCache()
{
  prefClearCache(nsINetDataCacheManager.MEM_CACHE);
}

function prefClearDiskCache()
{
  prefClearCache(nsINetDataCacheManager.FILE_CACHE |
                 nsINetDataCacheManager.FLAT_CACHE);
}
