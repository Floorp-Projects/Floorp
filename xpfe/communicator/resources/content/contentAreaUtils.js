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

  return (aFocusedWindow.top == window.content);
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
  return contentFrame.location.href;
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

// Clientelle: (Make sure you don't break any of these)
//  - File    ->  Save Page/Frame As...
//  - Context ->  Save Page/Frame As...
//  - Context ->  Save Link As...
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
function saveURL(aURL, aFileName, aFilePickerTitleKey, aShouldBypassCache,
                 aReferrer)
{
  internalSave(aURL, null, aFileName, null, null, aShouldBypassCache,
               aFilePickerTitleKey, null, aReferrer);
}

// Just like saveURL, but will get some info off the image before
// calling internalSave
// Clientelle: (Make sure you don't break any of these)
//  - Context ->  Save Image As...
const imgICache = Components.interfaces.imgICache;
const nsISupportsCString = Components.interfaces.nsISupportsCString;

function saveImageURL(aURL, aFileName, aFilePickerTitleKey, aShouldBypassCache,
                      aReferrer)
{
  var contentType = null;
  var contentDisposition = null;
  if (!aShouldBypassCache) {
    try {
      var imageCache = Components.classes["@mozilla.org/image/cache;1"]
                                 .getService(imgICache);
      var props =
        imageCache.findEntryProperties(makeURI(aURL, getCharsetforSave(null)));
      if (props) {
        contentType = props.get("type", nsISupportsCString);
        contentDisposition = props.get("content-disposition",
                                       nsISupportsCString);
      }
    } catch (e) {
      // Failure to get type and content-disposition off the image is non-fatal
    }
  }
  internalSave(aURL, null, aFileName, contentDisposition, contentType,
               aShouldBypassCache, aFilePickerTitleKey, null, aReferrer);
}

function saveFrameDocument()
{
  var focusedWindow = document.commandDispatcher.focusedWindow;
  if (isContentFrame(focusedWindow))
    saveDocument(focusedWindow.document);
}

function saveDocument(aDocument)
{
  if (!aDocument) {
    throw "Must have a document when calling saveDocument";
  }

  // We want to use cached data because the document is currently visible.
  var dispHeader = null;
  try {
    dispHeader =
      aDocument.defaultView
               .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
               .getInterface(Components.interfaces.nsIDOMWindowUtils)
               .getDocumentMetadata("content-disposition");
  } catch (ex) {
    // Failure to get a content-disposition is ok
  }
  internalSave(aDocument.location.href, aDocument, null, dispHeader,
               aDocument.contentType, false, null, null);
}

const SAVETYPE_COMPLETE_PAGE = 0x00;
const SAVETYPE_TEXT_ONLY     = 0x02;
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
 * @param aContentDisposition The caller-provided content-disposition header
 *         to use.
 * @param aContentType The caller-provided content-type to use
 * @param aShouldBypassCache If true, the document will always be refetched
 *        from the server
 * @param aFilePickerTitleKey Alternate title for the file picker
 * @param aChosenData If non-null this contains an instance of object AutoChosen
 *        (see below) which holds pre-determined data so that the user does not
 *        need to be prompted for a target filename.
 * @param aReferrer the referrer URI object (not URL string) to use, or null
          if no referrer should be sent.
 */
function internalSave(aURL, aDocument, aDefaultFileName, aContentDisposition,
                      aContentType, aShouldBypassCache, aFilePickerTitleKey,
                      aChosenData, aReferrer)
{
  // Note: aDocument == null when this code is used by save-link-as...
  var saveMode = GetSaveModeForContentType(aContentType);
  var isDocument = aDocument != null && saveMode != SAVEMODE_FILEONLY;
  var saveAsType = SAVETYPE_COMPLETE_PAGE;

  var file, fileURL;
  // Find the URI object for aURL and the FileName/Extension to use when saving.
  // FileName/Extension will be ignored if aChosenData supplied.
  var fileInfo = new FileInfo(aDefaultFileName);
  if (aChosenData)
    file = aChosenData.file;
  else {
    initFileInfo(fileInfo, aURL, aDocument, aContentType, aContentDisposition);
    var fpParams = {
      fpTitleKey: aFilePickerTitleKey,
      isDocument: isDocument,
      fileInfo: fileInfo,
      contentType: aContentType,
      saveMode: saveMode,
      saveAsType: saveAsType,
      file: file,
      fileURL: fileURL
    };
    if (!poseFilePicker(fpParams))
      // If the method returned false this is because the user cancelled from
      // the save file picker dialog.
      return;

    saveAsType = fpParams.saveAsType;
    saveMode = fpParams.saveMode;
    file = fpParams.file;
    fileURL = fpParams.fileURL;
  }

  if (!fileURL)
    fileURL = makeFileURI(file);

  // XXX We depend on the following holding true in appendFiltersForContentType():
  // If we should save as a complete page, the saveAsType is SAVETYPE_COMPLETE_PAGE.
  // If we should save as text, the saveAsType is SAVETYPE_TEXT_ONLY.
  var useSaveDocument = isDocument &&
                        (((saveMode & SAVEMODE_COMPLETE_DOM) && (saveAsType == SAVETYPE_COMPLETE_PAGE)) ||
                         ((saveMode & SAVEMODE_COMPLETE_TEXT) && (saveAsType == SAVETYPE_TEXT_ONLY)));

  // If we're saving a document, and are saving either in complete mode or
  // as converted text, pass the document to the web browser persist component.
  // If we're just saving the HTML (second option in the list), send only the URI.
  var source = useSaveDocument ? aDocument : fileInfo.uri;
  var persistArgs = {
    source      : source,
    contentType : (!aChosenData && useSaveDocument &&
                   saveAsType == SAVETYPE_TEXT_ONLY) ?
                  "text/plain" : aContentType,
    target      : fileURL,
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
  var tr = Components.classes["@mozilla.org/transfer;1"].createInstance(Components.interfaces.nsITransfer);

  if (useSaveDocument) {
    // Saving a Document, not a URI:
    var filesFolder = null;
    if (persistArgs.contentType != "text/plain") {
      // Create the local directory into which to save associated files.
      filesFolder = file.clone();

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
    tr.init((aChosenData ? aChosenData.uri : fileInfo.uri),
            persistArgs.target, "", null, null, null, persist);
    persist.progressListener = tr;
    persist.saveDocument(persistArgs.source, persistArgs.target, filesFolder,
                         persistArgs.contentType, encodingFlags, kWrapColumn);
  } else {
    tr.init((aChosenData ? aChosenData.uri : source),
            persistArgs.target, "", null, null, null, persist);
    persist.progressListener = tr;
    persist.saveURI((aChosenData ? aChosenData.uri : source),
                    null, aReferrer, persistArgs.postData, null,
                    persistArgs.target);
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
 * @param aContentType The content type we're saving, if it could be
 *        determined by the caller.
 * @param aContentDisposition The content-disposition header for the object
 *        we're saving, if it could be determined by the caller.
 */
function initFileInfo(aFI, aURL, aDocument, aContentType, aContentDisposition)
{
  var docCharset = (aDocument ? aDocument.characterSet : null);
  try {
    // Get an nsIURI object from aURL if possible:
    try {
      aFI.uri = makeURI(aURL, docCharset);
      // Assuming nsiUri is valid, calling QueryInterface(...) on it will
      // populate extra object fields (eg filename and file extension).
      var url = aFI.uri.QueryInterface(Components.interfaces.nsIURL);
      aFI.fileExt = url.fileExtension;
    } catch (e) {
    }

    // Get the default filename:
    aFI.fileName = getDefaultFileName((aFI.suggestedFileName || aFI.fileName),
                                      aFI.uri, aDocument, aContentDisposition);
    // If aFI.fileExt is still blank, consider: aFI.suggestedFileName is supplied
    // if saveURL(...) was the original caller (hence both aContentType and
    // aDocument are blank). If they were saving a link to a website then make
    // the extension .htm .
    if (!aFI.fileExt && !aDocument && !aContentType && (/^http(s?):\/\//i.test(aURL))) {
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
  const nsILocalFile = Components.interfaces.nsILocalFile;
  const kDownloadDirPref = "dir";

  var branch = getPrefsBrowserDownload("browser.download.");
  var dir = null;

  // Try and pull in download directory pref
  try {
    dir = branch.getComplexValue(kDownloadDirPref, nsILocalFile);
  } catch (e) { }

  var autoDownload = branch.getBoolPref("autoDownload");
  if (autoDownload && dir && dir.exists()) {
    dir.append(getNormalizedLeafName(aFpP.fileInfo.fileName, aFpP.fileInfo.fileExt));
    aFpP.file = uniqueFile(dir);
    return true;
  }

  // Show the file picker that allows the user to confirm the target filename:
  var fp = makeFilePicker();
  var titleKey = aFpP.fpTitleKey || "SaveLinkTitle";
  var bundle = getStringBundle();
  fp.init(window, bundle.GetStringFromName(titleKey),
          Components.interfaces.nsIFilePicker.modeSave);

  try {
    if (dir.exists())
      fp.displayDirectory = dir;
  } catch (e) { }

  fp.defaultExtension = aFpP.fileInfo.fileExt;
  fp.defaultString = getNormalizedLeafName(aFpP.fileInfo.fileName,
                                           aFpP.fileInfo.fileExt);
  appendFiltersForContentType(fp, aFpP.contentType, aFpP.fileInfo.fileExt,
                              aFpP.saveMode);

  if (aFpP.isDocument) {
    try {
      fp.filterIndex = branch.getIntPref("save_converter_index");
    }
    catch (e) {
    }
  }

  if (fp.show() == Components.interfaces.nsIFilePicker.returnCancel || !fp.file)
    return false;

  if (aFpP.isDocument) 
    branch.setIntPref("save_converter_index", fp.filterIndex);

  // Now that the user has had a chance to change the directory and/or filename,
  // re-read those values...
  if (branch.getBoolPref("lastLocation") || autoDownload) {
    var directory = fp.file.parent.QueryInterface(nsILocalFile);
    branch.setComplexValue(kDownloadDirPref, nsILocalFile, directory);
  }
  fp.file.leafName = validateFileName(fp.file.leafName);

  aFpP.saveAsType = fp.filterIndex;
  aFpP.file = fp.file;
  aFpP.fileURL = fp.fileURL;
  return true;
}

// Since we're automatically downloading, we don't get the file picker's
// logic to check for existing files, so we need to do that here.
//
// Note - this code is identical to that in nsHelperAppDlg.js.
// If you are updating this code, update that code too! We can't share code
// here since that code is called in a js component.

function uniqueFile(aLocalFile)
{
  while (aLocalFile.exists()) {
    parts = /(-\d+)?(\.[^.]+)?$/.test(aLocalFile.leafName);
    aLocalFile.leafName = RegExp.leftContext + (RegExp.$1 - 1) + RegExp.$2;
  }
  return aLocalFile;
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
  if (aMIMEType || aExtension) {
    try {
      return getMIMEService().getFromTypeAndExtension(aMIMEType, aExtension);
    }
    catch (e) {
    }
  }
  return null;
}

function getDefaultFileName(aDefaultFileName, aURI, aDocument,
                            aContentDisposition)
{
  // 1) look for a filename in the content-disposition header, if any
  if (aContentDisposition) {
    const mhpContractID = "@mozilla.org/network/mime-hdrparam;1";
    const mhpIID = Components.interfaces.nsIMIMEHeaderParam;
    const mhp = Components.classes[mhpContractID].getService(mhpIID);
    var dummy = { value: null };  // Need an out param...
    var charset = getCharsetforSave(aDocument);

    var fileName = null;
    try {
      fileName = mhp.getParameter(aContentDisposition, "filename", charset,
                                  true, dummy);
    }
    catch (e) {
      try {
        fileName = mhp.getParameter(aContentDisposition, "name", charset, true,
                                    dummy);
      }
      catch (e) {
      }
    }
    if (fileName) {
      return fileName;
    }
  }

  try {
    var url = aURI.QueryInterface(Components.interfaces.nsIURL);
    if (url.fileName != "") {
      // 2) Use the actual file name, if present
      var textToSubURI = Components.classes["@mozilla.org/intl/texttosuburi;1"]
                                   .getService(Components.interfaces.nsITextToSubURI);
      return validateFileName(textToSubURI.unEscapeURIForUI(url.originCharset || "UTF-8", url.fileName));
    }
  } catch (e) {
    // This is something like a data: and so forth URI... no filename here.
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
  var path = aURI.path.match(/\/([^\/]+)\/$/);
  if (path && path.length > 1) {
    return validateFileName(path[1]);
  }

  try {
    if (aURI.host)
      // 6) Use the host.
      return aURI.host;
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

  return  window.content.document.characterSet;
}
