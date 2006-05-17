
var gNewTypeRV    = null;
var gUpdateTypeRV = null;
var gTree   = null;
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
    //gTree.builder.rebuild();
    gTree.setAttribute("ref", "urn:mimetypes");
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
    var uri = gTree.selectedItems[0].id;
    var handlerOverride = new HandlerOverride(uri);
    removeOverride(handlerOverride.mimeType);
    gTree.setAttribute("ref", "urn:mimetypes");
  }
}

function editType()
{
  if (gTree.selectedItems && gTree.selectedItems[0]) {
    var uri = gTree.selectedItems[0].id;
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
  gTree = document.getElementById("appTree"); 
  gExtensionField = document.getElementById("extension");        
  gMIMETypeField  = document.getElementById("mimeType");
  gHandlerField   = document.getElementById("handler");
  gNewTypeButton  = document.getElementById("newTypeButton");
  gEditButton     = document.getElementById("editButton");
  gRemoveButton   = document.getElementById("removeButton");

  // Disable the Edit & Remove buttons until we click on something
  updateLockedButtonState(false);

  const mimeTypes = "UMimTyp";
  var fileLocator = Components.classes["@mozilla.org/file/directory_service;1"].getService();
  if (fileLocator)
    fileLocator = fileLocator.QueryInterface(Components.interfaces.nsIProperties);
  var file = fileLocator.get(mimeTypes, Components.interfaces.nsIFile);
  gDS = gRDF.GetDataSource(file.URL);
  if (gDS)
    gDS = gDS.QueryInterface(Components.interfaces.nsIRDFDataSource);

  // intialise the tree
  gTree.database.AddDataSource(gDS);
  gTree.setAttribute("ref", "urn:mimetypes");
}

function selectApplication()
{
  if (gTree.selectedItems && gTree.selectedItems.length && gTree.selectedItems[0]) {
    var uri = gTree.selectedItems[0].id;
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
