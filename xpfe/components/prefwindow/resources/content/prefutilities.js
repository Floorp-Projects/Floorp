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
const nsIFileURL    = Components.interfaces.nsIFileURL;

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
    var ret = fp.show();
    if (ret == nsIFilePicker.returnCancel)
      return -1;
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
      try {
        var fileURL = Components.classes["@mozilla.org/network/standard-url;1"].createInstance(nsIFileURL);
        fileURL.file = file;
        var url = fileURL.spec;
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
  var windowManager = Components.classes["@mozilla.org/rdf/datasource;1?name=window-mediator"]
                                .getService(Components.interfaces.nsIWindowMediator);
  var browserWindow = windowManager.getMostRecentWindow("navigator:browser");
  if (browserWindow) {
    var browser = browserWindow.document.getElementById("content");
    var url = browser.webNavigation.currentURI.spec;
    if (url) {
      var homePageField = document.getElementById(folderFieldId);
      homePageField.value = url;
    }
  }
}

function prefClearGlobalHistory()
{
  var globalHistory = nsJSComponentManager.getService("@mozilla.org/browser/global-history;1", "nsIGlobalHistory");
  if (globalHistory)
    globalHistory.RemoveAllPages();
}

function prefClearUrlbarHistory()
{
  var button = document.getElementById("ClearUrlBarHistoryButton");
  var urlBarHist = nsJSComponentManager.getService("@mozilla.org/browser/urlbarhistory;1",
  "nsIUrlbarHistory");
  if ( urlBarHist )
  {
    urlBarHist.clearHistory();
    button.setAttribute("disabled","true");
  }
}  

var bundle = srGetStrBundle("chrome://communicator/locale/pref/prefutilities.properties");

