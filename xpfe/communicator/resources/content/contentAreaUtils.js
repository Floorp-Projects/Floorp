/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <ben@netscape.com> (Save File)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * Determine whether or not a given focused DOMWindow is in the content
 * area.
 **/
function isDocumentFrame(aFocusedWindow)
{
  var contentFrames = _content.frames;
  if (contentFrames.length) {
    for (var i = 0; i < contentFrames.length; ++i) {
      if (aFocusedWindow == contentFrames[i])
        return true;
    }
  }
  return false;
}

function urlSecurityCheck(url, doc) 
{
  // URL Loading Security Check
  var focusedWindow = doc.commandDispatcher.focusedWindow;
  var sourceWin = isDocumentFrame(focusedWindow) ? focusedWindow.location.href : focusedWindow._content.location.href;
  const nsIScriptSecurityManager = Components.interfaces.nsIScriptSecurityManager;
  var secMan = Components.classes["@mozilla.org/scriptsecuritymanager;1"].getService().
                    QueryInterface(nsIScriptSecurityManager);
  try {
    secMan.checkLoadURIStr(sourceWin, url, nsIScriptSecurityManager.STANDARD);
  } catch (e) {
      throw "Load of " + url + " denied.";
  }
}

function getReferrer(doc)
{
  var focusedWindow = doc.commandDispatcher.focusedWindow;
  var sourceURL =
    isDocumentFrame(focusedWindow) ? focusedWindow.location.href : focusedWindow._content.location.href;
  try {
    var uri = Components.classes["@mozilla.org/network/standard-url;1"].createInstance(Components.interfaces.nsIURI);
    uri.spec = sourceURL;
    return uri;
  } catch (e) {
    return null;
  }
}

function openNewWindowWith(url) 
{
  urlSecurityCheck(url, document);
  var newWin;
  var wintype = document.firstChild.getAttribute('windowtype');
  var referrer = getReferrer(document);

  // if and only if the current window is a browser window and it has a document with a character
  // set, then extract the current charset menu setting from the current document and use it to
  // initialize the new browser window...
  if (window && (wintype == "navigator:browser") &&
    window._content && window._content.document) {
    var DocCharset = window._content.document.characterSet;
    var charsetArg = "charset="+DocCharset;

    //we should "inherit" the charset menu setting in a new window
    newWin = window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url, charsetArg, true, referrer );
  }
  else { // forget about the charset information.
    newWin = window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url, null, true, referrer );
  }
}

function openNewTabWith(url) 
{
  urlSecurityCheck(url, document);
  var wintype = document.firstChild.getAttribute('windowtype');
  var referrer = getReferrer(document);

  if (window && (wintype == "navigator:browser")) {
    var browser = getBrowser();
    var t = browser.addTab(url, referrer); // open link in new tab
    if (pref && !pref.getBoolPref("browser.tabs.loadInBackground"))
      browser.selectedTab = t;
  }
}

function findParentNode(node, parentNode)
{
  if (node && node.nodeType == Node.TEXT_NODE) {
    node = node.parentNode;
  }
  while (node) {
    var nodeName = node.localName;
    if (!nodeName)
      return null;
    nodeName = nodeName.toLowerCase();
    if (nodeName == "body" || nodeName == "html" ||
        nodeName == "#document") {
      return null;
    }
    if (nodeName == parentNode)
      return node;
    node = node.parentNode;
  }
  return null;
}

// Clientelle: (Make sure you don't break any of these)
//  - File    ->  Save Page/Frame As...
//  - Context ->  Save Page/Frame As...
//  - Context ->  Save Link As...
//  - Context ->  Save Image As...
//  - Shift-Click Save Link As
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
// - A linked document using shift-click Save Link As...
//
function saveURL(aURL, aFileName, aFilePickerTitleKey, aShouldBypassCache)
{
  saveInternal(aURL, null, aFileName, aFilePickerTitleKey, aShouldBypassCache);
}

function saveFrameDocument()
{
  var focusedWindow = document.commandDispatcher.focusedWindow;
  if (isDocumentFrame(focusedWindow))
    saveDocument(focusedWindow.document);
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

  var fp = makeFilePicker();
  var titleKey = aData.filePickerTitle || "SaveLinkTitle";
  var bundle = getStringBundle();
  fp.init(window, bundle.GetStringFromName(titleKey), 
          Components.interfaces.nsIFilePicker.modeSave);


  var isDocument = aData.document != null && isDocumentType(contentType);
  appendFiltersForContentType(fp, aSniffer.contentType,
                              isDocument ? MODE_COMPLETE : MODE_FILEONLY);  

  const prefSvcContractID = "@mozilla.org/preferences-service;1";
  const prefSvcIID = Components.interfaces.nsIPrefService;                              
  var prefs = Components.classes[prefSvcContractID].getService(prefSvcIID).getBranch("browser.download.");
  
  const nsILocalFile = Components.interfaces.nsILocalFile;
  try {
    fp.displayDirectory = prefs.getComplexValue("dir", nsILocalFile);
  }
  catch (e) {
  }

  if (isDocument) {
    try {
      fp.filterIndex = prefs.getIntPref("save_converter_index");
    }
    catch (e) {
    }
  }
  
  // Determine what the 'default' string to display in the File Picker dialog 
  // should be. 
  var defaultFileName = getDefaultFileName(aData.fileName, 
                                           aSniffer.suggestedFileName, 
                                           aSniffer.uri);
  fp.defaultString = getNormalizedLeafName(defaultFileName, contentType);
  
  if (fp.show() == Components.interfaces.nsIFilePicker.returnCancel || !fp.file)
    return;
    
  if (isDocument) 
    prefs.setIntPref("save_converter_index", fp.filterIndex);
  var directory = fp.file.parent.QueryInterface(nsILocalFile);
  prefs.setComplexValue("dir", nsILocalFile, directory);
    
  fp.file.leafName = validateFileName(fp.file.leafName);
  fp.file.leafName = getNormalizedLeafName(fp.file.leafName, contentType);
  
  // If we're saving a document, and are saving either in complete mode or 
  // as converted text, pass the document to the web browser persist component.
  // If we're just saving the HTML (second option in the list), send only the URI.
  var source = (isDocument && fp.filterIndex != 1) ? aData.document : aSniffer.uri;
  
  var persistArgs = {
    source      : source,
    contentType : (isDocument && fp.filterIndex == 2) ? "text/plain" : contentType,
    target      : fp.file,
    postData    : isDocument ? getPostData() : null,
    bypassCache : aData.bypassCache
  };
  
  openDialog("chrome://global/content/nsProgressDlg.xul", "", 
              "chrome,titlebar,minimizable,dialog=yes", 
              makeWebBrowserPersist(), persistArgs);
}

function nsHeaderSniffer(aURL, aCallback, aData)
{
  this.mCallback = aCallback;
  this.mData = aData;
  
  const stdURLContractID = "@mozilla.org/network/standard-url;1";
  const stdURLIID = Components.interfaces.nsIURI;
  this.uri = Components.classes[stdURLContractID].createInstance(stdURLIID);
  this.uri.spec = aURL;
  
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
  onStartRequest: function (aRequest, aContext) { },
  
  onStopRequest: function (aRequest, aContext, aStatus) {
    try {
      if (aStatus == 0) { // NS_BINDING_SUCCEEDED, so there's something there
        var linkChecker = aRequest.QueryInterface(Components.interfaces.nsIURIChecker);
        var channel = linkChecker.baseRequest.QueryInterface(Components.interfaces.nsIChannel);
        this.contentType = channel.contentType;
        try {
          var httpChannel = channel.QueryInterface(Components.interfaces.nsIHttpChannel);
          this.mContentDisposition = httpChannel.getResponseHeader("content-disposition");
        }
        catch (e) {
        }
      }
      else {
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
            this.contentType = getMIMEInfoForExtension(ext).MIMEType;
          }
        }
        catch (e) {
          // Not much we can do here.  Give up.
        }
      }
    }
    this.mCallback(this, this.mData);
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
    aFilePicker.appendFilter(bundle.GetStringFromName("TextOnlyFilter"), "*.txt");
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
        aFilePicker.appendFilter(bundle.GetStringFromName("AllFilesFilter"), "*.*");
      }        
    }
    else
      aFilePicker.appendFilter(bundle.GetStringFromName("AllFilesFilter"), "*.*");
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

function makeFilePicker()
{
  const fpContractID = "@mozilla.org/filepicker;1";
  const fpIID = Components.interfaces.nsIFilePicker;
  return Components.classes[fpContractID].createInstance(fpIID);
}

function makeTempFile()
{
  const mimeTypes = "TmpD";
  const flContractID = "@mozilla.org/file/directory_service;1";
  const flIID = Components.interfaces.nsIProperties;
  var fileLocator = Components.classes[flContractID].getService(flIID);
  var tempFile = fileLocator.get(mimeTypes, Components.interfaces.nsIFile);
  tempFile.append("~sav" + Math.floor(Math.random() * 1000) + ".tmp");
  return tempFile;
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
    return validateFileName(aNameFromHeaders);  // 1) Use the name suggested by the HTTP headers

  var url = aDocumentURI.QueryInterface(Components.interfaces.nsIURL);
  if (url.fileName != "")
    return url.fileName;                        // 2) Use the actual file name, if present
  
  if (aDocument && aDocument.title != "") 
    return validateFileName(aDocument.title)    // 3) Use the document title

  if (aDefaultFileName)
    return validateFileName(aDefaultFileName);  // 4) Use the caller-provided name, if any

  return aDocumentURI.host;                     // 5) Use the host.
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

function getNormalizedLeafName(aFile, aContentType)
{
  // Fix up the file name we're saving to so that if the user enters
  // no extension, an appropriate one is appended. 
  var leafName = aFile;

  var mimeInfo = getMIMEInfoForType(aContentType);
  if (mimeInfo) {
    var extCount = { };
    var extList = { };
    mimeInfo.GetFileExtensions(extCount, extList);

    const stdURLContractID = "@mozilla.org/network/standard-url;1";
    const stdURLIID = Components.interfaces.nsIURI;
    var uri = Components.classes[stdURLContractID].createInstance(stdURLIID);
    var url = uri.QueryInterface(Components.interfaces.nsIURL); 
    url.filePath = aFile;
    
    if (aContentType == "text/html") {
      if ((url.fileExtension && 
           url.fileExtension != "htm" && url.fileExtension != "html") ||
          (!url.fileExtension))
        return leafName + ".html";
    }

    if (!url.fileExtension)
      return leafName + "." + extList.value[0];
  }
 
  return leafName;
}

function isDocumentType(aContentType)
{
  switch (aContentType) {
  case "text/html":
  case "text/xml":
  case "application/xhtml+xml":
    return true;
  }
  return false;
}

