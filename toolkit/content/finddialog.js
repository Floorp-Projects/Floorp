// -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");

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
  dialog.find            = document.getElementById("btnFind");
  dialog.up              = document.getElementById("radioUp");
  dialog.down            = document.getElementById("radioDown");
  dialog.rg              = dialog.up.radioGroup;
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
  var findBackwards              = gFindInst.findBackwards ? gFindInst.findBackwards : findService.findBackwards;
  if (findBackwards)
    dialog.rg.selectedItem = dialog.up;
  else
    dialog.rg.selectedItem = dialog.down;
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
  findService.findBackwards = dialog.up.selected;
}

function onLoad()
{
  initDialogObject();

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
  gFindInst.findBackwards = dialog.up.selected;
  
  // Search.
  var result = gFindInst.findNext();

  if (!result)
  {
    if (!dialog.bundle)
      dialog.bundle = document.getElementById("findBundle");
    Services.prompt.alert(window, dialog.bundle.getString("notFoundTitle"),
                                  dialog.bundle.getString("notFoundWarning"));
    dialog.findKey.select();
    dialog.findKey.focus();
  } 
  return false;
}

function doEnabling()
{
  dialog.find.disabled = !dialog.findKey.value;
}
