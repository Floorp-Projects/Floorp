function prefCacheSelectFolder()
{
  var prefutilitiesBundle = document.getElementById("bundle_prefutilities");
  var folderField = document.getElementById("browserCacheDirectory");
  var file = getFileOrFolderSpec(prefutilitiesBundle.getString("cachefolder"),
                                 true);
  if ( file && file != -1 )
    folderField.value = file.unicodePath;
}

function prefClearMemCache()
{
  var cache = Components.classes['@mozilla.org/network/cache;1?name=manager'].getService();
  var cacheService = cache.QueryInterface( Components.interfaces.nsINetDataCacheManager);
  cacheService.clear( 2 );
}

function prefClearDiskCache()
{
  var cache = Components.classes['@mozilla.org/network/cache;1?name=manager'].getService();
  var cacheService = cache.QueryInterface( Components.interfaces.nsINetDataCacheManager);
  cacheService.clear( 12 );
}
