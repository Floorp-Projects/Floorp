var bundle = srGetStrBundle("chrome://pref/locale/prefutilities.properties");

function prefNavSelectFile()
{
  var folderField = document.getElementById("browserStartupHomepage");
  var url = getFileOrFolderURL( bundle.GetStringFromName("choosehomepage"), false );
  if( url != -1 )
    folderField.value = url;
}

function setHomePageToCurrentPage()
{
  if( !parent.opener.appCore )
    return false;
  var homePageField = document.getElementById("browserStartupHomepage");
  var url = parent.opener.content.location.href;
  if( url )
    homePageField.value = url;
}