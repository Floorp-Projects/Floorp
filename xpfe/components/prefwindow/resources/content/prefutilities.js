/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Jason Eager <jce2@po.cwru.edu>
 */

const nsIFilePicker = Components.interfaces.nsIFilePicker;

function getFileOrFolderSpec( aTitle, aFolder )
{
  try {
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  }
  catch(e) {
    dump("*** failed to create fileSpecWithUI or fileSpec objects\n");
    return false;
  }

  try {
    var mode;
    if (aFolder)
      mode = nsIFilePicker.modeGetFolder;
    else
      mode = nsIFilePicker.modeOpen;

    fp.init(window, aTitle, mode);
    fp.appendFilters(nsIFilePicker.filterAll);
    fp.show();
  }
  catch(e) {
    dump("Error: " + e + "\n");
    return -1;
  }

  return fp.file;
}

function prefNavSelectFile(folderFieldId, stringId, useNative)
{
  var folderField = document.getElementById(folderFieldId);
  var dlgString = stringId ? bundle.GetStringFromName(stringId) : '';
  var file = getFileOrFolderSpec( dlgString, false );
  if( file != -1 )
  {
    /* XXX nsILocalFile doesn't have a URL string */
    if (useNative)
    {
      folderField.value = file.unicodePath;
    }
    else
    {
      // Hack to get a file: url from an nsIFile
      var tempFileSpec = Components.classes["@mozilla.org/filespec;1"].createInstance(Components.interfaces.nsIFileSpec);
      tempFileSpec.nativePath = file.unicodePath;

      try {
        var url = tempFileSpec.URLString;
        if( url )
          folderField.value = url;
      }
      catch(e) {
      }
    }
  }
}

function setHomePageToCurrentPage(folderFieldId)
{
  if( !parent.opener.appCore )
    return false;
  var homePageField = document.getElementById(folderFieldId);
  var url = parent.opener._content.location.href;
  if( url )
    homePageField.value = url;
}

function prefClearUrlbarHistory()
{
  var button = document.getElementById("ClearUrlBarHistoryButton");
  var urlBarHist = nsJSComponentManager.getService("@mozilla.org/browser/urlbarhistory;1",
  "nsIUrlbarHistory");
  if ( urlBarHist )
  {
  	urlBarHist.clearHistory();
  	dump("Now history should be empty. \n");
    button.setAttribute("disabled","true");
  }
}  

var bundle = srGetStrBundle("chrome://communicator/locale/pref/prefutilities.properties");

