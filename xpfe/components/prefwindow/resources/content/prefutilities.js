var bundle = srGetStrBundle("chrome://pref/locale/prefutilities.properties");

function getFileOrFolderURL( aTitle, aFolder )
{
  try {
    var fileSpecWithUI = Components.classes["component://netscape/filespecwithui"].createInstance();
    if( fileSpecWithUI )
      fileSpecWithUI = fileSpecWithUI.QueryInterface( Components.interfaces.nsIFileSpecWithUI );
  }
  catch(e) {
    dump("*** failed to create fileSpecWithUI or fileSpec objects\n");
    return false;
  }
  try {
    var value;
    if( aFolder ) 
      value = fileSpecWithUI.chooseDirectory( aTitle );
    else 
      value = fileSpecWithUI.chooseFile( aTitle );
    dump("filespecWithUI.path = " + value + "\n");
    fileSpecWithUI.URLString = value;
  }
  catch(e) {
    dump("Error: " + e + "\n");
    return -1;
  }
  return fileSpecWithUI.URLString;
}

function prefNavSelectFile(folderFieldId, stringId)
{
  var folderField = document.getElementById(folderFieldId);
  var url = getFileOrFolderURL( bundle.GetStringFromName(stringId), false );
  if( url != -1 )
    folderField.value = url;
}

function setHomePageToCurrentPage(folderFieldId)
{
  if( !parent.opener.appCore )
    return false;
  var homePageField = document.getElementById(folderFieldId);
  var url = parent.opener.content.location.href;
  if( url )
    homePageField.value = url;
}
