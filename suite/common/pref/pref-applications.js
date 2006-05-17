/* -*- Mode: Java; tab-width: 2; c-basic-offset: 2; -*-
 * 
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributor(s):
 */

var gNewTypeRV    = null;
var gUpdateTypeRV = null;
var gList   = null;
var gDS     = null;
var gPrefApplicationsBundle = null;

var gExtensionField = null;
var gMIMETypeField  = null;
var gHandlerField   = null;
var gNewTypeButton  = null;
var gEditButton     = null;
var gRemoveButton   = null;

function newType()
{
  window.openDialog("chrome://communicator/content/pref/pref-applications-new.xul", "appEdit", "chrome,modal=yes,resizable=no");
  if (gNewTypeRV) {
    //gList.builder.rebuild();
    gList.setAttribute("ref", "urn:mimetypes");
    gNewTypeRV = null;
  }
}

function removeType()
{
  var titleMsg = gPrefApplicationsBundle.getString("removeHandlerTitle");
  var dialogMsg = gPrefApplicationsBundle.getString("removeHandler");
  dialogMsg = dialogMsg.replace(/%n/g, "\n");
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
  var remove = promptService.confirm(window, titleMsg, dialogMsg);
  if (remove) {
    var uri = gList.selectedItems[0].id;
    var handlerOverride = new HandlerOverride(uri);
    removeOverride(handlerOverride.mimeType);
    gList.setAttribute("ref", "urn:mimetypes");
  }
}

function editType()
{
  if (gList.selectedItems && gList.selectedItems[0]) {
    var uri = gList.selectedItems[0].id;
    var handlerOverride = new HandlerOverride(uri);
    window.openDialog("chrome://communicator/content/pref/pref-applications-edit.xul", "appEdit", "chrome,modal=yes,resizable=no", handlerOverride);
    selectApplication();
  }
}

function Startup()
{
  // set up the string bundle
  gPrefApplicationsBundle = document.getElementById("bundle_prefApplications");

  // set up the elements
  gList = document.getElementById("appList"); 
  gExtensionField = document.getElementById("extension");        
  gMIMETypeField  = document.getElementById("mimeType");
  gHandlerField   = document.getElementById("handler");
  gNewTypeButton  = document.getElementById("newTypeButton");
  gEditButton     = document.getElementById("editButton");
  gRemoveButton   = document.getElementById("removeButton");

  // Disable the Edit & Remove buttons until we click on something
  updateLockedButtonState(false);

  const mimeTypes = "UMimTyp";
  var fileLocator = Components.classes["@mozilla.org/file/directory_service;1"].getService(Components.interfaces.nsIProperties);
  
  var file = fileLocator.get(mimeTypes, Components.interfaces.nsIFile);

  var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
  var fileHandler = ioService.getProtocolHandler("file").QueryInterface(Components.interfaces.nsIFileProtocolHandler);
  dump("spec is " + fileHandler.getURLSpecFromFile(file));
  gDS = gRDF.GetDataSource(fileHandler.getURLSpecFromFile(file));

  // intialize the listbox
  gList.database.AddDataSource(gDS);
  gList.setAttribute("ref", "urn:mimetypes");
}

function selectApplication()
{
  if (gList.selectedItems && gList.selectedItems.length && gList.selectedItems[0]) {
    var uri = gList.selectedItems[0].id;
    var handlerOverride = new HandlerOverride(uri);
    gExtensionField.setAttribute("value", handlerOverride.extensions);
    gMIMETypeField.setAttribute("value", handlerOverride.mimeType);
    
    // figure out how this type is handled
    if (handlerOverride.handleInternal == "true")
      gHandlerField.setAttribute("value",
                                 gPrefApplicationsBundle.getString("handleInternally"));
    else if (handlerOverride.saveToDisk == "true")
      gHandlerField.setAttribute("value",
                                 gPrefApplicationsBundle.getString("saveToDisk"));
    else 
      gHandlerField.setAttribute("value", handlerOverride.appDisplayName);
    var ext;
    var posOfFirstSpace = handlerOverride.extensions.indexOf(" ");
    if (posOfFirstSpace > -1)
      ext = handlerOverride.extensions.substr(0, posOfFirstSpace);
    else
      ext = handlerOverride.extensions;
    var imageString = "moz-icon://" + "dummy." + ext.toLowerCase() + "?size=32&contentType=" + handlerOverride.mimeType;
    document.getElementById("contentTypeImage").setAttribute("src", imageString);
    updateLockedButtonState(handlerOverride.isEditable == "true");
    delete handlerOverride;
  } else {
    updateLockedButtonState(false)
    gHandlerField.removeAttribute("value");
    document.getElementById("contentTypeImage").removeAttribute("src");
    gExtensionField.removeAttribute("value");
    gMIMETypeField.removeAttribute("value");
  }
} 

// disable locked buttons
function updateLockedButtonState(handlerEditable)
{
  gNewTypeButton.disabled = parent.hPrefWindow.getPrefIsLocked(gNewTypeButton.getAttribute("prefstring") );
  if (!handlerEditable ||
      parent.hPrefWindow.getPrefIsLocked(gEditButton.getAttribute("prefstring"))) {
    gEditButton.disabled = true;
  } else {
    gEditButton.disabled = false;
  }
      
  if (!handlerEditable ||
      parent.hPrefWindow.getPrefIsLocked(gRemoveButton.getAttribute("prefstring"))) {
    gRemoveButton.disabled = true;
  } else {
    gRemoveButton.disabled = false;
  }
}

function clearRememberedSettings()
{
  var prefBranch = Components.classes["@mozilla.org/preferences;1"].getService(Components.interfaces.nsIPrefBranch);
  if (prefBranch) {
    prefBranch.setCharPref("browser.helperApps.neverAsk.saveToDisk", "");
    prefBranch.setCharPref("browser.helperApps.neverAsk.openFile", "");
  }
}
