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
      gExtensionField.value   = info.FirstExtension();
      gMIMEField.value        = info.MIMEType;
      var app = info.preferredApplicationHandler;
      if ( app ) {
          gAppPath.value      = app.unicodePath;
      }

      // Don't let user change mime type.
      gMIMEField.setAttribute( "readonly", "true" );

      // Start user in app field.
      gAppPath.focus();
  } else {
      gDescriptionField.focus();
  }
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
  const mimeTypes = "UMimTyp";
  var fileLocator = Components.classes["@mozilla.org/file/directory_service;1"].getService();
  if (fileLocator)
    fileLocator = fileLocator.QueryInterface(Components.interfaces.nsIProperties);
  var file = fileLocator.get(mimeTypes, Components.interfaces.nsIFile);
  gDS = gRDF.GetDataSource(file.URL);
  if (gDS)
    gDS = gDS.QueryInterface(Components.interfaces.nsIRDFDataSource);

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
      return;
    }
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
  file = Components.classes["@mozilla.org/file/local;1"].createInstance();
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
  }

  window.opener.gNewTypeRV = gMIMEField.value;
  window.close();  
}

