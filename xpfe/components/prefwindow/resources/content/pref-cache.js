function prefCacheSelectFolder()
{
  bundle = srGetStrBundle("chrome://communicator/locale/pref/prefutilities.properties");
  var folderField = document.getElementById("browserCacheDirectory");
  var file = getFileOrFolderSpec( bundle.GetStringFromName("cachefolder"), true );
  if ( file && file != -1 )
    folderField.value = file.unicodePath;
}

function prefClearMemCache()
{
	dump("prefClearMemCache \n");
	var cache = Components.classes['@mozilla.org/network/cache;1?name=manager'].getService();
  var	cacheService = cache.QueryInterface( Components.interfaces.nsINetDataCacheManager);
	cacheService.clear( 2 );
}

function prefClearDiskCache()
{
	dump("prefClearDiskCache \n");
	var cache = Components.classes['@mozilla.org/network/cache;1?name=manager'].getService();
  var	cacheService = cache.QueryInterface( Components.interfaces.nsINetDataCacheManager);
	cacheService.clear( 12 );
}
