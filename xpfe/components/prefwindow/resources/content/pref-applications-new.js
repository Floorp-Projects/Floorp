var gDescriptionField = null;
var gExtensionField   = null;
var gMIMEField        = null;
var gAppPath          = null;
var gOutgoingMIME     = null;

var gBundle           = null;

function Startup()
{
  doSetOKCancel(onOK);
  
  gDescriptionField = document.getElementById("description");
  gExtensionField   = document.getElementById("extensions");
  gMIMEField        = document.getElementById("mimeType");
  gAppPath          = document.getElementById("appPath");
  gOutgoingMime     = document.getElementById("outgoingDefault");
    
  gBundle           = srGetStrBundle("chrome://communicator/locale/pref/pref-applications.properties");  
  
  gDescriptionField.focus();
}

function chooseApp()
{
  const nsIFilePicker = Components.interfaces.nsIFilePicker;
  var filePicker = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  if (filePicker) {
    const FP = Components.interfaces.nsIFilePicker
    var windowTitle = gBundle.GetStringFromName("chooseHandler");
    var programsFilter = gBundle.GetStringFromName("programsFilter");
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
  const mimeTypes = "UMimTyp";
  var fileLocator = Components.classes["@mozilla.org/file/directory_service;1"].getService();
  if (fileLocator)
    fileLocator = fileLocator.QueryInterface(Components.interfaces.nsIProperties);
  var file = fileLocator.get(mimeTypes, Components.interfaces.nsIFile);
  var file_url = Components.classes["@mozilla.org/network/standard-url;1"].createInstance(Components.interfaces.nsIFileURL);
  if (file_url)
    file_url.file = file;
  gDS = gRDF.GetDataSource(file_url.spec);
  if (gDS)
    gDS = gDS.QueryInterface(Components.interfaces.nsIRDFDataSource);

  // figure out if this mime type already exists. 
  var exists = mimeHandlerExists(gMIMEField.value);
  if (exists) {
    var titleMsg = gBundle.GetStringFromName("handlerExistsTitle");
    var dialogMsg = gBundle.GetStringFromName("handlerExists");
    dialogMsg = dialogMsg.replace(/%mime%/g, gMIMEField.value);
    var commonDialogService = nsJSComponentManager.getService("@mozilla.org/appshell/commonDialogs;1",
                                                              "nsICommonDialogs");
    var replace = commonDialogService.Confirm(window, titleMsg, dialogMsg);
    if (!replace)
      window.close();
  }
  
  
  // now save the information
  var handlerInfo = new HandlerOverride(MIME_URI(gMIMEField.value));
  handlerInfo.mUpdateMode = exists; // XXX Somewhat sleazy, I know...
  handlerInfo.mimeType = gMIMEField.value;
  handlerInfo.description = gDescriptionField.value;
  
  var extensionString = gExtensionField.value.replace(/[*.;]/g, "");
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
  var file = Components.classes["@mozilla.org/file/local;1"].createInstance();
  if (file)
    file = file.QueryInterface(Components.interfaces.nsILocalFile);
  if (file) {
    try {
      file.initWithPath(gAppPath.value);
      handlerInfo.appDisplayName = file.unicodeLeafName;
    }
    catch(e) {
      handlerInfo.appDisplayName = gAppPath.value;    
    }
  }
  // do the rest of the work (ugly yes, but it works)
  handlerInfo.buildLinks();
  
  // flush the ds to disk.   
  gDS = gDS.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
  if (gDS)
    gDS.Flush();
  
  window.opener.gNewTypeRV = gMIMEField.value;
  window.close();  
}

