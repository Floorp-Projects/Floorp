/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

var bundle = srGetStrBundle("chrome://pref/locale/prefutilities.properties");

function getFileOrFolderSpec( aTitle, aFolder )
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
  return fileSpecWithUI;
}

function prefNavSelectFile(folderFieldId, stringId, useNative)
{
  var folderField = document.getElementById(folderFieldId);
  var spec = getFileOrFolderSpec( bundle.GetStringFromName(stringId), false );
  if( spec != -1 ) {
      if (useNative)
          folderField.value = spec.nativePath;
      else
          folderField.value = spec.URLString;
  }
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
