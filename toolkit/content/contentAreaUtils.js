# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
# ***** BEGIN LICENSE BLOCK *****
# Version: NPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Netscape Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is mozilla.org code.
# 
# The Initial Developer of the Original Code is 
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   Ben Goodger <ben@netscape.com> (Save File)
# 
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or 
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the NPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the NPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK *****

/**
 * Determine whether or not a given focused DOMWindow is in the content
 * area.
 **/

function getContentFrameURI(aFocusedWindow)
{
  var contentFrame = isContentFrame(aFocusedWindow) ? aFocusedWindow : window.content;
  return Components.lookupMethod(contentFrame, 'location').call(contentFrame).href;
}

function getReferrer(doc)
{
  var focusedWindow = doc.commandDispatcher.focusedWindow;
  var sourceURL = getContentFrameURI(focusedWindow);

  try {
    var uri = Components.classes["@mozilla.org/network/standard-url;1"].createInstance(Components.interfaces.nsIURI);
    uri.spec = sourceURL;
    return uri;
  } catch (e) {
    return null;
  }
}


// Clientelle: (Make sure you don't break any of these)
//  - File    ->  Save Page/Frame As...
//  - Context ->  Save Page/Frame As...
//  - Context ->  Save Link As...
//  - Context ->  Save Image As...
//  - Alt-Click Save Link As
//
// Try saving each of these types:
// - A complete webpage using File->Save Page As, and Context->Save Page As
// - A webpage as HTML only using the above methods
// - A webpage as Text only using the above methods
// - An image with an extension (e.g. .jpg) in its file name, using
//   Context->Save Image As...
// - An image without an extension (e.g. a banner ad on cnn.com) using
//   the above method. 
// - A linked document using Save Link As...
// - A linked document using Alt-click Save Link As...
//
function saveURL(aURL, aFileName, aFilePickerTitleKey, aShouldBypassCache)
{
  saveInternal(aURL, null, aFileName, aFilePickerTitleKey, aShouldBypassCache);
}

function saveDocument(aDocument)
{
  // In both cases here, we want to use cached data because the 
  // document is currently visible. 
  if (aDocument) 
    saveInternal(aDocument.location.href, aDocument, false);
  else
    saveInternal(_content.location.href, null, false);
}

function saveInternal(aURL, aDocument, 
                      aFileName, aFilePickerTitleKey,
                      aShouldBypassCache)
{
  var data = {
    url: aURL,
    fileName: aFileName,
    filePickerTitle: aFilePickerTitleKey,
    document: aDocument,
    bypassCache: aShouldBypassCache,
    window: window
  };
  var sniffer = new nsHeaderSniffer(aURL, foundHeaderInfo, data);
}

function foundHeaderInfo(aSniffer, aData)
{
  var contentType = aSniffer.contentType;
  var contentEncodingType = aSniffer.contentEncodingType;

  var shouldDecode = false;
  // Are we allowed to decode?
  try {
    const helperAppService =
      Components.classes["@mozilla.org/uriloader/external-helper-app-service;1"].
        getService(Components.interfaces.nsIExternalHelperAppService);
    var url = aSniffer.uri.QueryInterface(Components.interfaces.nsIURL);
    var urlExt = url.fileExtension;
    if (helperAppService.applyDecodingForType(contentType) &&
        (!urlExt || helperAppService.applyDecodingForExtension(urlExt))) {
      shouldDecode = true;
    }
  }
  catch (e) {
  }

  var isDocument = aData.document != null && isDocumentType(contentType);
  if (!isDocument && !shouldDecode && contentEncodingType) {
    // The data is encoded, we are not going to decode it, and this is not a
    // document save so we won't be doing a "save as, complete" (which would
    // break if we reset the type here).  So just set our content type to
    // correspond to the outermost encoding so we get extensions and the like
    // right.
    contentType = contentEncodingType;
  }

  // Determine what the 'default' string to display in the File Picker dialog 
  // should be. 
  var defaultFileName = getDefaultFileName(aData.fileName, 
                                           aSniffer.suggestedFileName, 
                                           aSniffer.uri,
                                           aData.document);

  var defaultExtension = getDefaultExtension(defaultFileName, aSniffer.uri, contentType);
  var defaultString = getNormalizedLeafName(defaultFileName, defaultExtension);

  const prefSvcContractID = "@mozilla.org/preferences-service;1";
  const prefSvcIID = Components.interfaces.nsIPrefService;                              
  var prefs = Components.classes[prefSvcContractID].getService(prefSvcIID).getBranch("browser.download.");
  if (isDocument) 
    prefs.setIntPref("save_converter_index", fp.filterIndex);

  const nsILocalFile = Components.interfaces.nsILocalFile;
  try {
    var dir = prefs.getComplexValue("dir", nsILocalFile);
  }
  catch (e) {
  }

  const lfContractID = "@mozilla.org/file/local;1";

  var prompt = prefs.getBoolPref("promptWithFilepicker");
  var filterIndex = 0;
  var file;
  if (prompt) {
    var fp = makeFilePicker();
    var titleKey = aData.filePickerTitle || "SaveLinkTitle";
    var bundle = getStringBundle();
    fp.init(window, bundle.GetStringFromName(titleKey), 
            Components.interfaces.nsIFilePicker.modeSave);
    
    appendFiltersForContentType(fp, contentType,
                                isDocument ? MODE_COMPLETE : MODE_FILEONLY);  
  
    fp.displayDirectory = dir;
    if (isDocument) {
      try {
        fp.filterIndex = prefs.getIntPref("save_converter_index");
      }
      catch (e) {
      }
    }
  
    fp.defaultExtension = defaultExtension;
    fp.defaultString = defaultString;
  
    if (fp.show() == Components.interfaces.nsIFilePicker.returnCancel || !fp.file)
      return;
  
    var directory = fp.file.parent.QueryInterface(nsILocalFile);
    prefs.setComplexValue("dir", nsILocalFile, directory);
    fp.file.leafName = validateFileName(fp.file.leafName);
    filterIndex = fp.filterIndex;
    file = fp.file;
  }
  else {
    dir.append(defaultString);
    file = dir;
  }

  // If we're saving a document, and are saving either in complete mode or 
  // as converted text, pass the document to the web browser persist component.
  // If we're just saving the HTML (second option in the list), send only the URI.
  var source = (isDocument && filterIndex != 1) ? aData.document : aSniffer.uri;
  var persistArgs = {
    source      : source,
    contentType : (isDocument && filterIndex == 2) ? "text/plain" : contentType,
    target      : file,
    postData    : aData.document ? getPostData() : null,
    bypassCache : aData.bypassCache
  };
  
  var persist = makeWebBrowserPersist();

  // Calculate persist flags.
  const nsIWBP = Components.interfaces.nsIWebBrowserPersist;
  const flags = nsIWBP.PERSIST_FLAGS_NO_CONVERSION | nsIWBP.PERSIST_FLAGS_REPLACE_EXISTING_FILES;
  if (aData.bypassCache)
    persist.persistFlags = flags | nsIWBP.PERSIST_FLAGS_BYPASS_CACHE;
  else 
    persist.persistFlags = flags | nsIWBP.PERSIST_FLAGS_FROM_CACHE;

  if (shouldDecode)
    persist.persistFlags &= ~nsIWBP.PERSIST_FLAGS_NO_CONVERSION;
    
  // Create download and initiate it (below)
  var dl = Components.classes["@mozilla.org/download;1"].createInstance(Components.interfaces.nsIDownload);

  if (isDocument && filterIndex != 1) {
    // Saving a Document, not a URI:
    var filesFolder = null;
    if (persistArgs.contentType != "text/plain") {
      // Create the local directory into which to save associated files. 
      filesFolder = Components .classes[lfContractID].createInstance(lfIID);
      filesFolder.initWithPath(persistArgs.target.path);
      
      var nameWithoutExtension = filesFolder.leafName;
      nameWithoutExtension = nameWithoutExtension.substring(0, nameWithoutExtension.lastIndexOf("."));
      var filesFolderLeafName = getStringBundle().formatStringFromName("filesFolder",
                                                                       [nameWithoutExtension],
                                                                       1);

      filesFolder.leafName = filesFolderLeafName;
    }
      
    var encodingFlags = 0;
    if (persistArgs.contentType == "text/plain") {
      encodingFlags |= nsIWBP.ENCODE_FLAGS_FORMATTED;
      encodingFlags |= nsIWBP.ENCODE_FLAGS_ABSOLUTE_LINKS;
      encodingFlags |= nsIWBP.ENCODE_FLAGS_NOFRAMES_CONTENT;        
    }
    
    const kWrapColumn = 80;
    dl.init(aSniffer.uri, persistArgs.target, null, null, null, persist);
    persist.saveDocument(persistArgs.source, persistArgs.target, filesFolder, 
                         persistArgs.contentType, encodingFlags, kWrapColumn);
  } else {
    dl.init(source, persistArgs.target, null, null, null, persist);
    persist.saveURI(source, persistArgs.postData, persistArgs.target);
  }
}

function nsHeaderSniffer(aURL, aCallback, aData)
{
  this.mCallback = aCallback;
  this.mData = aData;
  
  this.uri = makeURL(aURL);
  
  this.linkChecker = Components.classes["@mozilla.org/network/urichecker;1"]
    .createInstance().QueryInterface(Components.interfaces.nsIURIChecker);

  var flags;
  if (aData.bypassCache) {
    flags = Components.interfaces.nsIRequest.LOAD_BYPASS_CACHE;
  } else {
    flags = Components.interfaces.nsIRequest.LOAD_FROM_CACHE;
  }

  this.linkChecker.asyncCheckURI(aURL, this, null, flags);
}

nsHeaderSniffer.prototype = {

  // ---------- nsISupports methods ----------
  QueryInterface: function (iid) {
    if (!iid.equals(Components.interfaces.nsIRequestObserver) &&
        !iid.equals(Components.interfaces.nsISupports) &&
        !iid.equals(Components.interfaces.nsIInterfaceRequestor) &&
        !iid.equals(Components.interfaces.nsIAuthPrompt)) {
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
    return this;
  },

  // ---------- nsIInterfaceRequestor methods ----------
  getInterface : function(iid) {
    return this.QueryInterface(iid);
  },

  // ---------- nsIAuthPrompt methods ----------
  prompt : function(dlgTitle, text, pwrealm, savePW, defaultText, result)
  {
    dump("authprompt prompt! pwrealm="+pwrealm+"\n");
    var promptServ = this.promptService;
    if (!promptServ)
      return false;
    var saveCheck = {value:savePW};
    return promptServ.prompt(window, dlgTitle, text, defaultText, pwrealm, saveCheck);
  },
  promptUsernameAndPassword : function(dlgTitle, text, pwrealm, savePW, user, pw)
  {
    dump("authprompt promptUsernameAndPassword!  "+dlgTitle+" "+text+", pwrealm="+pwrealm+"\n");
    var promptServ = this.promptService;
    if (!promptServ)
      return false;
    var saveCheck = {value:savePW};
    return promptServ.promptUsernameAndPassword(window, dlgTitle, text, user, pw, pwrealm, saveCheck);
  },
  promptPassword : function(dlgTitle, text, pwrealm, savePW, pw)
  {
    dump("auth promptPassword!  "+dlgTitle+" "+text+", pwrealm="+pwrealm+"\n");
    var promptServ = this.promptService;
    if (!promptServ)
      return false;

    var saveCheck = {value:savePW};
    return promptServ.promptPassword(window, dlgTitle, text, pw, pwrealm, saveCheck);
  },

  // ---------- nsIRequestObserver methods ----------
  onStartRequest: function (aRequest, aContext) { },
  
  onStopRequest: function (aRequest, aContext, aStatus) {
    try {
      if (aStatus == 0) { // NS_BINDING_SUCCEEDED, so there's something there
        var linkChecker = aRequest.QueryInterface(Components.interfaces.nsIURIChecker);
        var channel = linkChecker.baseRequest.QueryInterface(Components.interfaces.nsIChannel);
        this.contentType = channel.contentType;
        try {
          var httpChannel = channel.QueryInterface(Components.interfaces.nsIHttpChannel);
          var encodedChannel = channel.QueryInterface(Components.interfaces.nsIEncodedChannel);
          this.contentEncodingType = null;
          // There may be content-encodings on the channel.  Multiple content
          // encodings are allowed, eg "Content-Encoding: gzip, uuencode".  This
          // header would mean that the content was first gzipped and then
          // uuencoded.  The encoding enumerator returns MIME types
          // corresponding to each encoding starting from the end, so the first
          // thing it returns corresponds to the outermost encoding.
          var encodingEnumerator = encodedChannel.contentEncodings;
          if (encodingEnumerator && encodingEnumerator.hasMoreElements()) {
            try {
              this.contentEncodingType =
                encodingEnumerator.getNext().
                  QueryInterface(Components.interfaces.nsISupportsCString).data;
            } catch (e) {
            }
          }
          this.mContentDisposition = httpChannel.getResponseHeader("content-disposition");
        }
        catch (e) {
        }
        if (!this.contentType || this.contentType == "application/x-unknown-content-type") {
          // We didn't get a type from the server.  Fall back on other type detection mechanisms
          throw "Unknown Type";
        }
      }
      else {
        dump("Error saving link aStatus = 0x" + aStatus.toString(16) + "\n");
        var bundle = getStringBundle();
        var errorTitle = bundle.GetStringFromName("saveLinkErrorTitle");
        var errorMsg = bundle.GetStringFromName("saveLinkErrorMsg");
        const promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
        promptService.alert(this.mData.window, errorTitle, errorMsg);
        return;
      }
    }
    catch (e) {
      if (this.mData.document) {
        this.contentType = this.mData.document.contentType;
      } else {
        try {
          var url = this.uri.QueryInterface(Components.interfaces.nsIURL);
          var ext = url.fileExtension;
          if (ext) {
            var mimeInfo = getMIMEInfoForExtension(ext);
            if (mimeInfo)
              this.contentType = mimeInfo.MIMEType;
          }
        }
        catch (e) {
          // Not much we can do here.  Give up.
        }
      }
    }
    this.mCallback(this, this.mData);
  },

  // ------------------------------------------------

  get promptService()
  {
    var promptSvc;
    try {
      promptSvc = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
      promptSvc = promptSvc.QueryInterface(Components.interfaces.nsIPromptService);
    }
    catch (e) {}
    return promptSvc;
  },

  get suggestedFileName()
  {
    var filename = "";
    var name = this.mContentDisposition;
    if (name) {
      const filenamePrefix = "filename=";
      var ix = name.indexOf(filenamePrefix);
      if (ix >= 0) {
        // Adjust ix to point to start of actual name
        ix += filenamePrefix.length;
        filename = name.substr(ix, name.length);
        if (filename != "") {
          ix = filename.lastIndexOf(";");
          if (ix > 0)
            filename = filename.substr(0, ix);
          
          filename = filename.replace(/^"|"$/g, "");
        }
      }
    }
    return filename;
  }  

};

const MODE_COMPLETE = 0;
const MODE_FILEONLY = 1;

function appendFiltersForContentType(aFilePicker, aContentType, aSaveMode)
{
  var bundle = getStringBundle();
    
  switch (aContentType) {
  case "text/html":
    if (aSaveMode == MODE_COMPLETE)
      aFilePicker.appendFilter(bundle.GetStringFromName("WebPageCompleteFilter"), "*.htm; *.html");
    aFilePicker.appendFilter(bundle.GetStringFromName("WebPageHTMLOnlyFilter"), "*.htm; *.html");
    if (aSaveMode == MODE_COMPLETE)
      aFilePicker.appendFilters(Components.interfaces.nsIFilePicker.filterText);
    break;
  default:
    var mimeInfo = getMIMEInfoForType(aContentType);
    if (mimeInfo) {
      var extCount = { };
      var extList = { };
      mimeInfo.GetFileExtensions(extCount, extList);
      
      var extString = "";
      for (var i = 0; i < extCount.value; ++i) {
        if (i > 0) 
          extString += "; "; // If adding more than one extension, separate by semi-colon
        extString += "*." + extList.value[i];
      }
      
      if (extCount.value > 0) {
        aFilePicker.appendFilter(mimeInfo.Description, extString);
      } else {
        aFilePicker.appendFilters(Components.interfaces.nsIFilePicker.filterAll);
      }        
    }
    else
      aFilePicker.appendFilters(Components.interfaces.nsIFilePicker.filterAll);
    break;
  }
} 

function getPostData()
{
  try {
    var sessionHistory = getWebNavigation().sessionHistory;
    entry = sessionHistory.getEntryAtIndex(sessionHistory.index, false);
    entry = entry.QueryInterface(Components.interfaces.nsISHEntry);
    return entry.postData;
  }
  catch (e) {
  }
  return null;
}

function getStringBundle()
{
  const bundleURL = "chrome://communicator/locale/contentAreaCommands.properties";
  
  const sbsContractID = "@mozilla.org/intl/stringbundle;1";
  const sbsIID = Components.interfaces.nsIStringBundleService;
  const sbs = Components.classes[sbsContractID].getService(sbsIID);
  
  const lsContractID = "@mozilla.org/intl/nslocaleservice;1";
  const lsIID = Components.interfaces.nsILocaleService;
  const ls = Components.classes[lsContractID].getService(lsIID);
  var appLocale = ls.GetApplicationLocale();
  return sbs.createBundle(bundleURL, appLocale);    
}

function makeWebBrowserPersist()
{
  const persistContractID = "@mozilla.org/embedding/browser/nsWebBrowserPersist;1";
  const persistIID = Components.interfaces.nsIWebBrowserPersist;
  return Components.classes[persistContractID].createInstance(persistIID);
}

function makeURL(aURL)
{
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                .getService(Components.interfaces.nsIIOService);
  return ioService.newURI(aURL, null, null);
  
}

function makeFilePicker()
{
  const fpContractID = "@mozilla.org/filepicker;1";
  const fpIID = Components.interfaces.nsIFilePicker;
  return Components.classes[fpContractID].createInstance(fpIID);
}

function getMIMEService()
{
  const mimeSvcContractID = "@mozilla.org/mime;1";
  const mimeSvcIID = Components.interfaces.nsIMIMEService;
  const mimeSvc = Components.classes[mimeSvcContractID].getService(mimeSvcIID);
  return mimeSvc;
}

function getMIMEInfoForExtension(aExtension)
{
  try {  
    return getMIMEService().GetFromExtension(aExtension);
  }
  catch (e) {
  }
  return null;
}

function getMIMEInfoForType(aMIMEType)
{
  try {  
    return getMIMEService().GetFromMIMEType(aMIMEType);
  }
  catch (e) {
  }
  return null;
}

function getDefaultFileName(aDefaultFileName, aNameFromHeaders, aDocumentURI, aDocument)
{
  if (aNameFromHeaders)
    // 1) Use the name suggested by the HTTP headers
    return validateFileName(aNameFromHeaders);

  try {
    var url = aDocumentURI.QueryInterface(Components.interfaces.nsIURL);
    if (url.fileName != "") {
      // 2) Use the actual file name, if present
      return unescape(url.fileName);
    }
  } catch (e) {
    try {
      // the file name might be non ASCII
      // try unescape again with a characterSet
      var textToSubURI = Components.classes["@mozilla.org/intl/texttosuburi;1"]
                                   .getService(Components.interfaces.nsITextToSubURI);
      var charset;
      if (aDocument)
        charset = aDocument.characterSet;
      else if (document.commandDispatcher.focusedWindow)
        charset = document.commandDispatcher.focusedWindow.document.characterSet;
      else
        charset = window._content.document.characterSet;
      return textToSubURI.unEscapeURIForUI(charset, url.fileName);
    } catch (e) {
      // This is something like a wyciwyg:, data:, and so forth
      // URI... no usable filename here.
    }
  }
  
  if (aDocument) {
    var docTitle = validateFileName(aDocument.title).replace(/^\s+|\s+$/g, "");

    if (docTitle != "") {
      // 3) Use the document title
      return docTitle;
    }
  }
  
  if (aDefaultFileName)
    // 4) Use the caller-provided name, if any
    return validateFileName(aDefaultFileName);

  try {
    if (aDocumentURI.host)
      // 5) Use the host.
      return aDocumentURI.host;
  } catch (e) {
    // Some files have no information at all, like Javascript generated pages
  }
  try {
    // 6) Use the default file name
    return getStringBundle().GetStringFromName("DefaultSaveFileName");
  } catch (e) {
    //in case localized string cannot be found
  }
  // 7) If all else fails, use "index"
  return "index";
}

function validateFileName(aFileName)
{
  var re = /[\/]+/g;
  if (navigator.appVersion.indexOf("Windows") != -1) {
    re = /[\\\/\|]+/g;
    aFileName = aFileName.replace(/[\"]+/g, "'");
    aFileName = aFileName.replace(/[\*\:\?]+/g, " ");
    aFileName = aFileName.replace(/[\<]+/g, "(");
    aFileName = aFileName.replace(/[\>]+/g, ")");
  }
  else if (navigator.appVersion.indexOf("Macintosh") != -1)
    re = /[\:\/]+/g;
  
  return aFileName.replace(re, "_");
}

function getNormalizedLeafName(aFile, aDefaultExtension)
{
  if (!aDefaultExtension)
    return aFile;
  
  // Fix up the file name we're saving to to include the default extension
  const stdURLContractID = "@mozilla.org/network/standard-url;1";
  const stdURLIID = Components.interfaces.nsIURL;
  var url = Components.classes[stdURLContractID].createInstance(stdURLIID);
  url.filePath = aFile;
  
  if (url.fileExtension != aDefaultExtension) {
    return aFile + "." + aDefaultExtension;
  }

  return aFile;
}

function getDefaultExtension(aFilename, aURI, aContentType)
{
  if (aContentType == "text/plain" || aContentType == "application/octet-stream" || aURI.scheme == "ftp")
    return "";   // temporary fix for bug 120327

  // This mirrors some code in nsExternalHelperAppService::DoContent
  // Use the filename first and then the URI if that fails
  
  var mimeInfo = getMIMEInfoForType(aContentType);

  // First try the extension from the filename
  const stdURLContractID = "@mozilla.org/network/standard-url;1";
  const stdURLIID = Components.interfaces.nsIURL;
  var url = Components.classes[stdURLContractID].createInstance(stdURLIID);
  url.filePath = aFilename;

  var ext = url.fileExtension;

  if (ext && mimeInfo && mimeInfo.ExtensionExists(ext)) {
    return ext;
  }
  
  // Well, that failed.  Now try the extension from the URI
  var urlext;
  try {
    url = aURI.QueryInterface(Components.interfaces.nsIURL);
    urlext = url.fileExtension;
  } catch (e) {
  }

  if (urlext && mimeInfo && mimeInfo.ExtensionExists(urlext)) {
    return urlext;
  }
  else {
    try {
      return mimeInfo.primaryExtension;
    }
    catch (e) {
      // Fall back on the extensions in the filename and URI for lack
      // of anything better.
      return ext || urlext;
    }
  }
}

function isDocumentType(aContentType)
{
  switch (aContentType) {
  case "text/html":
    return true;
  case "text/xml":
  case "application/xhtml+xml":
  case "application/xml":
    return false; // XXX Disables Save As Complete until it works for XML
  }
  return false;
}
