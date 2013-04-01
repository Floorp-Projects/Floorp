# -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/PrivateBrowsingUtils.jsm");

var ContentAreaUtils = {

  // this is for backwards compatibility.
  get ioService() {
    return Services.io;
  },

  get stringBundle() {
    delete this.stringBundle;
    return this.stringBundle =
      Services.strings.createBundle("chrome://global/locale/contentAreaCommands.properties");
  }
}

/**
 * urlSecurityCheck: JavaScript wrapper for checkLoadURIWithPrincipal
 * and checkLoadURIStrWithPrincipal.
 * If |aPrincipal| is not allowed to link to |aURL|, this function throws with
 * an error message.
 *
 * @param aURL
 *        The URL a page has linked to. This could be passed either as a string
 *        or as a nsIURI object.
 * @param aPrincipal
 *        The principal of the document from which aURL came.
 * @param aFlags
 *        Flags to be passed to checkLoadURIStr. If undefined,
 *        nsIScriptSecurityManager.STANDARD will be passed.
 */
function urlSecurityCheck(aURL, aPrincipal, aFlags)
{
  var secMan = Services.scriptSecurityManager;
  if (aFlags === undefined) {
    aFlags = secMan.STANDARD;
  }

  try {
    if (aURL instanceof Components.interfaces.nsIURI)
      secMan.checkLoadURIWithPrincipal(aPrincipal, aURL, aFlags);
    else
      secMan.checkLoadURIStrWithPrincipal(aPrincipal, aURL, aFlags);
  } catch (e) {
    let principalStr = "";
    try {
      principalStr = " from " + aPrincipal.URI.spec;
    }
    catch(e2) { }

    throw "Load of " + aURL + principalStr + " denied.";
  }
}

/**
 * Determine whether or not a given focused DOMWindow is in the content area.
 **/
function isContentFrame(aFocusedWindow)
{
  if (!aFocusedWindow)
    return false;

  return (aFocusedWindow.top == window.content);
}


// Clientele: (Make sure you don't break any of these)
//  - File    ->  Save Page/Frame As...
//  - Context ->  Save Page/Frame As...
//  - Context ->  Save Link As...
//  - Alt-Click links in web pages
//  - Alt-Click links in the UI
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
function saveURL(aURL, aFileName, aFilePickerTitleKey, aShouldBypassCache,
                 aSkipPrompt, aReferrer, aSourceDocument)
{
  internalSave(aURL, null, aFileName, null, null, aShouldBypassCache,
               aFilePickerTitleKey, null, aReferrer, aSourceDocument,
               aSkipPrompt, null);
}

// Just like saveURL, but will get some info off the image before
// calling internalSave
// Clientele: (Make sure you don't break any of these)
//  - Context ->  Save Image As...
const imgICache = Components.interfaces.imgICache;
const nsISupportsCString = Components.interfaces.nsISupportsCString;

function saveImageURL(aURL, aFileName, aFilePickerTitleKey, aShouldBypassCache,
                      aSkipPrompt, aReferrer, aDoc)
{
  var contentType = null;
  var contentDisposition = null;
  if (!aShouldBypassCache) {
    try {
      var imageCache = Components.classes["@mozilla.org/image/tools;1"]
                                 .getService(Components.interfaces.imgITools)
                                 .getImgCacheForDocument(aDoc);
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
               aShouldBypassCache, aFilePickerTitleKey, null, aReferrer,
               aDoc, aSkipPrompt, null);
}

function saveDocument(aDocument, aSkipPrompt)
{
  if (!aDocument)
    throw "Must have a document when calling saveDocument";

  // We want to use cached data because the document is currently visible.
  var ifreq =
    aDocument.defaultView
             .QueryInterface(Components.interfaces.nsIInterfaceRequestor);

  var contentDisposition = null;
  try {
    contentDisposition =
      ifreq.getInterface(Components.interfaces.nsIDOMWindowUtils)
           .getDocumentMetadata("content-disposition");
  } catch (ex) {
    // Failure to get a content-disposition is ok
  }

  var cacheKey = null;
  try {
    cacheKey =
      ifreq.getInterface(Components.interfaces.nsIWebNavigation)
           .QueryInterface(Components.interfaces.nsIWebPageDescriptor);
  } catch (ex) {
    // We might not find it in the cache.  Oh, well.
  }

  internalSave(aDocument.location.href, aDocument, null, contentDisposition,
               aDocument.contentType, false, null, null,
               aDocument.referrer ? makeURI(aDocument.referrer) : null,
               aDocument, aSkipPrompt, cacheKey);
}

function DownloadListener(win, transfer) {
  function makeClosure(name) {
    return function() {
      transfer[name].apply(transfer, arguments);
    }
  }

  this.window = win;

  // Now... we need to forward all calls to our transfer
  for (var i in transfer) {
    if (i != "QueryInterface")
      this[i] = makeClosure(i);
  }
}

DownloadListener.prototype = {
  QueryInterface: function dl_qi(aIID)
  {
    if (aIID.equals(Components.interfaces.nsIInterfaceRequestor) ||
        aIID.equals(Components.interfaces.nsIWebProgressListener) ||
        aIID.equals(Components.interfaces.nsIWebProgressListener2) ||
        aIID.equals(Components.interfaces.nsISupports)) {
      return this;
    }
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  getInterface: function dl_gi(aIID)
  {
    if (aIID.equals(Components.interfaces.nsIAuthPrompt) ||
        aIID.equals(Components.interfaces.nsIAuthPrompt2)) {
      var ww =
        Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                  .getService(Components.interfaces.nsIPromptFactory);
      return ww.getPrompt(this.window, aIID);
    }

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
}

const kSaveAsType_Complete = 0; // Save document with attached objects.
// const kSaveAsType_URL      = 1; // Save document or URL by itself.
const kSaveAsType_Text     = 2; // Save document, converting to plain text.

/**
 * internalSave: Used when saving a document or URL.
 *
 * If aChosenData is null, this method:
 *  - Determines a local target filename to use
 *  - Prompts the user to confirm the destination filename and save mode
 *    (aContentType affects this)
 *  - [Note] This process involves the parameters aURL, aReferrer (to determine
 *    how aURL was encoded), aDocument, aDefaultFileName, aFilePickerTitleKey,
 *    and aSkipPrompt.
 *
 * If aChosenData is non-null, this method:
 *  - Uses the provided source URI and save file name
 *  - Saves the document as complete DOM if possible (aDocument present and
 *    right aContentType)
 *  - [Note] The parameters aURL, aDefaultFileName, aFilePickerTitleKey, and
 *    aSkipPrompt are ignored.
 *
 * In any case, this method:
 *  - Creates a 'Persist' object (which will perform the saving in the
 *    background) and then starts it.
 *  - [Note] This part of the process only involves the parameters aDocument,
 *    aShouldBypassCache and aReferrer. The source, the save name and the save
 *    mode are the ones determined previously.
 *
 * @param aURL
 *        The String representation of the URL of the document being saved
 * @param aDocument
 *        The document to be saved
 * @param aDefaultFileName
 *        The caller-provided suggested filename if we don't 
 *        find a better one
 * @param aContentDisposition
 *        The caller-provided content-disposition header to use.
 * @param aContentType
 *        The caller-provided content-type to use
 * @param aShouldBypassCache
 *        If true, the document will always be refetched from the server
 * @param aFilePickerTitleKey
 *        Alternate title for the file picker
 * @param aChosenData
 *        If non-null this contains an instance of object AutoChosen (see below)
 *        which holds pre-determined data so that the user does not need to be
 *        prompted for a target filename.
 * @param aReferrer
 *        the referrer URI object (not URL string) to use, or null
 *        if no referrer should be sent.
 * @param aInitiatingDocument
 *        The document from which the save was initiated.
 * @param aSkipPrompt [optional]
 *        If set to true, we will attempt to save the file to the
 *        default downloads folder without prompting.
 * @param aCacheKey [optional]
 *        If set will be passed to saveURI.  See nsIWebBrowserPersist for
 *        allowed values.
 */
function internalSave(aURL, aDocument, aDefaultFileName, aContentDisposition,
                      aContentType, aShouldBypassCache, aFilePickerTitleKey,
                      aChosenData, aReferrer, aInitiatingDocument, aSkipPrompt,
                      aCacheKey)
{
  if (aSkipPrompt == undefined)
    aSkipPrompt = false;

  if (aCacheKey == undefined)
    aCacheKey = null;

  // Note: aDocument == null when this code is used by save-link-as...
  var saveMode = GetSaveModeForContentType(aContentType, aDocument);

  var file, sourceURI, saveAsType;
  // Find the URI object for aURL and the FileName/Extension to use when saving.
  // FileName/Extension will be ignored if aChosenData supplied.
  if (aChosenData) {
    file = aChosenData.file;
    sourceURI = aChosenData.uri;
    saveAsType = kSaveAsType_Complete;

    continueSave();
  } else {
    var charset = null;
    if (aDocument)
      charset = aDocument.characterSet;
    else if (aReferrer)
      charset = aReferrer.originCharset;
    var fileInfo = new FileInfo(aDefaultFileName);
    initFileInfo(fileInfo, aURL, charset, aDocument,
                 aContentType, aContentDisposition);
    sourceURI = fileInfo.uri;

    var fpParams = {
      fpTitleKey: aFilePickerTitleKey,
      fileInfo: fileInfo,
      contentType: aContentType,
      saveMode: saveMode,
      saveAsType: kSaveAsType_Complete,
      file: file
    };

    // Find a URI to use for determining last-downloaded-to directory
    let relatedURI = aReferrer || sourceURI;

    getTargetFile(fpParams, function(aDialogCancelled) {
      if (aDialogCancelled)
        return;

      saveAsType = fpParams.saveAsType;
      file = fpParams.file;

      continueSave();
    }, aSkipPrompt, relatedURI);
  }

  function continueSave() {
    // XXX We depend on the following holding true in appendFiltersForContentType():
    // If we should save as a complete page, the saveAsType is kSaveAsType_Complete.
    // If we should save as text, the saveAsType is kSaveAsType_Text.
    var useSaveDocument = aDocument &&
                          (((saveMode & SAVEMODE_COMPLETE_DOM) && (saveAsType == kSaveAsType_Complete)) ||
                           ((saveMode & SAVEMODE_COMPLETE_TEXT) && (saveAsType == kSaveAsType_Text)));
    // If we're saving a document, and are saving either in complete mode or
    // as converted text, pass the document to the web browser persist component.
    // If we're just saving the HTML (second option in the list), send only the URI.
    var persistArgs = {
      sourceURI         : sourceURI,
      sourceReferrer    : aReferrer,
      sourceDocument    : useSaveDocument ? aDocument : null,
      targetContentType : (saveAsType == kSaveAsType_Text) ? "text/plain" : null,
      targetFile        : file,
      sourceCacheKey    : aCacheKey,
      sourcePostData    : aDocument ? getPostData(aDocument) : null,
      bypassCache       : aShouldBypassCache,
      initiatingWindow  : aInitiatingDocument.defaultView
    };

    // Start the actual save process
    internalPersist(persistArgs);
  }
}

/**
 * internalPersist: Creates a 'Persist' object (which will perform the saving
 *  in the background) and then starts it.
 *
 * @param persistArgs.sourceURI
 *        The nsIURI of the document being saved
 * @param persistArgs.sourceCacheKey [optional]
 *        If set will be passed to saveURI
 * @param persistArgs.sourceDocument [optional]
 *        The document to be saved, or null if not saving a complete document
 * @param persistArgs.sourceReferrer
 *        Required and used only when persistArgs.sourceDocument is NOT present,
 *        the nsIURI of the referrer to use, or null if no referrer should be
 *        sent.
 * @param persistArgs.sourcePostData
 *        Required and used only when persistArgs.sourceDocument is NOT present,
 *        represents the POST data to be sent along with the HTTP request, and
 *        must be null if no POST data should be sent.
 * @param persistArgs.targetFile
 *        The nsIFile of the file to create
 * @param persistArgs.targetContentType
 *        Required and used only when persistArgs.sourceDocument is present,
 *        determines the final content type of the saved file, or null to use
 *        the same content type as the source document. Currently only
 *        "text/plain" is meaningful.
 * @param persistArgs.bypassCache
 *        If true, the document will always be refetched from the server
 * @param persistArgs.initiatingWindow
 *        The window from which the save operation was initiated.
 */
function internalPersist(persistArgs)
{
  var persist = makeWebBrowserPersist();

  // Calculate persist flags.
  const nsIWBP = Components.interfaces.nsIWebBrowserPersist;
  const flags = nsIWBP.PERSIST_FLAGS_REPLACE_EXISTING_FILES |
                nsIWBP.PERSIST_FLAGS_FORCE_ALLOW_COOKIES;
  if (persistArgs.bypassCache)
    persist.persistFlags = flags | nsIWBP.PERSIST_FLAGS_BYPASS_CACHE;
  else
    persist.persistFlags = flags | nsIWBP.PERSIST_FLAGS_FROM_CACHE;

  // Leave it to WebBrowserPersist to discover the encoding type (or lack thereof):
  persist.persistFlags |= nsIWBP.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;

  // Find the URI associated with the target file
  var targetFileURL = makeFileURI(persistArgs.targetFile);

  var isPrivate = PrivateBrowsingUtils.isWindowPrivate(persistArgs.initiatingWindow);

  // Create download and initiate it (below)
  var tr = Components.classes["@mozilla.org/transfer;1"].createInstance(Components.interfaces.nsITransfer);
  tr.init(persistArgs.sourceURI,
          targetFileURL, "", null, null, null, persist, isPrivate);
  persist.progressListener = new DownloadListener(window, tr);

  if (persistArgs.sourceDocument) {
    // Saving a Document, not a URI:
    var filesFolder = null;
    if (persistArgs.targetContentType != "text/plain") {
      // Create the local directory into which to save associated files.
      filesFolder = persistArgs.targetFile.clone();

      var nameWithoutExtension = getFileBaseName(filesFolder.leafName);
      var filesFolderLeafName =
        ContentAreaUtils.stringBundle
                        .formatStringFromName("filesFolder", [nameWithoutExtension], 1);

      filesFolder.leafName = filesFolderLeafName;
    }

    var encodingFlags = 0;
    if (persistArgs.targetContentType == "text/plain") {
      encodingFlags |= nsIWBP.ENCODE_FLAGS_FORMATTED;
      encodingFlags |= nsIWBP.ENCODE_FLAGS_ABSOLUTE_LINKS;
      encodingFlags |= nsIWBP.ENCODE_FLAGS_NOFRAMES_CONTENT;
    }
    else {
      encodingFlags |= nsIWBP.ENCODE_FLAGS_ENCODE_BASIC_ENTITIES;
    }

    const kWrapColumn = 80;
    persist.saveDocument(persistArgs.sourceDocument, targetFileURL, filesFolder,
                         persistArgs.targetContentType, encodingFlags, kWrapColumn);
  } else {
    let privacyContext = persistArgs.initiatingWindow
                                    .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                                    .getInterface(Components.interfaces.nsIWebNavigation)
                                    .QueryInterface(Components.interfaces.nsILoadContext);
    persist.saveURI(persistArgs.sourceURI,
                    persistArgs.sourceCacheKey, persistArgs.sourceReferrer, persistArgs.sourcePostData, null,
                    targetFileURL, privacyContext);
  }
}

/**
 * Structure for holding info about automatically supplied parameters for
 * internalSave(...). This allows parameters to be supplied so the user does not
 * need to be prompted for file info.
 * @param aFileAutoChosen This is an nsIFile object that has been
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
 * @param aURLCharset The charset of aURL.
 * @param aDocument The document to be saved
 * @param aContentType The content type we're saving, if it could be
 *        determined by the caller.
 * @param aContentDisposition The content-disposition header for the object
 *        we're saving, if it could be determined by the caller.
 */
function initFileInfo(aFI, aURL, aURLCharset, aDocument,
                      aContentType, aContentDisposition)
{
  try {
    // Get an nsIURI object from aURL if possible:
    try {
      aFI.uri = makeURI(aURL, aURLCharset);
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
      aFI.fileExt = getDefaultExtension(aFI.fileName, aFI.uri, aContentType);
      aFI.fileBaseName = getFileBaseName(aFI.fileName);
    }
  } catch (e) {
  }
}

/** 
 * Given the Filepicker Parameters (aFpP), show the file picker dialog,
 * prompting the user to confirm (or change) the fileName.
 * @param aFpP
 *        A structure (see definition in internalSave(...) method)
 *        containing all the data used within this method.
 * @param aCallback
 *        A callback function that will be called once the function finishes.
 *        The first argument passed to the function will be a boolean that,
 *        when true, indicated that the user dismissed the file picker.
 * @param aSkipPrompt
 *        If true, attempt to save the file automatically to the user's default
 *        download directory, thus skipping the explicit prompt for a file name,
 *        but only if the associated preference is set.
 *        If false, don't save the file automatically to the user's
 *        default download directory, even if the associated preference
 *        is set, but ask for the target explicitly.
 * @param aRelatedURI
 *        An nsIURI associated with the download. The last used
 *        directory of the picker is retrieved from/stored in the 
 *        Content Pref Service using this URI.
 */
function getTargetFile(aFpP, aCallback, /* optional */ aSkipPrompt, /* optional */ aRelatedURI)
{
  if (!getTargetFile.DownloadLastDir)
    Components.utils.import("resource://gre/modules/DownloadLastDir.jsm", getTargetFile);
  var gDownloadLastDir = new getTargetFile.DownloadLastDir(window);

  var prefs = Services.prefs.getBranch("browser.download.");
  var useDownloadDir = prefs.getBoolPref("useDownloadDir");
  const nsIFile = Components.interfaces.nsIFile;

  if (!aSkipPrompt)
    useDownloadDir = false;

  // Default to the user's default downloads directory configured
  // through download prefs.
  var dir = Services.downloads.userDownloadsDirectory;
  var dirExists = dir && dir.exists();

  if (useDownloadDir && dirExists) {
    dir.append(getNormalizedLeafName(aFpP.fileInfo.fileName,
                                     aFpP.fileInfo.fileExt));
    aFpP.file = uniqueFile(dir);
    aCallback(false);
    return;
  }

  // We must prompt for the file name explicitly.
  // If we must prompt because we were asked to...
  if (useDownloadDir) {
    // Keep async behavior in both branches
    Services.tm.mainThread.dispatch(function() {
      displayPicker();
    }, Components.interfaces.nsIThread.DISPATCH_NORMAL);
  } else {
    gDownloadLastDir.getFileAsync(aRelatedURI, function getFileAsyncCB(aFile) {
      if (aFile && aFile.exists()) {
        dir = aFile;
        dirExists = true;
      }
      displayPicker();
    });
  }

  function displayPicker() {
    if (!dirExists) {
      // Default to desktop.
      dir = Services.dirsvc.get("Desk", nsIFile);
    }

    var fp = makeFilePicker();
    var titleKey = aFpP.fpTitleKey || "SaveLinkTitle";
    fp.init(window, ContentAreaUtils.stringBundle.GetStringFromName(titleKey),
            Components.interfaces.nsIFilePicker.modeSave);

    fp.displayDirectory = dir;
    fp.defaultExtension = aFpP.fileInfo.fileExt;
    fp.defaultString = getNormalizedLeafName(aFpP.fileInfo.fileName,
                                             aFpP.fileInfo.fileExt);
    appendFiltersForContentType(fp, aFpP.contentType, aFpP.fileInfo.fileExt,
                                aFpP.saveMode);

    // The index of the selected filter is only preserved and restored if there's
    // more than one filter in addition to "All Files".
    if (aFpP.saveMode != SAVEMODE_FILEONLY) {
      try {
        fp.filterIndex = prefs.getIntPref("save_converter_index");
      }
      catch (e) {
      }
    }

    if (fp.show() == Components.interfaces.nsIFilePicker.returnCancel || !fp.file) {
      aCallback(true);
      return;
    }

    if (aFpP.saveMode != SAVEMODE_FILEONLY)
      prefs.setIntPref("save_converter_index", fp.filterIndex);

    // Do not store the last save directory as a pref inside the private browsing mode
    var directory = fp.file.parent.QueryInterface(nsIFile);
    gDownloadLastDir.setFile(aRelatedURI, directory);

    fp.file.leafName = validateFileName(fp.file.leafName);

    aFpP.saveAsType = fp.filterIndex;
    aFpP.file = fp.file;
    aFpP.fileURL = fp.fileURL;
    aCallback(false);
  }
}

// Since we're automatically downloading, we don't get the file picker's
// logic to check for existing files, so we need to do that here.
//
// Note - this code is identical to that in
//   mozilla/toolkit/mozapps/downloads/src/nsHelperAppDlg.js.in
// If you are updating this code, update that code too! We can't share code
// here since that code is called in a js component.
function uniqueFile(aLocalFile)
{
  var collisionCount = 0;
  while (aLocalFile.exists()) {
    collisionCount++;
    if (collisionCount == 1) {
      // Append "(2)" before the last dot in (or at the end of) the filename
      // special case .ext.gz etc files so we don't wind up with .tar(2).gz
      if (aLocalFile.leafName.match(/\.[^\.]{1,3}\.(gz|bz2|Z)$/i))
        aLocalFile.leafName = aLocalFile.leafName.replace(/\.[^\.]{1,3}\.(gz|bz2|Z)$/i, "(2)$&");
      else
        aLocalFile.leafName = aLocalFile.leafName.replace(/(\.[^\.]*)?$/, "(2)$&");
    }
    else {
      // replace the last (n) in the filename with (n+1)
      aLocalFile.leafName = aLocalFile.leafName.replace(/^(.*\()\d+\)/, "$1" + (collisionCount + 1) + ")");
    }
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

  case "image/svg+xml":
    bundleName   = "WebPageSVGOnlyFilter";
    filterString = "*.svg; *.svgz";
    break;

  case "text/xml":
  case "application/xml":
    bundleName   = "WebPageXMLOnlyFilter";
    filterString = "*.xml";
    break;

  default:
    if (aSaveMode != SAVEMODE_FILEONLY)
      throw "Invalid save mode for type '" + aContentType + "'";

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

      if (extString)
        aFilePicker.appendFilter(mimeInfo.description, extString);
    }

    break;
  }

  if (aSaveMode & SAVEMODE_COMPLETE_DOM) {
    aFilePicker.appendFilter(ContentAreaUtils.stringBundle.GetStringFromName("WebPageCompleteFilter"),
                             filterString);
    // We should always offer a choice to save document only if
    // we allow saving as complete.
    aFilePicker.appendFilter(ContentAreaUtils.stringBundle.GetStringFromName(bundleName),
                             filterString);
  }

  if (aSaveMode & SAVEMODE_COMPLETE_TEXT)
    aFilePicker.appendFilters(Components.interfaces.nsIFilePicker.filterText);

  // Always append the all files (*) filter
  aFilePicker.appendFilters(Components.interfaces.nsIFilePicker.filterAll);
}

function getPostData(aDocument)
{
  try {
    // Find the session history entry corresponding to the given document. In
    // the current implementation, nsIWebPageDescriptor.currentDescriptor always
    // returns a session history entry.
    var sessionHistoryEntry =
        aDocument.defaultView
                 .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                 .getInterface(Components.interfaces.nsIWebNavigation)
                 .QueryInterface(Components.interfaces.nsIWebPageDescriptor)
                 .currentDescriptor
                 .QueryInterface(Components.interfaces.nsISHEntry);
    return sessionHistoryEntry.postData;
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
 * @param aBaseURI Base URI to resolve aURL, or null.
 * @return an nsIURI object based on aURL.
 */
function makeURI(aURL, aOriginCharset, aBaseURI)
{
  return Services.io.newURI(aURL, aOriginCharset, aBaseURI);
}

function makeFileURI(aFile)
{
  return Services.io.newFileURI(aFile);
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
function getFileBaseName(aFileName)
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
    if (fileName)
      return fileName;
  }

  let docTitle;
  if (aDocument) {
    // If the document looks like HTML or XML, try to use its original title.
    docTitle = validateFileName(aDocument.title).trim();
    if (docTitle) {
      let contentType = aDocument.contentType;
      if (contentType == "application/xhtml+xml" ||
          contentType == "application/xml" ||
          contentType == "image/svg+xml" ||
          contentType == "text/html" ||
          contentType == "text/xml") {
        // 2) Use the document title
        return docTitle;
      }
    }
  }

  try {
    var url = aURI.QueryInterface(Components.interfaces.nsIURL);
    if (url.fileName != "") {
      // 3) Use the actual file name, if present
      var textToSubURI = Components.classes["@mozilla.org/intl/texttosuburi;1"]
                                   .getService(Components.interfaces.nsITextToSubURI);
      return validateFileName(textToSubURI.unEscapeURIForUI(url.originCharset || "UTF-8", url.fileName));
    }
  } catch (e) {
    // This is something like a data: and so forth URI... no filename here.
  }

  if (docTitle)
    // 4) Use the document title
    return docTitle;

  if (aDefaultFileName)
    // 5) Use the caller-provided name, if any
    return validateFileName(aDefaultFileName);

  // 6) If this is a directory, use the last directory name
  var path = aURI.path.match(/\/([^\/]+)\/$/);
  if (path && path.length > 1)
    return validateFileName(path[1]);

  try {
    if (aURI.host)
      // 7) Use the host.
      return aURI.host;
  } catch (e) {
    // Some files have no information at all, like Javascript generated pages
  }
  try {
    // 8) Use the default file name
    return ContentAreaUtils.stringBundle.GetStringFromName("DefaultSaveFileName");
  } catch (e) {
    //in case localized string cannot be found
  }
  // 9) If all else fails, use "index"
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
  else if (navigator.appVersion.indexOf("Android") != -1 ||
           navigator.appVersion.indexOf("Maemo") != -1) {
    // On mobile devices, the filesystem may be very limited in what
    // it considers valid characters. To avoid errors, we sanitize
    // conservatively.
    const dangerousChars = "*?<>|\":/\\[];,+=";
    var processed = "";
    for (var i = 0; i < aFileName.length; i++)
      processed += aFileName.charCodeAt(i) >= 32 &&
                   !(dangerousChars.indexOf(aFileName[i]) >= 0) ? aFileName[i]
                                                                : "_";

    // Last character should not be a space
    processed = processed.trim();

    // If a large part of the filename has been sanitized, then we
    // will use a default filename instead
    if (processed.replace(/_/g, "").length <= processed.length/2) {
      // We purposefully do not use a localized default filename,
      // which we could have done using
      // ContentAreaUtils.stringBundle.GetStringFromName("DefaultSaveFileName")
      // since it may contain invalid characters.
      var original = processed;
      processed = "download";

      // Preserve a suffix, if there is one
      if (original.indexOf(".") >= 0) {
        var suffix = original.split(".").slice(-1)[0];
        if (suffix && suffix.indexOf("_") < 0)
          processed += "." + suffix;
      }
    }
    return processed;
  }

  return aFileName.replace(re, "_");
}

function getNormalizedLeafName(aFile, aDefaultExtension)
{
  if (!aDefaultExtension)
    return aFile;

#ifdef XP_WIN
  // Remove trailing dots and spaces on windows
  aFile = aFile.replace(/[\s.]+$/, "");
#endif

  // Remove leading dots
  aFile = aFile.replace(/^\.+/, "");

  // Fix up the file name we're saving to to include the default extension
  var i = aFile.lastIndexOf(".");
  if (aFile.substr(i + 1) != aDefaultExtension)
    return aFile + "." + aDefaultExtension;

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

  if (ext && mimeInfo && mimeInfo.extensionExists(ext))
    return ext;

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
      if (mimeInfo)
        return mimeInfo.primaryExtension;
    }
    catch (e) {
    }
    // Fall back on the extensions in the filename and URI for lack
    // of anything better.
    return ext || urlext;
  }
}

function GetSaveModeForContentType(aContentType, aDocument)
{
  // We can only save a complete page if we have a loaded document
  if (!aDocument)
    return SAVEMODE_FILEONLY;

  // Find the possible save modes using the provided content type
  var saveMode = SAVEMODE_FILEONLY;
  switch (aContentType) {
  case "text/html":
  case "application/xhtml+xml":
  case "image/svg+xml":
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

  return window.content.document.characterSet;
}

/**
 * Open a URL from chrome, determining if we can handle it internally or need to
 *  launch an external application to handle it.
 * @param aURL The URL to be opened
 */
function openURL(aURL)
{
  var uri = makeURI(aURL);

  var protocolSvc = Components.classes["@mozilla.org/uriloader/external-protocol-service;1"]
                              .getService(Components.interfaces.nsIExternalProtocolService);

  if (!protocolSvc.isExposedProtocol(uri.scheme)) {
    // If we're not a browser, use the external protocol service to load the URI.
    protocolSvc.loadUrl(uri);
  }
  else {
    var recentWindow = Services.wm.getMostRecentWindow("navigator:browser");
    if (recentWindow) {
      var win = recentWindow.browserDOMWindow.openURI(uri, null,
                                                      recentWindow.browserDOMWindow.OPEN_DEFAULTWINDOW,
                                                      recentWindow.browserDOMWindow.OPEN_NEW);
      win.focus();
      return;
    }

    var loadgroup = Components.classes["@mozilla.org/network/load-group;1"]
                              .createInstance(Components.interfaces.nsILoadGroup);
    var appstartup = Services.startup;

    var loadListener = {
      onStartRequest: function ll_start(aRequest, aContext) {
        appstartup.enterLastWindowClosingSurvivalArea();
      },
      onStopRequest: function ll_stop(aRequest, aContext, aStatusCode) {
        appstartup.exitLastWindowClosingSurvivalArea();
      },
      QueryInterface: function ll_QI(iid) {
        if (iid.equals(Components.interfaces.nsISupports) ||
            iid.equals(Components.interfaces.nsIRequestObserver) ||
            iid.equals(Components.interfaces.nsISupportsWeakReference))
          return this;
        throw Components.results.NS_ERROR_NO_INTERFACE;
      }
    }
    loadgroup.groupObserver = loadListener;

    var uriListener = {
      onStartURIOpen: function(uri) { return false; },
      doContent: function(ctype, preferred, request, handler) { return false; },
      isPreferred: function(ctype, desired) { return false; },
      canHandleContent: function(ctype, preferred, desired) { return false; },
      loadCookie: null,
      parentContentListener: null,
      getInterface: function(iid) {
        if (iid.equals(Components.interfaces.nsIURIContentListener))
          return this;
        if (iid.equals(Components.interfaces.nsILoadGroup))
          return loadgroup;
        throw Components.results.NS_ERROR_NO_INTERFACE;
      }
    }

    var channel = Services.io.newChannelFromURI(uri);
    var uriLoader = Components.classes["@mozilla.org/uriloader;1"]
                              .getService(Components.interfaces.nsIURILoader);
    uriLoader.openURI(channel, true, uriListener);
  }
}
