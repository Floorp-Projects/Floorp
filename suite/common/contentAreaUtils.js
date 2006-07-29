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

  var focusedTop = new XPCNativeWrapper(aFocusedWindow, 'top').top;

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
  return new XPCNativeWrapper(contentFrame, "location").location.href;
}

function getReferrer(doc)
{
  var focusedWindow = doc.commandDispatcher.focusedWindow;
  var sourceURL = getContentFrameURI(focusedWindow);

  try {
    return makeURI(sourceURL);
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
    charsetArg = "charset=" + window.content.document.characterSet;

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
    originCharset = window.content.document.characterSet;
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
  internalSave(aURL, null, aFileName, aShouldBypassCache,
               aFilePickerTitleKey, null);
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
    internalSave(aDocument.location.href, aDocument,
                 null, false, null, null);
  else
    internalSave(content.location.href,  null,
                 null, false, null, null);
}

/**
 * internalSave: Used when saving a document or URL. This method:
 *  - Determines a local target filename to use (unless parameter
 *    aChosenData is non-null)
 *  - Determines content-type if possible
 *  - Prompts the user to confirm the destination filename and save mode
 *    (content-type affects this)
 *  - Creates a 'Persist' object (which will perform the saving in the
 *    background) and then starts it.
 *
 * @param aURL The String representation of the URL of the document being saved
 * @param aDocument The document to be saved
 * @param aDefaultFileName The caller-provided suggested filename if we don't
 *        find a better one
 * @param aShouldBypassCache If true, the document will always be refetched
 *        from the server
 * @param aFilePickerTitleKey Alternate title for the file picker
 * @param aChosenData If non-null this contains an instance of object AutoChosen
 *        (see below) which holds pre-determined data so that the user does not
 *        need to be prompted for a target filename.
 */
function internalSave(aURL, aDocument, aDefaultFileName, aShouldBypassCache,
                      aFilePickerTitleKey, aChosenData)
{
  // Note: aDocument == null when this code is used by save-link-as...
  var contentType = (aDocument ? aDocument.contentType : null);

  // Find the URI object for aURL and the FileName/Extension to use when saving.
  // FileName/Extension will be ignored if aChosenData supplied.
  var fileInfo = new FileInfo(aDefaultFileName);
  if (!aChosenData)
    initFileInfo(fileInfo, aURL, aDocument, contentType);

  var saveMode = GetSaveModeForContentType(contentType);
  var isDocument = aDocument != null && saveMode != SAVEMODE_FILEONLY;

  // Show the file picker that allows the user to confirm the target filename:
  var prefs = getPrefsBrowserDownload("browser.download.");
  var fpParams = {
    fp: makeFilePicker(),
    prefs: prefs,
    userDirectory: getUserDownloadDir(prefs),
    fpTitleKey: aFilePickerTitleKey,
    isDocument: isDocument,
    fileInfo: fileInfo,
    contentType: contentType,
    saveMode: saveMode
  };
  // If aChosenData is null then the file picker is shown.
  if (!aChosenData) {
    if (!poseFilePicker(fpParams))
      // If the method returned false this is because the user cancelled from
      // the save file picker dialog.
      return;
  }


  // XXX We depend on the following holding true in appendFiltersForContentType():
  // If we should save as a complete page, the filterIndex is 0.
  // If we should save as text, the filterIndex is 2.
  var useSaveDocument = isDocument &&
                        (((fpParams.saveMode & SAVEMODE_COMPLETE_DOM) && (fpParams.fp.filterIndex == 0)) ||
                         ((fpParams.saveMode & SAVEMODE_COMPLETE_TEXT) && (fpParams.fp.filterIndex == 2)));

  // If we're saving a document, and are saving either in complete mode or
  // as converted text, pass the document to the web browser persist component.
  // If we're just saving the HTML (second option in the list), send only the URI.
  var source = useSaveDocument ? aDocument : fileInfo.uri;
  var persistArgs = {
    source      : source,
    contentType : (!aChosenData && useSaveDocument && fpParams.fp.filterIndex == 2) ? "text/plain" : contentType,
    target      : (aChosenData ? makeFileURI(aChosenData.file) : fpParams.fp.fileURL),
    postData    : isDocument ? getPostData() : null,
    bypassCache : aShouldBypassCache
  };

  var persist = makeWebBrowserPersist();

  // Calculate persist flags.
  const nsIWBP = Components.interfaces.nsIWebBrowserPersist;
  const flags = nsIWBP.PERSIST_FLAGS_REPLACE_EXISTING_FILES;
  if (aShouldBypassCache)
    persist.persistFlags = flags | nsIWBP.PERSIST_FLAGS_BYPASS_CACHE;
  else
    persist.persistFlags = flags | nsIWBP.PERSIST_FLAGS_FROM_CACHE;

  // Leave it to WebBrowserPersist to discover the encoding type (or lack thereof):
  persist.persistFlags |= nsIWBP.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;

  // Create download and initiate it (below)
  var dl = Components.classes["@mozilla.org/download;1"].createInstance(Components.interfaces.nsIDownload);

  if (useSaveDocument) {
    // Saving a Document, not a URI:
    var filesFolder = null;
    if (persistArgs.contentType != "text/plain") {
      // Create the local directory into which to save associated files.
      var destFile = (aChosenData ? aChosenData.file : fpParams.fp.file);
      filesFolder = destFile.clone();

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
    dl.init((aChosenData ? aChosenData.uri : fileInfo.uri),
            persistArgs.target, null, null, null, persist);
    persist.saveDocument(persistArgs.source, persistArgs.target, filesFolder,
                         persistArgs.contentType, encodingFlags, kWrapColumn);
  } else {
    dl.init((aChosenData ? aChosenData.uri : source),
            persistArgs.target, null, null, null, persist);
    var referer = getReferrer(document);
    persist.saveURI((aChosenData ? aChosenData.uri : source),
                    null, referer, persistArgs.postData, null, persistArgs.target);
  }
}

/**
 * Structure for holding info about automatically supplied parameters for
 * internalSave(...). This allows parameters to be supplied so the user does not
 * need to be prompted for file info.
 * @param aFileAutoChosen This is an nsILocalFile object that has been
 *        pre-determined as the filename for the target to save to
 * @param aUriAutoChosen  This is the nsIURI object for the target
 */
function AutoChosen(aFileAutoChosen, aUriAutoChosen) {
  this.file = aFileAutoChosen;
  this.uri  = aUriAutoChosen;
}

/**
 * Structure for holding info about a URL and the target filename it should be
 * saved to. This structure is populated by initFileInfo(...).
 * @param aSuggestedFileName This is used by initFileInfo(...) when it
 *        cannot 'discover' the filename from the url 
 * @param aFileName The target filename
 * @param aFileBaseName The filename without the file extension
 * @param aFileExt The extension of the filename
 * @param aUri An nsIURI object for the url that is being saved
 */
function FileInfo(aSuggestedFileName, aFileName, aFileBaseName, aFileExt, aUri) {
  this.suggestedFileName = aSuggestedFileName;
  this.fileName = aFileName;
  this.fileBaseName = aFileBaseName;
  this.fileExt = aFileExt;
  this.uri = aUri;
}

/**
 * Determine what the 'default' filename string is, its file extension and the
 * filename without the extension. This filename is used when prompting the user
 * for confirmation in the file picker dialog.
 * @param aFI A FileInfo structure into which we'll put the results of this method.
 * @param aURL The String representation of the URL of the document being saved
 * @param aDocument The document to be saved
 * @param aContentType The content type of the document, if it could be
 *        determined by the caller.
 */
function initFileInfo(aFI, aURL, aDocument, aContentType)
{
  var docCharset = (aDocument ? aDocument.characterSet : null);
  try {
    // Get an nsIURI object from aURL if possible:
    try {
      aFI.uri = makeURI(aURL, docCharset);
      // Assuming nsiUri is valid, calling QueryInterface(...) on it will
      // populate extra object fields (eg filename and file extension).
      var url = aFI.uri.QueryInterface(Components.interfaces.nsIURL);
    } catch (e) {
    }

    // Get the default filename:
    aFI.fileName = getDefaultFileName((aFI.suggestedFileName || aFI.fileName),
                                      aFI.uri, aDocument);
    aFI.fileExt = url.fileExtension;
    // If aFI.fileExt is still blank, consider: aFI.suggestedFileName is supplied
    // if saveURL(...) was the original caller (hence both aContentType and
    // aDocument are blank). If they were saving a link to a website then make
    // the extension .htm .
    if (!aFI.fileExt && !aDocument && !aContentType && (aURL.length > 7) &&
      aURL.substring(0,7).toUpperCase() == "HTTP://") {
      aFI.fileExt = "htm";
      aFI.fileBaseName = aFI.fileName;
    } else {
      aFI.fileExt    = getDefaultExtension(aFI.fileName, aFI.uri, aContentType);
      aFI.fileBaseName = getFileBaseName(aFI.fileName, aFI.fileExt);
    }
  } catch (e) {
  }
}

/** 
 * Given the Filepicker Parameters (aFpP), show the file picker dialog,
 * prompting the user to confirm (or change) the fileName.
 * @param aFpP a structure (see definition in internalSave(...) method)
 *        containing all the data used within this method.
 * @return true if the user confirmed a filename in the picker; false if they
 *         dismissed the picker.
 */
function poseFilePicker(aFpP)
{
  var titleKey = aFpP.fpTitleKey || "SaveLinkTitle";
  var bundle = getStringBundle();
  var fp = aFpP.fp; // simply for smaller readable code
  fp.init(window, bundle.GetStringFromName(titleKey),
          Components.interfaces.nsIFilePicker.modeSave);

  const nsILocalFile = Components.interfaces.nsILocalFile;
  try {
    fp.displayDirectory = aFpP.userDirectory;
  }
  catch (e) {
  }

  fp.defaultExtension = aFpP.fileInfo.fileExt;
  fp.defaultString = getNormalizedLeafName(aFpP.fileInfo.fileName,
                                           aFpP.fileInfo.fileExt);
  appendFiltersForContentType(fp, aFpP.contentType, aFpP.fileInfo.fileExt,
                              aFpP.saveMode);

  if (aFpP.isDocument) {
    try {
      fp.filterIndex = aFpP.prefs.getIntPref("save_converter_index");
    }
    catch (e) {
    }
  }

  if (fp.show() == Components.interfaces.nsIFilePicker.returnCancel || !fp.file)
    return false;

  if (aFpP.isDocument) 
    aFpP.prefs.setIntPref("save_converter_index", fp.filterIndex);

  // Now that the user has had a chance to change the directory and/or filename,
  // re-read those values...
  var directory = fp.file.parent.QueryInterface(nsILocalFile);
  aFpP.prefs.setComplexValue("dir", nsILocalFile, directory);
  fp.file.leafName = validateFileName(fp.file.leafName);
  return true;
}

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

// Get the preferences branch ("browser.download." for normal 'save' mode)...
function getPrefsBrowserDownload(branch)
{
  const prefSvcContractID = "@mozilla.org/preferences-service;1";
  const prefSvcIID = Components.interfaces.nsIPrefService;                              
  return Components.classes[prefSvcContractID].getService(prefSvcIID).getBranch(branch);
}

// Get the current user download directory, return it to the caller:
function getUserDownloadDir(prefs)
{
  try {
    return prefs.getComplexValue("dir", Components.interfaces.nsILocalFile);
  }
  catch (e) {
  }
  return null;
}

function makeWebBrowserPersist()
{
  const persistContractID = "@mozilla.org/embedding/browser/nsWebBrowserPersist;1";
  const persistIID = Components.interfaces.nsIWebBrowserPersist;
  return Components.classes[persistContractID].createInstance(persistIID);
}

/**
 * Constructs a new URI, using nsIIOService.
 * @param aURL The URI spec.
 * @param aOriginCharset The charset of the URI.
 * @return an nsIURI object based on aURL.
 */
function makeURI(aURL, aOriginCharset)
{
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                .getService(Components.interfaces.nsIIOService);
  return ioService.newURI(aURL, aOriginCharset, null);
}

function makeFileURI(aFile)
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

// Given aFileName, find the fileName without the extension on the end.
function getFileBaseName(aFileName, aFileExt)
{
  // Remove the file extension from aFileName:
  return aFileName.replace(/\.[^.]*$/, "");
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

function getDefaultFileName(aDefaultFileName, aDocumentURI, aDocument)
{
  try {
    var url = aDocumentURI.QueryInterface(Components.interfaces.nsIURL);
    if (url.fileName != "") {
      // 1) Use the actual file name, if present
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
      // 2) Use the document title
      return docTitle;
    }
  }

  if (aDefaultFileName)
    // 3) Use the caller-provided name, if any
    return validateFileName(aDefaultFileName);

  // 4) If this is a directory, use the last directory name
  var path = aDocumentURI.path.match(/\/([^\/]+)\/$/);
  if (path && path.length > 1) {
    return validateFileName(path[1]);
  }

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

  return  window.content.document.characterSet;
}
