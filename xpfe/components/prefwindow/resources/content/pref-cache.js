
function prefCacheSelectFolder()
{
  bundle = srGetStrBundle("chrome://pref/locale/prefutilities.properties");
  var folderField = document.getElementById("browserCacheDirectory");
  var url = getFileOrFolderURL( bundle.GetStringFromName("cachefolder"), true );
  if( url != -1 )
    folderField.value = url;
}