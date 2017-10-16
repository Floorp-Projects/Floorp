/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserUtils: "resource://gre/modules/BrowserUtils.jsm",
  Downloads: "resource://gre/modules/Downloads.jsm",
  DownloadPaths: "resource://gre/modules/DownloadPaths.jsm",
  DownloadLastDir: "resource://gre/modules/DownloadLastDir.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  Deprecated: "resource://gre/modules/Deprecated.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
});

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
};

function urlSecurityCheck(aURL, aPrincipal, aFlags) {
  return BrowserUtils.urlSecurityCheck(aURL, aPrincipal, aFlags);
}

/**
 * Determine whether or not a given focused DOMWindow is in the content area.
 **/
function isContentFrame(aFocusedWindow) {
  if (!aFocusedWindow)
    return false;

  return (aFocusedWindow.top == window.content);
}

function forbidCPOW(arg, func, argname) {
  if (arg && (typeof(arg) == "object" || typeof(arg) == "function") &&
      Components.utils.isCrossProcessWrapper(arg)) {
    throw new Error(`no CPOWs allowed for argument ${argname} to ${func}`);
  }
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
                 aSkipPrompt, aReferrer, aSourceDocument, aIsContentWindowPrivate) {
  forbidCPOW(aURL, "saveURL", "aURL");
  forbidCPOW(aReferrer, "saveURL", "aReferrer");
  // Allow aSourceDocument to be a CPOW.

  internalSave(aURL, null, aFileName, null, null, aShouldBypassCache,
               aFilePickerTitleKey, null, aReferrer, aSourceDocument,
               aSkipPrompt, null, aIsContentWindowPrivate);
}

// Just like saveURL, but will get some info off the image before
// calling internalSave
// Clientele: (Make sure you don't break any of these)
//  - Context ->  Save Image As...
const imgICache = Components.interfaces.imgICache;
const nsISupportsCString = Components.interfaces.nsISupportsCString;

/**
 * Offers to save an image URL to the file system.
 *
 * @param aURL (string)
 *        The URL of the image to be saved.
 * @param aFileName (string)
 *        The suggested filename for the saved file.
 * @param aFilePickerTitleKey (string, optional)
 *        Localized string key for an alternate title for the file
 *        picker. If set to null, this will default to something sensible.
 * @param aShouldBypassCache (bool)
 *        If true, the image will always be retrieved from the server instead
 *        of the network or image caches.
 * @param aSkipPrompt (bool)
 *        If true, we will attempt to save the file with the suggested
 *        filename to the default downloads folder without showing the
 *        file picker.
 * @param aReferrer (nsIURI, optional)
 *        The referrer URI object (not a URL string) to use, or null
 *        if no referrer should be sent.
 * @param aDoc (nsIDocument, deprecated, optional)
 *        The content document that the save is being initiated from. If this
 *        is omitted, then aIsContentWindowPrivate must be provided.
 * @param aContentType (string, optional)
 *        The content type of the image.
 * @param aContentDisp (string, optional)
 *        The content disposition of the image.
 * @param aIsContentWindowPrivate (bool)
 *        Whether or not the containing window is in private browsing mode.
 *        Does not need to be provided is aDoc is passed.
 */
function saveImageURL(aURL, aFileName, aFilePickerTitleKey, aShouldBypassCache,
                      aSkipPrompt, aReferrer, aDoc, aContentType, aContentDisp,
                      aIsContentWindowPrivate) {
  forbidCPOW(aURL, "saveImageURL", "aURL");
  forbidCPOW(aReferrer, "saveImageURL", "aReferrer");

  if (aDoc && aIsContentWindowPrivate == undefined) {
    if (Components.utils.isCrossProcessWrapper(aDoc)) {
      Deprecated.warning("saveImageURL should not be passed document CPOWs. " +
                         "The caller should pass in the content type and " +
                         "disposition themselves",
                         "https://bugzilla.mozilla.org/show_bug.cgi?id=1243643");
    }
    // This will definitely not work for in-browser code or multi-process compatible
    // add-ons due to bug 1233497, which makes unsafe CPOW usage throw by default.
    Deprecated.warning("saveImageURL should be passed the private state of " +
                       "the containing window.",
                       "https://bugzilla.mozilla.org/show_bug.cgi?id=1243643");
    aIsContentWindowPrivate =
      PrivateBrowsingUtils.isContentWindowPrivate(aDoc.defaultView);
  }

  // We'd better have the private state by now.
  if (aIsContentWindowPrivate == undefined) {
    throw new Error("saveImageURL couldn't compute private state of content window");
  }

  if (!aShouldBypassCache && (aDoc && !Components.utils.isCrossProcessWrapper(aDoc)) &&
      (!aContentType && !aContentDisp)) {
    try {
      var imageCache = Components.classes["@mozilla.org/image/tools;1"]
                                 .getService(Components.interfaces.imgITools)
                                 .getImgCacheForDocument(aDoc);
      var props =
        imageCache.findEntryProperties(makeURI(aURL, getCharsetforSave(null)), aDoc);
      if (props) {
        aContentType = props.get("type", nsISupportsCString);
        aContentDisp = props.get("content-disposition", nsISupportsCString);
      }
    } catch (e) {
      // Failure to get type and content-disposition off the image is non-fatal
    }
  }

  internalSave(aURL, null, aFileName, aContentDisp, aContentType,
               aShouldBypassCache, aFilePickerTitleKey, null, aReferrer,
               null, aSkipPrompt, null, aIsContentWindowPrivate);
}

// This is like saveDocument, but takes any browser/frame-like element
// and saves the current document inside it,
// whether in-process or out-of-process.
function saveBrowser(aBrowser, aSkipPrompt, aOuterWindowID = 0) {
  if (!aBrowser) {
    throw "Must have a browser when calling saveBrowser";
  }
  let persistable = aBrowser.frameLoader;
  let stack = Components.stack.caller;
  persistable.startPersistence(aOuterWindowID, {
    onDocumentReady(document) {
      saveDocument(document, aSkipPrompt);
    },
    onError(status) {
      throw new Components.Exception("saveBrowser failed asynchronously in startPersistence",
                                     status, stack);
    }
  });
}

// Saves a document; aDocument can be an nsIWebBrowserPersistDocument
// (see saveBrowser, above) or an nsIDOMDocument.
//
// aDocument can also be a CPOW for a remote nsIDOMDocument, in which
// case "save as" modes that serialize the document's DOM are
// unavailable.  This is a temporary measure for the "Save Frame As"
// command (bug 1141337) and pre-e10s add-ons.
function saveDocument(aDocument, aSkipPrompt) {
  const Ci = Components.interfaces;

  if (!aDocument)
    throw "Must have a document when calling saveDocument";

  let contentDisposition = null;
  let cacheKeyInt = null;

  if (aDocument instanceof Ci.nsIWebBrowserPersistDocument) {
    // nsIWebBrowserPersistDocument exposes these directly.
    contentDisposition = aDocument.contentDisposition;
    cacheKeyInt = aDocument.cacheKey;
  } else if (aDocument instanceof Ci.nsIDOMDocument) {
    // Otherwise it's an actual nsDocument (and possibly a CPOW).
    // We want to use cached data because the document is currently visible.
    let ifreq =
      aDocument.defaultView
               .QueryInterface(Ci.nsIInterfaceRequestor);

    try {
      contentDisposition =
        ifreq.getInterface(Ci.nsIDOMWindowUtils)
             .getDocumentMetadata("content-disposition");
    } catch (ex) {
      // Failure to get a content-disposition is ok
    }

    try {
      let shEntry =
        ifreq.getInterface(Ci.nsIWebNavigation)
             .QueryInterface(Ci.nsIWebPageDescriptor)
             .currentDescriptor
             .QueryInterface(Ci.nsISHEntry);

      let cacheKey = shEntry.cacheKey
                            .QueryInterface(Ci.nsISupportsPRUint32)
                            .data;
      // cacheKey might be a CPOW, which can't be passed to native
      // code, but the data attribute is just a number.
      cacheKeyInt = cacheKey.data;
    } catch (ex) {
      // We might not find it in the cache.  Oh, well.
    }
  }

  // Convert the cacheKey back into an XPCOM object.
  let cacheKey = null;
  if (cacheKeyInt) {
    cacheKey = Cc["@mozilla.org/supports-PRUint32;1"]
      .createInstance(Ci.nsISupportsPRUint32);
    cacheKey.data = cacheKeyInt;
  }

  internalSave(aDocument.documentURI, aDocument, null, contentDisposition,
               aDocument.contentType, false, null, null,
               aDocument.referrer ? makeURI(aDocument.referrer) : null,
               aDocument, aSkipPrompt, cacheKey);
}

function DownloadListener(win, transfer) {
  function makeClosure(name) {
    return function() {
      transfer[name].apply(transfer, arguments);
    };
  }

  this.window = win;

  // Now... we need to forward all calls to our transfer
  for (var i in transfer) {
    if (i != "QueryInterface")
      this[i] = makeClosure(i);
  }
}

DownloadListener.prototype = {
  QueryInterface: function dl_qi(aIID) {
    if (aIID.equals(Components.interfaces.nsIInterfaceRequestor) ||
        aIID.equals(Components.interfaces.nsIWebProgressListener) ||
        aIID.equals(Components.interfaces.nsIWebProgressListener2) ||
        aIID.equals(Components.interfaces.nsISupports)) {
      return this;
    }
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  getInterface: function dl_gi(aIID) {
    if (aIID.equals(Components.interfaces.nsIAuthPrompt) ||
        aIID.equals(Components.interfaces.nsIAuthPrompt2)) {
      var ww =
        Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                  .getService(Components.interfaces.nsIPromptFactory);
      return ww.getPrompt(this.window, aIID);
    }

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

const kSaveAsType_Complete = 0; // Save document with attached objects.
XPCOMUtils.defineConstant(this, "kSaveAsType_Complete", 0);
// const kSaveAsType_URL      = 1; // Save document or URL by itself.
const kSaveAsType_Text     = 2; // Save document, converting to plain text.
XPCOMUtils.defineConstant(this, "kSaveAsType_Text", kSaveAsType_Text);

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
 * @param aInitiatingDocument [optional]
 *        The document from which the save was initiated.
 *        If this is omitted then aIsContentWindowPrivate has to be provided.
 * @param aSkipPrompt [optional]
 *        If set to true, we will attempt to save the file to the
 *        default downloads folder without prompting.
 * @param aCacheKey [optional]
 *        If set will be passed to saveURI.  See nsIWebBrowserPersist for
 *        allowed values.
 * @param aIsContentWindowPrivate [optional]
 *        This parameter is provided when the aInitiatingDocument is not a
 *        real document object. Stores whether aInitiatingDocument.defaultView
 *        was private or not.
 */
function internalSave(aURL, aDocument, aDefaultFileName, aContentDisposition,
                      aContentType, aShouldBypassCache, aFilePickerTitleKey,
                      aChosenData, aReferrer, aInitiatingDocument, aSkipPrompt,
                      aCacheKey, aIsContentWindowPrivate) {
  forbidCPOW(aURL, "internalSave", "aURL");
  forbidCPOW(aReferrer, "internalSave", "aReferrer");
  forbidCPOW(aCacheKey, "internalSave", "aCacheKey");
  // Allow aInitiatingDocument to be a CPOW.

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
    var fileInfo = new FileInfo(aDefaultFileName);
    initFileInfo(fileInfo, aURL, charset, aDocument,
                 aContentType, aContentDisposition);
    sourceURI = fileInfo.uri;

    var fpParams = {
      fpTitleKey: aFilePickerTitleKey,
      fileInfo,
      contentType: aContentType,
      saveMode,
      saveAsType: kSaveAsType_Complete,
      file
    };

    // Find a URI to use for determining last-downloaded-to directory
    let relatedURI = aReferrer || sourceURI;

    promiseTargetFile(fpParams, aSkipPrompt, relatedURI).then(aDialogAccepted => {
      if (!aDialogAccepted)
        return;

      saveAsType = fpParams.saveAsType;
      file = fpParams.file;

      continueSave();
    }).catch(Components.utils.reportError);
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
    let nonCPOWDocument =
      aDocument && !Components.utils.isCrossProcessWrapper(aDocument);

    let isPrivate = aIsContentWindowPrivate;
    if (isPrivate === undefined) {
      isPrivate = aInitiatingDocument instanceof Components.interfaces.nsIDOMDocument
        ? PrivateBrowsingUtils.isContentWindowPrivate(aInitiatingDocument.defaultView)
        : aInitiatingDocument.isPrivate;
    }

    var persistArgs = {
      sourceURI,
      sourceReferrer: aReferrer,
      sourceDocument: useSaveDocument ? aDocument : null,
      targetContentType: (saveAsType == kSaveAsType_Text) ? "text/plain" : null,
      targetFile: file,
      sourceCacheKey: aCacheKey,
      sourcePostData: nonCPOWDocument ? getPostData(aDocument) : null,
      bypassCache: aShouldBypassCache,
      isPrivate,
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
 * @param persistArgs.isPrivate
 *        Indicates whether this is taking place in a private browsing context.
 */
function internalPersist(persistArgs) {
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

  // Create download and initiate it (below)
  var tr = Components.classes["@mozilla.org/transfer;1"].createInstance(Components.interfaces.nsITransfer);
  tr.init(persistArgs.sourceURI,
          targetFileURL, "", null, null, null, persist, persistArgs.isPrivate);
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
    } else {
      encodingFlags |= nsIWBP.ENCODE_FLAGS_ENCODE_BASIC_ENTITIES;
    }

    const kWrapColumn = 80;
    persist.saveDocument(persistArgs.sourceDocument, targetFileURL, filesFolder,
                         persistArgs.targetContentType, encodingFlags, kWrapColumn);
  } else {
    persist.savePrivacyAwareURI(persistArgs.sourceURI,
                                persistArgs.sourceCacheKey,
                                persistArgs.sourceReferrer,
                                Components.interfaces.nsIHttpChannel.REFERRER_POLICY_UNSET,
                                persistArgs.sourcePostData,
                                null,
                                targetFileURL,
                                persistArgs.isPrivate);
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
                      aContentType, aContentDisposition) {
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
 * @return Promise
 * @resolve a boolean. When true, it indicates that the file picker dialog
 *          is accepted.
 */
function promiseTargetFile(aFpP, /* optional */ aSkipPrompt, /* optional */ aRelatedURI) {
  return (async function() {
    let downloadLastDir = new DownloadLastDir(window);
    let prefBranch = Services.prefs.getBranch("browser.download.");
    let useDownloadDir = prefBranch.getBoolPref("useDownloadDir");

    if (!aSkipPrompt)
      useDownloadDir = false;

    // Default to the user's default downloads directory configured
    // through download prefs.
    let dirPath = await Downloads.getPreferredDownloadsDirectory();
    let dirExists = await OS.File.exists(dirPath);
    let dir = new FileUtils.File(dirPath);

    if (useDownloadDir && dirExists) {
      dir.append(getNormalizedLeafName(aFpP.fileInfo.fileName,
                                       aFpP.fileInfo.fileExt));
      aFpP.file = uniqueFile(dir);
      return true;
    }

    // We must prompt for the file name explicitly.
    // If we must prompt because we were asked to...
    let file = await new Promise(resolve => {
      if (useDownloadDir) {
        // Keep async behavior in both branches
        Services.tm.dispatchToMainThread(function() {
          resolve(null);
        });
      } else {
        downloadLastDir.getFileAsync(aRelatedURI, function getFileAsyncCB(aFile) {
          resolve(aFile);
        });
      }
    });
    if (file && (await OS.File.exists(file.path))) {
      dir = file;
      dirExists = true;
    }

    if (!dirExists) {
      // Default to desktop.
      dir = Services.dirsvc.get("Desk", Components.interfaces.nsIFile);
    }

    let fp = makeFilePicker();
    let titleKey = aFpP.fpTitleKey || "SaveLinkTitle";
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
      // eslint-disable-next-line mozilla/use-default-preference-values
      try {
        fp.filterIndex = prefBranch.getIntPref("save_converter_index");
      } catch (e) {
      }
    }

    let result = await new Promise(resolve => {
      fp.open(function(aResult) {
        resolve(aResult);
      });
    });
    if (result == Components.interfaces.nsIFilePicker.returnCancel || !fp.file) {
      return false;
    }

    if (aFpP.saveMode != SAVEMODE_FILEONLY)
      prefBranch.setIntPref("save_converter_index", fp.filterIndex);

    // Do not store the last save directory as a pref inside the private browsing mode
    downloadLastDir.setFile(aRelatedURI, fp.file.parent);

    fp.file.leafName = validateFileName(fp.file.leafName);

    aFpP.saveAsType = fp.filterIndex;
    aFpP.file = fp.file;
    aFpP.fileURL = fp.fileURL;

    return true;
  })();
}

// Since we're automatically downloading, we don't get the file picker's
// logic to check for existing files, so we need to do that here.
//
// Note - this code is identical to that in
//   mozilla/toolkit/mozapps/downloads/src/nsHelperAppDlg.js.in
// If you are updating this code, update that code too! We can't share code
// here since that code is called in a js component.
function uniqueFile(aLocalFile) {
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
    } else {
      // replace the last (n) in the filename with (n+1)
      aLocalFile.leafName = aLocalFile.leafName.replace(/^(.*\()\d+\)/, "$1" + (collisionCount + 1) + ")");
    }
  }
  return aLocalFile;
}

/**
 * Download a URL using the new jsdownloads API.
 *
 * @param aURL
 *        the url to download
 * @param [optional] aFileName
 *        the destination file name, if omitted will be obtained from the url.
 * @param aInitiatingDocument
 *        The document from which the download was initiated.
 */
function DownloadURL(aURL, aFileName, aInitiatingDocument) {
  // For private browsing, try to get document out of the most recent browser
  // window, or provide our own if there's no browser window.
  let isPrivate = aInitiatingDocument.defaultView
                                     .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                                     .getInterface(Components.interfaces.nsIWebNavigation)
                                     .QueryInterface(Components.interfaces.nsILoadContext)
                                     .usePrivateBrowsing;

  let fileInfo = new FileInfo(aFileName);
  initFileInfo(fileInfo, aURL, null, null, null, null);

  let filepickerParams = {
    fileInfo,
    saveMode: SAVEMODE_FILEONLY
  };

  (async function() {
    let accepted = await promiseTargetFile(filepickerParams, true, fileInfo.uri);
    if (!accepted)
      return;

    let file = filepickerParams.file;
    let download = await Downloads.createDownload({
      source: { url: aURL, isPrivate },
      target: { path: file.path, partFilePath: file.path + ".part" }
    });
    download.tryToKeepPartialData = true;

    // Ignore errors because failures are reported through the download list.
    download.start().catch(() => {});

    // Add the download to the list, allowing it to be managed.
    let list = await Downloads.getList(Downloads.ALL);
    list.add(download);
  })().catch(Components.utils.reportError);
}

// We have no DOM, and can only save the URL as is.
const SAVEMODE_FILEONLY      = 0x00;
XPCOMUtils.defineConstant(this, "SAVEMODE_FILEONLY", SAVEMODE_FILEONLY);
// We have a DOM and can save as complete.
const SAVEMODE_COMPLETE_DOM  = 0x01;
XPCOMUtils.defineConstant(this, "SAVEMODE_COMPLETE_DOM", SAVEMODE_COMPLETE_DOM);
// We have a DOM which we can serialize as text.
const SAVEMODE_COMPLETE_TEXT = 0x02;
XPCOMUtils.defineConstant(this, "SAVEMODE_COMPLETE_TEXT", SAVEMODE_COMPLETE_TEXT);

// If we are able to save a complete DOM, the 'save as complete' filter
// must be the first filter appended.  The 'save page only' counterpart
// must be the second filter appended.  And the 'save as complete text'
// filter must be the third filter appended.
function appendFiltersForContentType(aFilePicker, aContentType, aFileExtension, aSaveMode) {
  // The bundle name for saving only a specific content type.
  var bundleName;
  // The corresponding filter string for a specific content type.
  var filterString;

  // Every case where GetSaveModeForContentType can return non-FILEONLY
  // modes must be handled here.
  if (aSaveMode != SAVEMODE_FILEONLY) {
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
    }
  }

  if (!bundleName) {
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

function getPostData(aDocument) {
  const Ci = Components.interfaces;

  if (aDocument instanceof Ci.nsIWebBrowserPersistDocument) {
    return aDocument.postData;
  }
  try {
    // Find the session history entry corresponding to the given document. In
    // the current implementation, nsIWebPageDescriptor.currentDescriptor always
    // returns a session history entry.
    let sessionHistoryEntry =
        aDocument.defaultView
                 .QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIWebNavigation)
                 .QueryInterface(Ci.nsIWebPageDescriptor)
                 .currentDescriptor
                 .QueryInterface(Ci.nsISHEntry);
    return sessionHistoryEntry.postData;
  } catch (e) {
  }
  return null;
}

function makeWebBrowserPersist() {
  const persistContractID = "@mozilla.org/embedding/browser/nsWebBrowserPersist;1";
  const persistIID = Components.interfaces.nsIWebBrowserPersist;
  return Components.classes[persistContractID].createInstance(persistIID);
}

function makeURI(aURL, aOriginCharset, aBaseURI) {
  return Services.io.newURI(aURL, aOriginCharset, aBaseURI);
}

function makeFileURI(aFile) {
  return Services.io.newFileURI(aFile);
}

function makeFilePicker() {
  const fpContractID = "@mozilla.org/filepicker;1";
  const fpIID = Components.interfaces.nsIFilePicker;
  return Components.classes[fpContractID].createInstance(fpIID);
}

function getMIMEService() {
  const mimeSvcContractID = "@mozilla.org/mime;1";
  const mimeSvcIID = Components.interfaces.nsIMIMEService;
  const mimeSvc = Components.classes[mimeSvcContractID].getService(mimeSvcIID);
  return mimeSvc;
}

// Given aFileName, find the fileName without the extension on the end.
function getFileBaseName(aFileName) {
  // Remove the file extension from aFileName:
  return aFileName.replace(/\.[^.]*$/, "");
}

function getMIMETypeForURI(aURI) {
  try {
    return getMIMEService().getTypeFromURI(aURI);
  } catch (e) {
  }
  return null;
}

function getMIMEInfoForType(aMIMEType, aExtension) {
  if (aMIMEType || aExtension) {
    try {
      return getMIMEService().getFromTypeAndExtension(aMIMEType, aExtension);
    } catch (e) {
    }
  }
  return null;
}

function getDefaultFileName(aDefaultFileName, aURI, aDocument,
                            aContentDisposition) {
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
    } catch (e) {
      try {
        fileName = mhp.getParameter(aContentDisposition, "name", charset, true,
                                    dummy);
      } catch (e) {
      }
    }
    if (fileName)
      return fileName;
  }

  let docTitle;
  if (aDocument && aDocument.title && aDocument.title.trim()) {
    // If the document looks like HTML or XML, try to use its original title.
    docTitle = validateFileName(aDocument.title);
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

  try {
    var url = aURI.QueryInterface(Components.interfaces.nsIURL);
    if (url.fileName != "") {
      // 3) Use the actual file name, if present
      var textToSubURI = Components.classes["@mozilla.org/intl/texttosuburi;1"]
                                   .getService(Components.interfaces.nsITextToSubURI);
      return validateFileName(textToSubURI.unEscapeURIForUI("UTF-8", url.fileName));
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
  var path = aURI.pathQueryRef.match(/\/([^\/]+)\/$/);
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
    // in case localized string cannot be found
  }
  // 9) If all else fails, use "index"
  return "index";
}

function validateFileName(aFileName) {
  let processed = DownloadPaths.sanitize(aFileName) || "_";
  if (AppConstants.platform == "android") {
    // If a large part of the filename has been sanitized, then we
    // will use a default filename instead
    if (processed.replace(/_/g, "").length <= processed.length / 2) {
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
  }
  return processed;
}

function getNormalizedLeafName(aFile, aDefaultExtension) {
  if (!aDefaultExtension)
    return aFile;

  if (AppConstants.platform == "win") {
    // Remove trailing dots and spaces on windows
    aFile = aFile.replace(/[\s.]+$/, "");
  }

  // Remove leading dots
  aFile = aFile.replace(/^\.+/, "");

  // Fix up the file name we're saving to to include the default extension
  var i = aFile.lastIndexOf(".");
  if (aFile.substr(i + 1) != aDefaultExtension)
    return aFile + "." + aDefaultExtension;

  return aFile;
}

function getDefaultExtension(aFilename, aURI, aContentType) {
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
  try {
    if (mimeInfo)
      return mimeInfo.primaryExtension;
  } catch (e) {
  }
  // Fall back on the extensions in the filename and URI for lack
  // of anything better.
  return ext || urlext;
}

function GetSaveModeForContentType(aContentType, aDocument) {
  // We can only save a complete page if we have a loaded document,
  // and it's not a CPOW -- nsWebBrowserPersist needs a real document.
  if (!aDocument || Components.utils.isCrossProcessWrapper(aDocument))
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

function getCharsetforSave(aDocument) {
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
 *
 * WARNING: Please note that openURL() does not perform any content security checks!!!
 */
function openURL(aURL) {
  var uri = makeURI(aURL);

  var protocolSvc = Components.classes["@mozilla.org/uriloader/external-protocol-service;1"]
                              .getService(Components.interfaces.nsIExternalProtocolService);

  if (!protocolSvc.isExposedProtocol(uri.scheme)) {
    // If we're not a browser, use the external protocol service to load the URI.
    protocolSvc.loadURI(uri);
  } else {
    var recentWindow = Services.wm.getMostRecentWindow("navigator:browser");
    if (recentWindow) {
      recentWindow.openUILinkIn(uri.spec, "tab");
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
    };
    loadgroup.groupObserver = loadListener;

    var uriListener = {
      onStartURIOpen(uri) { return false; },
      doContent(ctype, preferred, request, handler) { return false; },
      isPreferred(ctype, desired) { return false; },
      canHandleContent(ctype, preferred, desired) { return false; },
      loadCookie: null,
      parentContentListener: null,
      getInterface(iid) {
        if (iid.equals(Components.interfaces.nsIURIContentListener))
          return this;
        if (iid.equals(Components.interfaces.nsILoadGroup))
          return loadgroup;
        throw Components.results.NS_ERROR_NO_INTERFACE;
      }
    };

    var channel = NetUtil.newChannel({
      uri,
      loadUsingSystemPrincipal: true
    });

    if (channel) {
      channel.channelIsForDownload = true;
    }

    var uriLoader = Components.classes["@mozilla.org/uriloader;1"]
                              .getService(Components.interfaces.nsIURILoader);
    uriLoader.openURI(channel,
                      Components.interfaces.nsIURILoader.IS_CONTENT_PREFERRED,
                      uriListener);
  }
}
