
function getFileOrFolderURL( aTitle, aFolder )
{
  try {
    var fileSpecWithUI = Components.classes["component://netscape/filespecwithui"].createInstance();
    if( fileSpecWithUI )
      fileSpecWithUI = fileSpecWithUI.QueryInterface( Components.interfaces.nsIFileSpecWithUI );
/*
    var fileSpec = Components.classes["component://netscape/filespec"].createInstance();
    if( fileSpec )
      fileSpec = fileSpec.QueryInterface( Components.interfaces.nsIFileSpec );*/
  }
  catch(e) {
    dump("*** failed to create fileSpecWithUI or fileSpec objects\n");
    return false;
  }
  try {
    if( aFolder )
      fileSpecWithUI.path = fileSpecWithUI.chooseDirectory( aTitle );
    else 
      fileSpecWithUI.path = fileSpecWithUI.chooseFile( aTitle );
  }
  catch(e) {
    return -1;
  }
  return fileSpecWithUI.nativePath;
}

