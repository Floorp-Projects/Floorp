/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett       <alecf@netscape.com>
 *   Bill Law         <law@netscape.com>
 *   Blake Ross       <blakeross@telocity.com>
 *   Dean Tessman     <dean_tessman@hotmail.com>
 *   Matt Fisher      <matt@netscape.com>
 *   Simon Fraser     <sfraser@netscape.com>
 *   Stuart Parmenter <pavlov@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var dialog;     // Quick access to document/form elements.
var gFindInst;   // nsIWebBrowserFind that we're going to use
var gFindInstData; // use this to update the find inst data

function initDialogObject()
{
  // Create dialog object and initialize.
  dialog = new Object;
  dialog.findKey         = document.getElementById("dialog.findKey");
  dialog.caseSensitive   = document.getElementById("dialog.caseSensitive");
  dialog.wrap            = document.getElementById("dialog.wrap");
  dialog.searchBackwards = document.getElementById("dialog.searchBackwards");
  dialog.find            = document.getElementById("findDialog").getButton("accept");
  dialog.bundle          = null;

  // Move dialog to center, if it not been shown before
  var windowElement = document.getElementById("findDialog");
  if (!windowElement.hasAttribute("screenX") || !windowElement.hasAttribute("screenY"))
  {
    sizeToContent();
    moveToAlertPosition();
  }
}

function fillDialog()
{
  // get the find service, which stores global find state
  var findService = Components.classes["@mozilla.org/find/find_service;1"]
                         .getService(Components.interfaces.nsIFindService);
  
  // Set initial dialog field contents. Use the gFindInst attributes first,
  // this is necessary for window.find()
  dialog.findKey.value           = gFindInst.searchString ? gFindInst.searchString : findService.searchString;
  dialog.caseSensitive.checked   = gFindInst.matchCase ? gFindInst.matchCase : findService.matchCase;
  dialog.wrap.checked            = gFindInst.wrapFind ? gFindInst.wrapFind : findService.wrapFind;
  dialog.searchBackwards.checked = gFindInst.findBackwards ? gFindInst.findBackwards : findService.findBackwards;
}

function saveFindData()
{
  // get the find service, which stores global find state
  var findService = Components.classes["@mozilla.org/find/find_service;1"]
                         .getService(Components.interfaces.nsIFindService);

  // Set data attributes per user input.
  findService.searchString  = dialog.findKey.value;
  findService.matchCase     = dialog.caseSensitive.checked;
  findService.wrapFind      = dialog.wrap.checked;
  findService.findBackwards = dialog.searchBackwards.checked;
}

function onLoad()
{
  initDialogObject();

  // Change "OK" to "Find".
  dialog.find.label = document.getElementById("fBLT").getAttribute("label");
  dialog.find.accessKey = document.getElementById("fBLT").getAttribute("accesskey");

  // get the find instance
  var arg0 = window.arguments[0];
  // If the dialog was opened from window.find(),
  // arg0 will be an instance of nsIWebBrowserFind
  if (arg0 instanceof Components.interfaces.nsIWebBrowserFind) {
    gFindInst = arg0;
  } else {
    gFindInstData = arg0;
    gFindInst = gFindInstData.webBrowserFind;
  }

  fillDialog();
  doEnabling();

  if (dialog.findKey.value)
    dialog.findKey.select();
  dialog.findKey.focus();
}

function onUnload()
{
    window.opener.findDialog = 0;
}

function onAccept()
{
  if (gFindInstData && gFindInst != gFindInstData.webBrowserFind) {
    gFindInstData.init();
    gFindInst = gFindInstData.webBrowserFind;
  }

  // Transfer dialog contents to the find service.
  saveFindData();

  // set up the find instance
  gFindInst.searchString  = dialog.findKey.value;
  gFindInst.matchCase     = dialog.caseSensitive.checked;
  gFindInst.wrapFind      = dialog.wrap.checked;
  gFindInst.findBackwards = dialog.searchBackwards.checked;
  
  // Search.
  var result = gFindInst.findNext();

  if (!result)
  {
    if (!dialog.bundle)
      dialog.bundle = document.getElementById("findBundle");

    try {
      var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
      promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService);
      promptService.alert(window, null,
                          dialog.bundle.getString("notFoundWarning"));
    }
    catch (e) { dump("The text you entered was not found."); }

    dialog.findKey.select();
    dialog.findKey.focus();
  } 
  return false;
}

function doEnabling()
{
  dialog.find.disabled = !dialog.findKey.value;
}
