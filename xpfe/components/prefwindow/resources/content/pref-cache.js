function prefCacheSelectFolder()
{
  bundle = srGetStrBundle("chrome://pref/locale/prefutilities.properties");
  var folderField = document.getElementById("browserCacheDirectory");
  var url = getFileOrFolderSpec( bundle.GetStringFromName("cachefolder"), true );
  if( url != -1 )
    folderField.value = url.URLString;
}

function prefClearMemCache()
{
	dump("prefClearMemCache \n");
	var cache = Components.classes['component://netscape/network/cache?name=manager'].getService();
  var	cacheService = cache.QueryInterface( Components.interfaces.nsINetDataCacheManager);
	cacheService.Clear( 2 );
}

function prefClearDiskCache()
{
	dump("prefClearDiskCache \n");
	var cache = Components.classes['component://netscape/network/cache?name=manager'].getService();
  var	cacheService = cache.QueryInterface( Components.interfaces.nsINetDataCacheManager);
	cacheService.Clear( 12 );
}
