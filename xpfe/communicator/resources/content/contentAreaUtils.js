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

function openNewWindowWith(url) 
{
  urlSecurityCheck(url, document);
  var newWin;
  var wintype = document.firstChild.getAttribute('windowtype');

  // if and only if the current window is a browser window and it has a document with a character
  // set, then extract the current charset menu setting from the current document and use it to
  // initialize the new browser window...
  if (window && (wintype == "navigator:browser") &&
    window._content && window._content.document) {
    var DocCharset = window._content.document.characterSet;
    var charsetArg = "charset="+DocCharset;

    //we should "inherit" the charset menu setting in a new window
    newWin = window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url, charsetArg, true );
  }
  else { // forget about the charset information.
    newWin = window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url, null, true );
  }
}

function openNewTabWith(url) 
{
  urlSecurityCheck(url, document);
  var wintype = document.firstChild.getAttribute('windowtype');

  if (window && (wintype == "navigator:browser")) {
    var browser = getBrowser();
    var t = browser.addTab(url); // open link in new tab
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
function saveURL(aURL, aFileName, aFilePickerTitleKey)
{
  saveInternal(aURL, null, aFileName, aFilePickerTitleKey);
}

function saveDocument(aDocument)
{
  if (aDocument) 
    saveInternal(aDocument.location.href, aDocument);
  else
    saveInternal(_content.location.href, aDocument);
}

function saveInternal(aURL, aDocument, 
                      aFileName, aFilePickerTitleKey)
{
  var data = {
    fileName: aFileName,
    filePickerTitle: aFilePickerTitleKey,
    document: aDocument
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
  var prefs = Components.classes[prefSvcContractID].getService(prefSvcIID).getBranch("browser.download");
  
  const nsILocalFile = Components.interfaces.nsILocalFile;
  try {
    fp.downloadDirectory = prefs.getComplexValue("dir", nsILocalFile);
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
  fp.defaultString = getDefaultFileName(aData.fileName, 
                                        aSniffer.suggestedFileName, 
                                        aSniffer.uri, aData.document);
  
  if (fp.show() == Components.interfaces.nsIFilePicker.returnCancel || !fp.file)
    return;
    
  if (isDocument) 
    prefs.setIntPref("save_converter_index", fp.filterIndex);
  prefs.setComplexValue("dir", nsILocalFile, fp.file);
    
  fp.file.leafName = validateFileName(fp.file.leafName);
  const kAllFilesFilterIndex = 1;
  if (isDocument || fp.filterIndex != kAllFilesFilterIndex) 
    fp.file.leafName = getNormalizedLeafName(fp.file.leafName, contentType);
  
// XXX turn this on when Adam lands the ability to save as a specific content
//     type
//var contentType = fp.filterIndex == 2 ? "text/unicode" : "text/html";
  var source = (isDocument && fp.filterIndex == 0) ? aData.document : aSniffer.uri;
  
  var persistArgs = {
    source    : source,
//  XXX turn this on when Adam lands the ability to save as a specific content
//      type
//  contentType : fp.filterIndex == 2 ? "text/unicode" : "text/html";
    target    : fp.file,
    postData  : getPostData()
  };
  
  openDialog("chrome://global/content/nsProgressDlg.xul", "", 
              "chrome,titlebar,minizable,dialog=yes", 
              makeWebBrowserPersist(), persistArgs);
}

function nsHeaderSniffer(aURL, aCallback, aData)
{
  this.mPersist = makeWebBrowserPersist();
  this.mCallback = aCallback;
  this.mData = aData;
  
  this.mPersist.progressListener = this;

  this.mTempFile = makeTempFile();
  while (this.mTempFile.exists())
    this.mTempFile = makeTempFile();    
  
  const stdURLContractID = "@mozilla.org/network/standard-url;1";
  const stdURLIID = Components.interfaces.nsIURI;
  this.uri = Components.classes[stdURLContractID].createInstance(stdURLIID);
  this.uri.spec = aURL;
  
  this.mPersist.saveURI(this.uri, null, this.mTempFile);
}

nsHeaderSniffer.prototype = {
  onStateChange: function (aWebProgress, aRequest, aStateFlags, aStatus)
  {
    if (aStateFlags & Components.interfaces.nsIWebProgressListener.STATE_START) {
      try {
        var channel = aRequest.QueryInterface(Components.interfaces.nsIChannel);
        this.contentType = channel.contentType;
        try {
          var httpChannel = aRequest.QueryInterface(Components.interfaces.nsIHttpChannel);
          this.mContentDisposition = httpChannel.getResponseHeader("content-disposition");
        }
        catch (e) {
        }
        this.mPersist.cancelSave();
        if (this.mTempFile.exists())
          this.mTempFile.remove(false);
        this.mCallback(this, this.mData);
      }
      catch (e) {
      }
    }
  },
  onLocationChange: function (aWebProgress, aRequest, aLocation) { },
  onStatusChange: function (aWebProgress, aRequest, aStatus, aMessage) { },
  onSecurityChange: function (aWebProgress, aRequest, aState) { },
  onProgressChange: function (aWebProgress, aRequest, aCurSelfProgress, 
                              aMaxSelfProgress, aCurTotalProgress, 
                              aMaxTotalProgress) { },
  
  get suggestedFileName()
  {
    var filename = "";
    var name = this.mContentDisposition;
    if (name) {
      var ix = name.indexOf("filename=");
      if (ix > 0) {
        filename = name.substr(ix, name.length);
        if (filename != "") {
          ix = filename.lastIndexOf(";");
          if (ix > 0)
            filename = filename.substr(0, ix);
          // XXX strip out quotes;
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
    // XXX waiting for fix for 110135 to land
    // aFilePicker.appendFilter(bundle.GetStringFromName("TextOnlyFilter"), "*.txt");
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
      
      aFilePicker.appendFilter(mimeInfo.Description, extString);
    }
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

function getMIMEInfoForType(aMIMEType)
{
  const mimeSvcCID = "{03af31da-3109-11d3-8cd0-0060b0fc14a3}";
  const mimeSvcIID = Components.interfaces.nsIMIMEService;
  const mimeSvc = Components.classesByID[mimeSvcCID].getService(mimeSvcIID);

  try {  
    return mimeSvc.GetFromMIMEType(aMIMEType);
  }
  catch (e) {
  }
  return null;
}

function getDefaultFileName(aDefaultFileName, aNameFromHeaders, aDocumentURI, aDocument)
{
  if (aNameFromHeaders)
    return validateFileName(aNameFromHeaders);  // 1) Use the name suggested by the HTTP headers

  if (aDocument && aDocument.title != "") 
    return validateFileName(aDocument.title)    // 2) Use the document title

  var url = aDocumentURI.QueryInterface(Components.interfaces.nsIURL);
  if (url.fileName != "")
    return url.fileName;                        // 4) Use the actual file name, if present
  
  if (aDefaultFileName)
    return validateFileName(aDefaultFileName);  // 3) Use the caller-provided name, if any

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

