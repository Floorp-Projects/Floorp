/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * Determine whether or not a given focused DOMWindow is in the content
 * area.
 **/
function isContentFrame(aFocusedWindow)
{
  if (!aFocusedWindow)
    return false;

  var focusedTop = Components.lookupMethod(aFocusedWindow, 'top')
                             .call(aFocusedWindow);

  return (focusedTop == window.content);
}

function urlSecurityCheck(url, doc)
{
  // URL Loading Security Check
  var focusedWindow = doc.commandDispatcher.focusedWindow;
  var sourceURL = getContentFrameURI(focusedWindow);
  const nsIScriptSecurityManager = Components.interfaces.nsIScriptSecurityManager;
  var secMan = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                         .getService(nsIScriptSecurityManager);
  try {
    secMan.checkLoadURIStr(sourceURL, url, nsIScriptSecurityManager.STANDARD);
  } catch (e) {
    throw "Load of " + url + " denied.";
  }
}

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
    return makeURL(sourceURL);
  } catch (e) {
    return null;
  }
}

function openNewWindowWith(url, sendReferrer)
{
  urlSecurityCheck(url, document);

  // if and only if the current window is a browser window and it has a document with a character
  // set, then extract the current charset menu setting from the current document and use it to
  // initialize the new browser window...
  var charsetArg = null;
  var wintype = document.firstChild.getAttribute('windowtype');
  if (wintype == "navigator:browser")
    charsetArg = "charset=" + window._content.document.characterSet;

  var referrer = sendReferrer ? getReferrer(document) : null;
  window.openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no", url, charsetArg, referrer);
}

function openTopBrowserWith(url)
{
  urlSecurityCheck(url, document);

  var windowMediator = Components.classes["@mozilla.org/appshell/window-mediator;1"].getService(Components.interfaces.nsIWindowMediator);
  var browserWin = windowMediator.getMostRecentWindow("navigator:browser");

  // if there's an existing browser window, open this url in one
  if (browserWin) {
    browserWin.getBrowser().loadURI(url); // Just do a normal load.
    browserWin.content.focus();
  }
  else
    window.openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no", url, null, null);
}

function openNewTabWith(url, sendReferrer, reverseBackgroundPref)
{
  var browser;
  try {
    // if we're running in a browser window, this should work
    //
    browser = getBrowser();

  } catch (ex if ex instanceof ReferenceError) {

    // must be running somewhere else (eg mailnews message pane); need to
    // find a browser window first
    //
    var windowMediator =
      Components.classes["@mozilla.org/appshell/window-mediator;1"]
      .getService(Components.interfaces.nsIWindowMediator);

    var browserWin = windowMediator.getMostRecentWindow("navigator:browser");

    // if there's no existing browser window, then, as long as
    // we are allowed to, open this url in one and return
    //
    if (!browserWin) {
      urlSecurityCheck(url, document);
      window.openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no",
                        url, null, referrer);
      return;
    }

    // otherwise, get the existing browser object
    //
    browser = browserWin.getBrowser();
  }

  // Get the XUL document that the browser is actually contained in.
  // This is needed if we are trying to load a URL from a non-navigator
  // window such as the JS Console.
  var browserDocument = browser.ownerDocument;

  urlSecurityCheck(url, browserDocument);

  var referrer = sendReferrer ? getReferrer(browserDocument) : null;

  // As in openNewWindowWith(), we want to pass the charset of the
  // current document over to a new tab.
  var wintype = browserDocument.firstChild.getAttribute('windowtype');
  var originCharset;
  if (wintype == "navigator:browser") {
    originCharset = window._content.document.characterSet;
  }

  // open link in new tab
  var tab = browser.addTab(url, referrer, originCharset);
  if (pref) {
    var loadInBackground = pref.getBoolPref("browser.tabs.loadInBackground");
    if (reverseBackgroundPref)
      loadInBackground = !loadInBackground;

    if (!loadInBackground)
      browser.selectedTab = tab;
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
  if (isContentFrame(focusedWindow))
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
  var urlExt = null;
  // Are we allowed to decode?
  try {
    const helperAppService =
      Components.classes["@mozilla.org/uriloader/external-helper-app-service;1"].
        getService(Components.interfaces.nsIExternalHelperAppService);
    var url = aSniffer.uri.QueryInterface(Components.interfaces.nsIURL);
    urlExt = url.fileExtension;
    if (contentEncodingType &&
        helperAppService.applyDecodingForExtension(urlExt,
                                                   contentEncodingType)) {
      shouldDecode = true;
    }
  }
  catch (e) {
  }

  var fp = makeFilePicker();
  var titleKey = aData.filePickerTitle || "SaveLinkTitle";
  var bundle = getStringBundle();
  fp.init(window, bundle.GetStringFromName(titleKey),
          Components.interfaces.nsIFilePicker.modeSave);

  var saveMode = GetSaveModeForContentType(contentType);
  var isDocument = aData.document != null && saveMode;
  if (!isDocument && !shouldDecode && contentEncodingType) {
    // The data is encoded, we are not going to decode it, and this is not a
    // document save so we won't be doing a "save as, complete" (which would
    // break if we reset the type here).  So just set our content type to
    // correspond to the outermost encoding so we get extensions and the like
    // right.
    contentType = contentEncodingType;
  }

  appendFiltersForContentType(fp, contentType, urlExt,
                              isDocument ? saveMode : SAVEMODE_FILEONLY);

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
                                           aSniffer.uri,
                                           aData.document);
  var defaultExtension = getDefaultExtension(defaultFileName, aSniffer.uri, contentType);
  fp.defaultExtension = defaultExtension;
  fp.defaultString = getNormalizedLeafName(defaultFileName, defaultExtension);

  if (fp.show() == Components.interfaces.nsIFilePicker.returnCancel || !fp.file)
    return;

  if (isDocument)
    prefs.setIntPref("save_converter_index", fp.filterIndex);
  var directory = fp.file.parent.QueryInterface(nsILocalFile);
  prefs.setComplexValue("dir", nsILocalFile, directory);

  fp.file.leafName = validateFileName(fp.file.leafName);

  // XXX We depend on the following holding true in appendFiltersForContentType():
  // If we should save as a complete page, the filterIndex is 0.
  // If we should save as text, the filterIndex is 2.
  var useSaveDocument = isDocument &&
                        ((saveMode & SAVEMODE_COMPLETE_DOM && fp.filterIndex == 0) ||
                         (saveMode & SAVEMODE_COMPLETE_TEXT && fp.filterIndex == 2));

  // If we're saving a document, and are saving either in complete mode or
  // as converted text, pass the document to the web browser persist component.
  // If we're just saving the HTML (second option in the list), send only the URI.
  var source = useSaveDocument ? aData.document : aSniffer.uri;
  var persistArgs = {
    source      : source,
    contentType : (useSaveDocument && fp.filterIndex == 2) ? "text/plain" : contentType,
    target      : makeFileURL(fp.file),
    postData    : isDocument ? getPostData() : null,
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

  if (useSaveDocument) {
    // Saving a Document, not a URI:
    var filesFolder = null;
    if (persistArgs.contentType != "text/plain") {
      // Create the local directory into which to save associated files.
      filesFolder = fp.file.clone();

      var nameWithoutExtension = filesFolder.leafName.replace(/\.[^.]*$/, "");
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
    else {
      encodingFlags |= nsIWBP.ENCODE_FLAGS_ENCODE_BASIC_ENTITIES;
    }

    const kWrapColumn = 80;
    dl.init(aSniffer.uri, persistArgs.target, null, null, null, persist);
    persist.saveDocument(persistArgs.source, persistArgs.target, filesFolder,
                         persistArgs.contentType, encodingFlags, kWrapColumn);
  } else {
    dl.init(source, persistArgs.target, null, null, null, persist);
    var referer = getReferrer(document);
    persist.saveURI(source, null, referer, persistArgs.postData, null, persistArgs.target);
  }
}

function nsHeaderSniffer(aURL, aCallback, aData)
{
  this.mCallback = aCallback;
  this.mData = aData;

  this.uri = makeURL(aURL);

  this.linkChecker = Components.classes["@mozilla.org/network/urichecker;1"]
    .createInstance(Components.interfaces.nsIURIChecker);
  this.linkChecker.init(this.uri);

  var flags;
  if (aData.bypassCache) {
    flags = Components.interfaces.nsIRequest.LOAD_BYPASS_CACHE;
  } else {
    flags = Components.interfaces.nsIRequest.LOAD_FROM_CACHE;
  }
  this.linkChecker.loadFlags = flags;

  // Set referrer, ignore errors
  try {
    var referrer = getReferrer(document);
    this.linkChecker.baseChannel.QueryInterface(Components.interfaces.nsIHttpChannel).referrer = referrer;
  }
  catch (ex) { }

  this.linkChecker.asyncCheck(this, null);
}

nsHeaderSniffer.prototype = {

  // ---------- nsISupports methods ----------
  QueryInterface: function (iid) {
    if (iid.equals(Components.interfaces.nsIRequestObserver) ||
        iid.equals(Components.interfaces.nsISupports) ||
        iid.equals(Components.interfaces.nsIInterfaceRequestor))
      return this;

    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
    return null;
  },

  // ---------- nsIInterfaceRequestor methods ----------
  getInterface : function(iid) {
    if (iid.equals(Components.interfaces.nsIAuthPrompt)) {
      // use the window watcher service to get a nsIAuthPrompt impl
      var ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                         .getService(Components.interfaces.nsIWindowWatcher);
      return ww.getNewAuthPrompter(window);
    }
    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
    return null;
  },

  // ---------- nsIRequestObserver methods ----------
  onStartRequest: function (aRequest, aContext) { },

  onStopRequest: function (aRequest, aContext, aStatus) {
    try {
      if (aStatus == 0) { // NS_BINDING_SUCCEEDED, so there's something there
        var linkChecker = aRequest.QueryInterface(Components.interfaces.nsIURIChecker);
        var channel = linkChecker.baseChannel;
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
          if (encodingEnumerator && encodingEnumerator.hasMore()) {
            try {
              this.contentEncodingType = encodingEnumerator.getNext();
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
        var type = getMIMETypeForURI(this.uri);
        if (type)
          this.contentType = type;
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
    var fileName = "";

    if ("mContentDisposition" in this) {
      const mhpContractID = "@mozilla.org/network/mime-hdrparam;1"
      const mhpIID = Components.interfaces.nsIMIMEHeaderParam;
      const mhp = Components.classes[mhpContractID].getService(mhpIID);
      var dummy = { value: null }; // To make JS engine happy.
      var charset = getCharsetforSave(null);

      try {
        fileName = mhp.getParameter(this.mContentDisposition, "filename", charset, true, dummy);
      }
      catch (e) {
        try {
          fileName = mhp.getParameter(this.mContentDisposition, "name", charset, true, dummy);
        }
        catch (e) {
        }
      }
    }

    fileName = fileName.replace(/^"|"$/g, "");
    return fileName;
  }
};

// We have no DOM, and can only save the URL as is.
const SAVEMODE_FILEONLY      = 0x00;
// We have a DOM and can save as complete.
const SAVEMODE_COMPLETE_DOM  = 0x01;
// We have a DOM which we can serialize as text.
const SAVEMODE_COMPLETE_TEXT = 0x02;

// If we are able to save a complete DOM, the 'save as complete' filter
// must be the first filter appended.  The 'save page only' counterpart
// must be the second filter appended.  And the 'save as complete text'
// filter must be the third filter appended.
function appendFiltersForContentType(aFilePicker, aContentType, aFileExtension, aSaveMode)
{
  var bundle = getStringBundle();
  // The bundle name for saving only a specific content type.
  var bundleName;
  // The corresponding filter string for a specific content type.
  var filterString;

  // XXX all the cases that are handled explicitly here MUST be handled
  // in GetSaveModeForContentType to return a non-fileonly filter.
  switch (aContentType) {
  case "text/html":
    bundleName   = "WebPageHTMLOnlyFilter";
    filterString = "*.htm; *.html";
    break;

  case "application/xhtml+xml":
    bundleName   = "WebPageXHTMLOnlyFilter";
    filterString = "*.xht; *.xhtml";
    break;

  case "text/xml":
  case "application/xml":
    bundleName   = "WebPageXMLOnlyFilter";
    filterString = "*.xml";
    break;

  default:
    if (aSaveMode != SAVEMODE_FILEONLY) {
      throw "Invalid save mode for type '" + aContentType + "'";
    }

    var mimeInfo = getMIMEInfoForType(aContentType, aFileExtension);
    if (mimeInfo) {

      var extEnumerator = mimeInfo.getFileExtensions();

      var extString = "";
      while (extEnumerator.hasMore()) {
        var extension = extEnumerator.getNext();
        if (extString)
          extString += "; ";    // If adding more than one extension,
                                // separate by semi-colon
        extString += "*." + extension;
      }

      if (extString) {
        aFilePicker.appendFilter(mimeInfo.description, extString);
      }
    }

    break;
  }

  if (aSaveMode & SAVEMODE_COMPLETE_DOM) {
    aFilePicker.appendFilter(bundle.GetStringFromName("WebPageCompleteFilter"), filterString);
    // We should always offer a choice to save document only if
    // we allow saving as complete.
    aFilePicker.appendFilter(bundle.GetStringFromName(bundleName), filterString);
  }

  if (aSaveMode & SAVEMODE_COMPLETE_TEXT) {
    aFilePicker.appendFilters(Components.interfaces.nsIFilePicker.filterText);
  }

  // Always append the all files (*) filter
  aFilePicker.appendFilters(Components.interfaces.nsIFilePicker.filterAll);
}

function getPostData()
{
  try {
    var sessionHistory = getWebNavigation().sessionHistory;
    return sessionHistory.getEntryAtIndex(sessionHistory.index, false)
                         .QueryInterface(Components.interfaces.nsISHEntry)
                         .postData;
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
  var appLocale = ls.getApplicationLocale();
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

function makeFileURL(aFile)
{
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                .getService(Components.interfaces.nsIIOService);
  return ioService.newFileURI(aFile);
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

function getMIMETypeForURI(aURI)
{
  try {
    return getMIMEService().getTypeFromURI(aURI);
  }
  catch (e) {
  }
  return null;
}

function getMIMEInfoForType(aMIMEType, aExtension)
{
  try {
    return getMIMEService().getFromTypeAndExtension(aMIMEType, aExtension);
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
      return validateFileName(decodeURIComponent(url.fileName));
    }
  } catch (e) {
    try {
      // the file name might be non ASCII
      // try unescape again with a characterSet
      var textToSubURI = Components.classes["@mozilla.org/intl/texttosuburi;1"]
                                   .getService(Components.interfaces.nsITextToSubURI);
      var charset = getCharsetforSave(aDocument);
      return validateFileName(textToSubURI.unEscapeURIForUI(charset, url.fileName));
    } catch (e) {
      // This is something like a wyciwyg:, data:, and so forth
      // URI... no usable filename here.
    }
  }

  if (aDocument) {
    var docTitle = GenerateValidFilename(aDocument.title, "");

    if (docTitle) {
      // 3) Use the document title
      return docTitle;
    }
  }

  if (aDefaultFileName)
    // 4) Use the caller-provided name, if any
    return validateFileName(aDefaultFileName);

  // 5) If this is a directory, use the last directory name
  var path = aDocumentURI.path.match(/\/([^\/]+)\/$/);
  if (path && path.length > 1) {
      return validateFileName(path[1]);
  }

  try {
    if (aDocumentURI.host)
      // 6) Use the host.
      return aDocumentURI.host;
  } catch (e) {
    // Some files have no information at all, like Javascript generated pages
  }
  try {
    // 7) Use the default file name
    return getStringBundle().GetStringFromName("DefaultSaveFileName");
  } catch (e) {
    //in case localized string cannot be found
  }
  // 8) If all else fails, use "index"
  return "index";
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

  // First try the extension from the filename
  const stdURLContractID = "@mozilla.org/network/standard-url;1";
  const stdURLIID = Components.interfaces.nsIURL;
  var url = Components.classes[stdURLContractID].createInstance(stdURLIID);
  url.filePath = aFilename;

  var ext = url.fileExtension;

  // This mirrors some code in nsExternalHelperAppService::DoContent
  // Use the filename first and then the URI if that fails

  var mimeInfo = getMIMEInfoForType(aContentType, ext);

  if (ext && mimeInfo && mimeInfo.extensionExists(ext)) {
    return ext;
  }

  // Well, that failed.  Now try the extension from the URI
  var urlext;
  try {
    url = aURI.QueryInterface(Components.interfaces.nsIURL);
    urlext = url.fileExtension;
  } catch (e) {
  }

  if (urlext && mimeInfo && mimeInfo.extensionExists(urlext)) {
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

function GetSaveModeForContentType(aContentType)
{
  var saveMode = SAVEMODE_FILEONLY;
  switch (aContentType) {
  case "text/html":
  case "application/xhtml+xml":
    saveMode |= SAVEMODE_COMPLETE_TEXT;
    // Fall through
  case "text/xml":
  case "application/xml":
    saveMode |= SAVEMODE_COMPLETE_DOM;
    break;
  }

  return saveMode;
}

function getCharsetforSave(aDocument)
{
  if (aDocument)
    return aDocument.characterSet;

  if (document.commandDispatcher.focusedWindow)
    return document.commandDispatcher.focusedWindow.document.characterSet;

  return  window._content.document.characterSet;
}
