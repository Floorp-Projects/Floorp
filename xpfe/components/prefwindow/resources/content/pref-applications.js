
var gNewTypeRV    = null;
var gUpdateTypeRV = null;

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
  var titleMsg = gBundle.GetStringFromName("removeHandlerTitle");
  var dialogMsg = gBundle.GetStringFromName("removeHandler");
  dialogMsg = dialogMsg.replace(/%n/g, "\n");
  var commonDialogService = nsJSComponentManager.getService("component://netscape/appshell/commonDialogs",
                                                              "nsICommonDialogs");
  var remove = commonDialogService.Confirm(window, titleMsg, dialogMsg);
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
    dump("*** foopy\n");
    window.openDialog("chrome://communicator/content/pref/pref-applications-edit.xul", "appEdit", "chrome,modal=yes,resizable=no", handlerOverride);
    dump("*** foopy\n");
    selectApplication();
  }
}

var gTree   = null;
var gDS     = null;
var gBundle = null;

var gExtensionField = null;
var gMIMETypeField  = null;
var gHandlerField   = null;
var gEditButton     = null;
var gRemoveButton   = null;

function Startup()
{
  // set up the string bundle
  gBundle = srGetStrBundle("chrome://communicator/locale/pref/pref-applications.properties");

  // set up the elements
  gTree = document.getElementById("appTree"); 
  gExtensionField = document.getElementById("extension");        
  gMIMETypeField  = document.getElementById("mimeType");
  gHandlerField   = document.getElementById("handler");
  gEditButton     = document.getElementById("editButton");
  gRemoveButton   = document.getElementById("removeButton");

  const mimeTypes = "UMimTyp";
  var fileLocator = Components.classes["component://netscape/file/directory_service"].getService();
  if (fileLocator)
    fileLocator = fileLocator.QueryInterface(Components.interfaces.nsIProperties);
  var file = fileLocator.get(mimeTypes, Components.interfaces.nsIFile);
  var file_url = Components.classes["component://netscape/network/standard-url"].createInstance(Components.interfaces.nsIFileURL);
  if (file_url)
    file_url.file = file;
  gDS = gRDF.GetDataSource(file_url.spec);
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
      gHandlerField.setAttribute("value", gBundle.GetStringFromName("handleInternally"));
    else if (handlerOverride.saveToDisk == "true")
      gHandlerField.setAttribute("value", gBundle.GetStringFromName("saveToDisk"));
    else 
      gHandlerField.setAttribute("value", handlerOverride.appDisplayName);

    if (handlerOverride.isEditable == "false") {
      gEditButton.setAttribute("disabled", "true");
      gRemoveButton.setAttribute("disabled", "true");
    }
    else {
      gEditButton.removeAttribute("disabled");
      gRemoveButton.removeAttribute("disabled");
    }
      
    delete handlerOverride;
  }
} 
