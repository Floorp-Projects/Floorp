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

var gDescriptionField = null;
var gExtensionField   = null;
var gMIMEField        = null;
var gAppPath          = null;

var gPrefApplicationsBundle = null;

function Startup()
{
  doSetOKCancel(onOK);
  
  gDescriptionField = document.getElementById("description");
  gExtensionField   = document.getElementById("extensions");
  gMIMEField        = document.getElementById("mimeType");
  gAppPath          = document.getElementById("appPath");
    
  gPrefApplicationsBundle = document.getElementById("bundle_prefApplications");

  // If an arg was passed, then it's an nsIHelperAppLauncherDialog
  if ( "arguments" in window && window.arguments[0] ) {
      // Get mime info.
      var info = window.arguments[0].mLauncher.MIMEInfo;

      // Fill the fields we can from this.
      gDescriptionField.value = info.Description;
      gExtensionField.value   = info.primaryExtension;
      gMIMEField.value        = info.MIMEType;
      // an app may have been selected in the opening dialog but not in the mimeinfo
      var app = info.preferredApplicationHandler || window.arguments[0].chosenApp;
      if ( app ) {
          gAppPath.value      = app.path;
      }

      // Don't let user change mime type.
      gMIMEField.setAttribute( "readonly", "true" );

      // Start user in app field.
      gAppPath.focus();
  } else {
      gDescriptionField.focus();
  }
  sizeToContent();
  moveToAlertPosition();
}

function chooseApp()
{
  const nsIFilePicker = Components.interfaces.nsIFilePicker;
  var filePicker = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  if (filePicker) {
    const FP = Components.interfaces.nsIFilePicker
    var windowTitle = gPrefApplicationsBundle.getString("chooseHandler");
    var programsFilter = gPrefApplicationsBundle.getString("programsFilter");
    filePicker.init(window, windowTitle, FP.modeOpen);
    if (navigator.platform == "Win32")
      filePicker.appendFilter(programsFilter, "*.exe; *.com");
    else
      filePicker.appendFilters(FP.filterAll);
    var filePicked = filePicker.show();
    if (filePicked == nsIFilePicker.returnOK && filePicker.file) {
      var file = filePicker.file.QueryInterface(Components.interfaces.nsILocalFile);
      gAppPath.value = file.path;
      gAppPath.select();
    }
  }
}

var gDS = null;
function onOK()
{
  // Make sure all fields are filled in OK.
  if ( !checkInput() ) {
    return false;
  }

  const mimeTypes = "UMimTyp";
  var fileLocator = Components.classes["@mozilla.org/file/directory_service;1"].getService(Components.interfaces.nsIProperties);
  
  var file = fileLocator.get(mimeTypes, Components.interfaces.nsIFile);
  
  var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
  var fileHandler = ioService.getProtocolHandler("file").QueryInterface(Components.interfaces.nsIFileProtocolHandler);
  
  gDS = gRDF.GetDataSource(fileHandler.getURLSpecFromFile(file));

  gMIMEField.value = gMIMEField.value.toLowerCase();
	
  // figure out if this mime type already exists. 
  var exists = mimeHandlerExists(gMIMEField.value);
  if (exists) {
    var titleMsg = gPrefApplicationsBundle.getString("handlerExistsTitle");
    var dialogMsg = gPrefApplicationsBundle.getString("handlerExists");
    dialogMsg = dialogMsg.replace(/%mime%/g, gMIMEField.value);
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
    var replace = promptService.confirm(window, titleMsg, dialogMsg);
    if (!replace)
    {
      window.close();
      return false;
    }
  }
  
  
  // now save the information
  var handlerInfo = new HandlerOverride(MIME_URI(gMIMEField.value));
  handlerInfo.mUpdateMode = exists; // XXX Somewhat sleazy, I know...
  handlerInfo.mimeType = gMIMEField.value;
  handlerInfo.description = gDescriptionField.value;
  
  var extensionString = gExtensionField.value.replace(/[*.;]/g, "").toLowerCase();
  var extensions = extensionString.split(" ");
  for (var i = 0; i < extensions.length; i++) {
    var currExtension = extensions[i];
    handlerInfo.addExtension(currExtension);
  }
  handlerInfo.appPath = gAppPath.value;

  // other info we need to set (not reflected in UI)
  handlerInfo.isEditable = true;
  handlerInfo.saveToDisk = false;
  handlerInfo.handleInternal = false;
  handlerInfo.alwaysAsk = true;
  file = Components.classes["@mozilla.org/file/local;1"].createInstance();
  if (file)
    file = file.QueryInterface(Components.interfaces.nsILocalFile);
  if (file) {
    try {
      file.initWithPath(gAppPath.value);
      handlerInfo.appDisplayName = file.leafName;
    }
    catch(e) {
      handlerInfo.appDisplayName = gAppPath.value;    
    }
  }
  // do the rest of the work (ugly yes, but it works)
  handlerInfo.buildLinks();
  
  // flush the ds to disk.   
  var remoteDS = gDS.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
  if (remoteDS)
    remoteDS.Flush();
  
  // If an arg was passed, then it's an nsIHelperAppLauncherDialog
  // and we need to update its MIMEInfo.
  if ( "arguments" in window && window.arguments[0] ) {
      // Get mime info.
      var info = window.arguments[0].mLauncher.MIMEInfo;

      // Update fields that might have changed.
      info.preferredAction = Components.interfaces.nsIMIMEInfo.useHelperApp;
      info.Description = gDescriptionField.value;
      info.preferredApplicationHandler = file;
      info.applicationDescription = handlerInfo.appDisplayName;

      // Tell the nsIHelperAppLauncherDialog to update to the changes
      window.arguments[0].updateSelf = true;
  }

  window.close();  
  return false;
}

