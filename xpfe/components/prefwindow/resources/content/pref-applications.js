
var gNewTypeRV    = null;
var gRemoveTypeRV = null;
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
  // implement me
  if (gRemoveTypeRV) {
    //gTree.builder.rebuild();
    gTree.setAttribute("ref", "urn:mimetypes");
    gRemoveTypeRV = null;
  }
}

function editType()
{
  if (gTree.selectedItems && gTree.selectedItems[0]) {
    var uri = gTree.selectedItems[0].id;
    var handlerOverride = new HandlerOverride(uri);
    window.openDialog("chrome://communicator/content/pref/pref-applications-edit.xul", "appEdit", "chrome,modal=yes,resizable=no", handlerOverride);
  }
}

var gTree   = null;
var gDS     = null;
var gBundle = null;

var gExtensionField = null;
var gMIMETypeField  = null;
var gHandlerField   = null;

function Startup()
{
  // set up the string bundle
  gBundle = srGetStrBundle("chrome://communicator/locale/pref/pref-applications.properties");

  // set up the elements
  gTree = document.getElementById("appTree"); 
  gExtensionField = document.getElementById("extension");        
  gMIMETypeField  = document.getElementById("mimeType");
  gHandlerField   = document.getElementById("handler");

  const mimeTypes = 66638;
  var fileLocator = Components.classes["component://netscape/filelocator"].getService();
  if (fileLocator)
    fileLocator = fileLocator.QueryInterface(Components.interfaces.nsIFileLocator);
  var file = fileLocator.GetFileLocation(mimeTypes);
  if (file)
    file = file.QueryInterface(Components.interfaces.nsIFileSpec);
  gDS = gRDF.GetDataSource(file.URLString);
  if (gDS)
    gDS = gDS.QueryInterface(Components.interfaces.nsIRDFDataSource);

  // intialise the tree
  gTree.database.AddDataSource(gDS);
  gTree.setAttribute("ref", "urn:mimetypes");
}

function selectApplication()
{
  if (gTree.selectedItems && gTree.selectedItems[0]) {
    var uri = gTree.selectedItems[0].id;
    var handlerOverride = new HandlerOverride(uri);
    gExtensionField.setAttribute("value", handlerOverride.extensions);
    gMIMETypeField.setAttribute("value", handlerOverride.mimeType);
    
    // figure out how this type is handled
    if (handlerOverride.handleInternal == "true")
      gHandlerField.setAttribute("value", "Handled Internally"); // gBundle.GetStringFromName("handleInternally"));
    else if (handlerOverride.saveToDisk == "true")
      gHandlerField.setAttribute("value", "Save to Disk"); // gBundle.GetStringFromName("saveToDisk"));
    else 
      gHandlerField.setAttribute("value", handlerOverride.appDisplayName);

    delete handlerOverride;
  }
} 
