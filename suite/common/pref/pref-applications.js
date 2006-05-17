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
var gMIMEDescField  = null;
var gHandlerField   = null;
var gNewTypeButton  = null;
var gEditButton     = null;
var gRemoveButton   = null;

function newType()
{
  var handlerOverride = new HandlerOverride();
  window.openDialog("chrome://communicator/content/pref/pref-applications-edit.xul", "appEdit", "chrome,modal=yes,resizable=no", handlerOverride);
  if (gNewTypeRV) {
    gList.builder.rebuild();
    gNewTypeRV = null;
  }
}

function removeType()
{
  // Only prompt if setting is "useHelperApp".
  var uri = gList.selectedItems[0].id;
  var handlerOverride = new HandlerOverride(uri);
  if ( !handlerOverride.useSystemDefault && !handlerOverride.saveToDisk ) {
    var titleMsg = gPrefApplicationsBundle.getString("removeHandlerTitle");
    var dialogMsg = gPrefApplicationsBundle.getString("removeHandler");
    dialogMsg = dialogMsg.replace(/%n/g, "\n");
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
    var remove = promptService.confirm(window, titleMsg, dialogMsg);
    if (!remove) {
      return;
    }
  }
  removeOverride(handlerOverride.mimeType);
  gList.builder.rebuild();
  selectApplication();
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

const xmlSinkObserver = {
  onBeginLoad: function(aSink)
  {
  },
  onInterrupt: function(aSink)
  {
  },
  onResume: function(aSink)
  {
  },
  // This is called when the RDF data source has finished loading.
  onEndLoad: function(aSink)
  {
    // Unhook observer.
    aSink.removeXMLSinkObserver(this);

    // Convert old "don't ask" pref info to helper app pref entries
    try {
      var prefService = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefService);
      var prefBranch = prefService.getBranch("browser.helperApps.neverAsk.");
      if (!prefBranch) return;
    } catch(e) { return; }

    var neverAskSave = new Array();
    var neverAskOpen = new Array();
    try {
      neverAskSave = prefBranch.getCharPref("saveToDisk").split(",");
    } catch(e) {}
    try {
      neverAskOpen = prefBranch.getCharPref("openFile").split(",");
    } catch(e) {}
    
    var i;
		var type;
    var newEntries = {};
    for ( i = 0; i < neverAskSave.length; i++ ) {
      // See if mime type is in data source.
      type = decodeURIComponent(neverAskSave[i]);
      if (type != "" && !mimeHandlerExists(type)) {
        // Not in there, need to create an entry now so user can edit it.
        newEntries[type] = "saveToDisk";
      }
    }
    
    for ( i = 0; i < neverAskOpen.length; i++ ) {
      // See if mime type is in data source.
      type = decodeURIComponent(neverAskOpen[i]);
      if (type != "" && !mimeHandlerExists(type)) {
        // Not in there, need to create an entry now so user can edit it.
        newEntries[type] = "useSystemDefault";
      }
    }
    
    // Now create all new entries.
    for ( var newEntry in newEntries ) {
      this.createNewEntry(newEntry, newEntries[newEntry]);
    }
    
    // Don't need these any more!
    try { prefBranch.clearUserPref("saveToDisk"); } catch(e) {}
    try { prefBranch.clearUserPref("openFile"); } catch(e) {}
  },
  onError: function(aSink, aStatus, aMsg)
  {
  },
  createNewEntry: function(mimeType, action)
  {
    // Create HandlerOverride and populate it.
    var entry = new HandlerOverride(MIME_URI(mimeType));
    entry.mUpdateMode = false;
    entry.mimeType    = mimeType;
    entry.description = "";
    entry.isEditable  = true;
    entry.alwaysAsk   = false;
    entry.appPath     = "";
    entry.appDisplayName = "";
    // This sets preferred action.
    entry[action]     = true;
    
    // Do RDF magic.
    entry.buildLinks();
  }
}

function Startup()
{
  // set up the string bundle
  gPrefApplicationsBundle = document.getElementById("bundle_prefApplications");

  // set up the elements
  gList = document.getElementById("appList"); 
  gExtensionField = document.getElementById("extension");        
  gMIMEDescField  = document.getElementById("mimeDesc");
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
  gDS = gRDF.GetDataSource(fileHandler.getURLSpecFromFile(file));

  // intialize the listbox
  gList.database.AddDataSource(gDS);
  gList.builder.rebuild();

  // Test whether the data source is already loaded.
  if (gDS.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource).loaded) {
    // Do it now.
    xmlSinkObserver.onEndLoad(gDS.QueryInterface(Components.interfaces.nsIRDFXMLSink));
  } else {
    // Add observer that will kick in when data source load completes.
    gDS.QueryInterface(Components.interfaces.nsIRDFXMLSink).addXMLSinkObserver( xmlSinkObserver );
  }
}

function selectApplication()
{
  if (gList.selectedItems && gList.selectedItems.length && gList.selectedItems[0]) {
    var uri = gList.selectedItems[0].id;
    var handlerOverride = new HandlerOverride(uri);
    gExtensionField.setAttribute("value", handlerOverride.extensions);
    gMIMEDescField.setAttribute("value", handlerOverride.description);
    
    // figure out how this type is handled
    if (handlerOverride.handleInternal == "true")
      gHandlerField.setAttribute("value",
                                 gPrefApplicationsBundle.getString("handleInternally"));
    else if (handlerOverride.saveToDisk == "true")
      gHandlerField.setAttribute("value",
                                 gPrefApplicationsBundle.getString("saveToDisk"));
    else if (handlerOverride.useSystemDefault == "true")
      gHandlerField.setAttribute("value",
                                 gPrefApplicationsBundle.getString("useSystemDefault"));
    else 
      gHandlerField.setAttribute("value",
                                 gPrefApplicationsBundle.getFormattedString("useHelperApp", [handlerOverride.appDisplayName]));
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
    gMIMEDescField.removeAttribute("value");
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
