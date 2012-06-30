/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:cin:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif

#include "base/basictypes.h"

/* This must occur *after* base/basictypes.h to avoid typedefs conflicts. */
#include "mozilla/Util.h"

#include "mozilla/dom/ContentChild.h"
#include "nsXULAppAPI.h"

#include "nsExternalHelperAppService.h"
#include "nsCExternalHandlerService.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsIChannel.h"
#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsICategoryManager.h"
#include "nsXPIDLString.h"
#include "nsUnicharUtils.h"
#include "nsIStringEnumerator.h"
#include "nsMemory.h"
#include "nsIStreamListener.h"
#include "nsIMIMEService.h"
#include "nsILoadGroup.h"
#include "nsIWebProgressListener.h"
#include "nsITransfer.h"
#include "nsReadableUtils.h"
#include "nsIRequest.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIInterfaceRequestor.h"
#include "nsThreadUtils.h"
#include "nsAutoPtr.h"
#include "nsIMutableArray.h"

// used to access our datastore of user-configured helper applications
#include "nsIHandlerService.h"
#include "nsIMIMEInfo.h"
#include "nsIRefreshURI.h" // XXX needed to redirect according to Refresh: URI
#include "nsIDocumentLoader.h" // XXX needed to get orig. channel and assoc. refresh uri
#include "nsIHelperAppLauncherDialog.h"
#include "nsIContentDispatchChooser.h"
#include "nsNetUtil.h"
#include "nsIIOService.h"
#include "nsNetCID.h"
#include "nsChannelProperties.h"

#include "nsMimeTypes.h"
// used for header disposition information.
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIEncodedChannel.h"
#include "nsIMultiPartChannel.h"
#include "nsIFileChannel.h"
#include "nsIObserverService.h" // so we can be a profile change observer
#include "nsIPropertyBag2.h" // for the 64-bit content length

#ifdef XP_MACOSX
#include "nsILocalFileMac.h"
#ifndef __LP64__
#include "nsIAppleFileDecoder.h"
#endif
#elif defined(XP_OS2)
#include "nsILocalFileOS2.h"
#endif

#include "nsIPluginHost.h" // XXX needed for ext->type mapping (bug 233289)
#include "nsPluginHost.h"
#include "nsEscape.h"

#include "nsIStringBundle.h" // XXX needed to localize error msgs
#include "nsIPrompt.h"

#include "nsITextToSubURI.h" // to unescape the filename
#include "nsIMIMEHeaderParam.h"

#include "nsIWindowWatcher.h"

#include "nsIDownloadHistory.h" // to mark downloads as visited
#include "nsDocShellCID.h"

#include "nsIDOMWindow.h"
#include "nsIDocShell.h"

#include "nsCRT.h"

#include "nsLocalHandlerApp.h"

#include "nsIRandomGenerator.h"
#include "plbase64.h"
#include "prmem.h"

#include "ContentChild.h"
#include "nsXULAppAPI.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocShellTreeItem.h"
#include "ExternalHelperAppChild.h"
#include "nsILoadContext.h"

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

#include "mozilla/Preferences.h"

using namespace mozilla;

// Buffer file writes in 32kb chunks
#define BUFFERED_OUTPUT_SIZE (1024 * 32)

// Download Folder location constants
#define NS_PREF_DOWNLOAD_DIR        "browser.download.dir"
#define NS_PREF_DOWNLOAD_FOLDERLIST "browser.download.folderList"
enum {
  NS_FOLDER_VALUE_DESKTOP = 0
, NS_FOLDER_VALUE_DOWNLOADS = 1
, NS_FOLDER_VALUE_CUSTOM = 2
};

#ifdef PR_LOGGING
PRLogModuleInfo* nsExternalHelperAppService::mLog = nsnull;
#endif

// Using level 3 here because the OSHelperAppServices use a log level
// of PR_LOG_DEBUG (4), and we want less detailed output here
// Using 3 instead of PR_LOG_WARN because we don't output warnings
#undef LOG
#define LOG(args) PR_LOG(mLog, 3, args)
#define LOG_ENABLED() PR_LOG_TEST(mLog, 3)

static const char NEVER_ASK_FOR_SAVE_TO_DISK_PREF[] =
  "browser.helperApps.neverAsk.saveToDisk";
static const char NEVER_ASK_FOR_OPEN_FILE_PREF[] =
  "browser.helperApps.neverAsk.openFile";

// Helper functions for Content-Disposition headers

/**
 * Given a URI fragment, unescape it
 * @param aFragment The string to unescape
 * @param aURI The URI from which this fragment is taken. Only its character set
 *             will be used.
 * @param aResult [out] Unescaped string.
 */
static nsresult UnescapeFragment(const nsACString& aFragment, nsIURI* aURI,
                                 nsAString& aResult)
{
  // First, we need a charset
  nsCAutoString originCharset;
  nsresult rv = aURI->GetOriginCharset(originCharset);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now, we need the unescaper
  nsCOMPtr<nsITextToSubURI> textToSubURI = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return textToSubURI->UnEscapeURIForUI(originCharset, aFragment, aResult);
}

/**
 * UTF-8 version of UnescapeFragment.
 * @param aFragment The string to unescape
 * @param aURI The URI from which this fragment is taken. Only its character set
 *             will be used.
 * @param aResult [out] Unescaped string, UTF-8 encoded.
 * @note It is safe to pass the same string for aFragment and aResult.
 * @note When this function fails, aResult will not be modified.
 */
static nsresult UnescapeFragment(const nsACString& aFragment, nsIURI* aURI,
                                 nsACString& aResult)
{
  nsAutoString result;
  nsresult rv = UnescapeFragment(aFragment, aURI, result);
  if (NS_SUCCEEDED(rv))
    CopyUTF16toUTF8(result, aResult);
  return rv;
}

/**
 * Given a channel, returns the filename and extension the channel has.
 * This uses the URL and other sources (nsIMultiPartChannel).
 * Also gives back whether the channel requested external handling (i.e.
 * whether Content-Disposition: attachment was sent)
 * @param aChannel The channel to extract the filename/extension from
 * @param aFileName [out] Reference to the string where the filename should be
 *        stored. Empty if it could not be retrieved.
 *        WARNING - this filename may contain characters which the OS does not
 *        allow as part of filenames!
 * @param aExtension [out] Reference to the string where the extension should
 *        be stored. Empty if it could not be retrieved. Stored in UTF-8.
 * @param aAllowURLExtension (optional) Get the extension from the URL if no
 *        Content-Disposition header is present. Default is true.
 * @retval true The server sent Content-Disposition:attachment or equivalent
 * @retval false Content-Disposition: inline or no content-disposition header
 *         was sent.
 */
static bool GetFilenameAndExtensionFromChannel(nsIChannel* aChannel,
                                                 nsString& aFileName,
                                                 nsCString& aExtension,
                                                 bool aAllowURLExtension = true)
{
  aExtension.Truncate();
  /*
   * If the channel is an http or part of a multipart channel and we
   * have a content disposition header set, then use the file name
   * suggested there as the preferred file name to SUGGEST to the
   * user.  we shouldn't actually use that without their
   * permission... otherwise just use our temp file
   */
  bool handleExternally = false;
  PRUint32 disp;
  nsresult rv = aChannel->GetContentDisposition(&disp);
  if (NS_SUCCEEDED(rv))
  {
    aChannel->GetContentDispositionFilename(aFileName);
    if (disp == nsIChannel::DISPOSITION_ATTACHMENT)
      handleExternally = true;
  }

  // If the disposition header didn't work, try the filename from nsIURL
  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));
  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (url && aFileName.IsEmpty())
  {
    if (aAllowURLExtension) {
      url->GetFileExtension(aExtension);
      UnescapeFragment(aExtension, url, aExtension);

      // Windows ignores terminating dots. So we have to as well, so
      // that our security checks do "the right thing"
      // In case the aExtension consisted only of the dot, the code below will
      // extract an aExtension from the filename
      aExtension.Trim(".", false);
    }

    // try to extract the file name from the url and use that as a first pass as the
    // leaf name of our temp file...
    nsCAutoString leafName;
    url->GetFileName(leafName);
    if (!leafName.IsEmpty())
    {
      rv = UnescapeFragment(leafName, url, aFileName);
      if (NS_FAILED(rv))
      {
        CopyUTF8toUTF16(leafName, aFileName); // use escaped name
      }
    }
  }

  // Extract Extension, if we have a filename; otherwise,
  // truncate the string
  if (aExtension.IsEmpty()) {
    if (!aFileName.IsEmpty())
    {
      // Windows ignores terminating dots. So we have to as well, so
      // that our security checks do "the right thing"
      aFileName.Trim(".", false);

      // XXX RFindCharInReadable!!
      nsAutoString fileNameStr(aFileName);
      PRInt32 idx = fileNameStr.RFindChar(PRUnichar('.'));
      if (idx != kNotFound)
        CopyUTF16toUTF8(StringTail(fileNameStr, fileNameStr.Length() - idx - 1), aExtension);
    }
  }


  return handleExternally;
}

/**
 * Obtains the directory to use.  This tends to vary per platform, and
 * needs to be consistent throughout our codepaths. For platforms where
 * helper apps use the downloads directory, this should be kept in
 * sync with nsDownloadManager.cpp
 */
static nsresult GetDownloadDirectory(nsIFile **_directory)
{
  nsCOMPtr<nsIFile> dir;
#ifdef XP_MACOSX
  // On OS X, we first try to get the users download location, if it's set.
  switch (Preferences::GetInt(NS_PREF_DOWNLOAD_FOLDERLIST, -1)) {
    case NS_FOLDER_VALUE_DESKTOP:
      (void) NS_GetSpecialDirectory(NS_OS_DESKTOP_DIR, getter_AddRefs(dir));
      break;
    case NS_FOLDER_VALUE_CUSTOM:
      {
        Preferences::GetComplex(NS_PREF_DOWNLOAD_DIR,
                                NS_GET_IID(nsIFile),
                                getter_AddRefs(dir));
        if (!dir) break;

        // We have the directory, and now we need to make sure it exists
        bool dirExists = false;
        (void) dir->Exists(&dirExists);
        if (dirExists) break;

        nsresult rv = dir->Create(nsIFile::DIRECTORY_TYPE, 0755);
        if (NS_FAILED(rv)) {
          dir = nsnull;
          break;
        }
      }
      break;
    case NS_FOLDER_VALUE_DOWNLOADS:
      // This is just the OS default location, so fall out
      break;
  }

  if (!dir) {
    // If not, we default to the OS X default download location.
    nsresult rv = NS_GetSpecialDirectory(NS_OSX_DEFAULT_DOWNLOAD_DIR,
                                         getter_AddRefs(dir));
    NS_ENSURE_SUCCESS(rv, rv);
  }
#elif defined(ANDROID)
  // On mobile devices, we are avoiding exposing users to the file
  // system, and don't save downloads to temp directories

  // On Android we only return something if we have and SD-card
  char* downloadDir = getenv("DOWNLOADS_DIRECTORY");
  nsresult rv;
  if (downloadDir) {
    nsCOMPtr<nsIFile> ldir; 
    rv = NS_NewNativeLocalFile(nsDependentCString(downloadDir),
                               true, getter_AddRefs(ldir));
    NS_ENSURE_SUCCESS(rv, rv);
    dir = do_QueryInterface(ldir);
  }
  else {
    return NS_ERROR_FAILURE;
  }
#elif defined(MOZ_PLATFORM_MAEMO)
  nsresult rv = NS_GetSpecialDirectory(NS_UNIX_XDG_DOCUMENTS_DIR, getter_AddRefs(dir));
  NS_ENSURE_SUCCESS(rv, rv);
#else
  // On all other platforms, we default to the systems temporary directory.
  nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(dir));
  NS_ENSURE_SUCCESS(rv, rv);
#endif

  NS_ASSERTION(dir, "Somehow we didn't get a download directory!");
  dir.forget(_directory);
  return NS_OK;
}

/**
 * Structure for storing extension->type mappings.
 * @see defaultMimeEntries
 */
struct nsDefaultMimeTypeEntry {
  const char* mMimeType;
  const char* mFileExtension;
};

/**
 * Default extension->mimetype mappings. These are not overridable.
 * If you add types here, make sure they are lowercase, or you'll regret it.
 */
static nsDefaultMimeTypeEntry defaultMimeEntries [] = 
{
  // The following are those extensions that we're asked about during startup,
  // sorted by order used
  { IMAGE_GIF, "gif" },
  { TEXT_XML, "xml" },
  { APPLICATION_RDF, "rdf" },
  { TEXT_XUL, "xul" },
  { IMAGE_PNG, "png" },
  // -- end extensions used during startup
  { TEXT_CSS, "css" },
  { IMAGE_JPG, "jpeg" },
  { IMAGE_JPG, "jpg" },
  { TEXT_HTML, "html" },
  { TEXT_HTML, "htm" },
  { APPLICATION_XPINSTALL, "xpi" },
  { "application/xhtml+xml", "xhtml" },
  { "application/xhtml+xml", "xht" },
  { TEXT_PLAIN, "txt" },
#ifdef MOZ_OGG
  { VIDEO_OGG, "ogv" },
  { VIDEO_OGG, "ogg" },
  { APPLICATION_OGG, "ogg" },
  { AUDIO_OGG, "oga" },
#ifdef MOZ_OPUS
  { AUDIO_OGG, "opus" },
#endif
#endif
#ifdef MOZ_WEBM
  { VIDEO_WEBM, "webm" },
  { AUDIO_WEBM, "webm" },
#endif
#ifdef MOZ_GSTREAMER
  { VIDEO_MP4, "mp4" },
#endif
#ifdef MOZ_RAW
  { VIDEO_RAW, "yuv" }
#endif
};

/**
 * This is a small private struct used to help us initialize some
 * default mime types.
 */
struct nsExtraMimeTypeEntry {
  const char* mMimeType; 
  const char* mFileExtensions;
  const char* mDescription;
};

#ifdef XP_MACOSX
#define MAC_TYPE(x) x
#else
#define MAC_TYPE(x) 0
#endif

/**
 * This table lists all of the 'extra' content types that we can deduce from particular
 * file extensions.  These entries also ensure that we provide a good descriptive name
 * when we encounter files with these content types and/or extensions.  These can be
 * overridden by user helper app prefs.
 * If you add types here, make sure they are lowercase, or you'll regret it.
 */
static nsExtraMimeTypeEntry extraMimeEntries [] =
{
#if defined(VMS)
  { APPLICATION_OCTET_STREAM, "exe,com,bin,sav,bck,pcsi,dcx_axpexe,dcx_vaxexe,sfx_axpexe,sfx_vaxexe", "Binary File" },
#elif defined(XP_MACOSX) // don't define .bin on the mac...use internet config to look that up...
  { APPLICATION_OCTET_STREAM, "exe,com", "Binary File" },
#else
  { APPLICATION_OCTET_STREAM, "exe,com,bin", "Binary File" },
#endif
  { APPLICATION_GZIP2, "gz", "gzip" },
  { "application/x-arj", "arj", "ARJ file" },
  { "application/rtf", "rtf", "Rich Text Format File" },
  { APPLICATION_XPINSTALL, "xpi", "XPInstall Install" },
  { APPLICATION_POSTSCRIPT, "ps,eps,ai", "Postscript File" },
  { APPLICATION_XJAVASCRIPT, "js", "Javascript Source File" },
#ifdef MOZ_WIDGET_ANDROID
  { "application/vnd.android.package-archive", "apk", "Android Package" },
#endif
  { IMAGE_ART, "art", "ART Image" },
  { IMAGE_BMP, "bmp", "BMP Image" },
  { IMAGE_GIF, "gif", "GIF Image" },
  { IMAGE_ICO, "ico,cur", "ICO Image" },
  { IMAGE_JPG, "jpeg,jpg,jfif,pjpeg,pjp", "JPEG Image" },
  { IMAGE_PNG, "png", "PNG Image" },
  { IMAGE_TIFF, "tiff,tif", "TIFF Image" },
  { IMAGE_XBM, "xbm", "XBM Image" },
  { "image/svg+xml", "svg", "Scalable Vector Graphics" },
  { MESSAGE_RFC822, "eml", "RFC-822 data" },
  { TEXT_PLAIN, "txt,text", "Text File" },
  { TEXT_HTML, "html,htm,shtml,ehtml", "HyperText Markup Language" },
  { "application/xhtml+xml", "xhtml,xht", "Extensible HyperText Markup Language" },
  { APPLICATION_MATHML_XML, "mml", "Mathematical Markup Language" },
  { APPLICATION_RDF, "rdf", "Resource Description Framework" },
  { TEXT_XUL, "xul", "XML-Based User Interface Language" },
  { TEXT_XML, "xml,xsl,xbl", "Extensible Markup Language" },
  { TEXT_CSS, "css", "Style Sheet" },
  { VIDEO_OGG, "ogv", "Ogg Video" },
  { VIDEO_OGG, "ogg", "Ogg Video" },
  { APPLICATION_OGG, "ogg", "Ogg Video"},
  { AUDIO_OGG, "oga", "Ogg Audio" },
  { AUDIO_OGG, "opus", "Opus Audio" },
  { VIDEO_WEBM, "webm", "Web Media Video" },
  { AUDIO_WEBM, "webm", "Web Media Audio" },
  { VIDEO_MP4, "mp4", "MPEG-4 Video" },
  { VIDEO_RAW, "yuv", "Raw YUV Video" },
  { AUDIO_WAV, "wav", "Waveform Audio" }
};

#undef MAC_TYPE

/**
 * File extensions for which decoding should be disabled.
 * NOTE: These MUST be lower-case and ASCII.
 */
static nsDefaultMimeTypeEntry nonDecodableExtensions [] = {
  { APPLICATION_GZIP, "gz" }, 
  { APPLICATION_GZIP, "tgz" },
  { APPLICATION_ZIP, "zip" },
  { APPLICATION_COMPRESS, "z" },
  { APPLICATION_GZIP, "svgz" }
};

NS_IMPL_ISUPPORTS6(
  nsExternalHelperAppService,
  nsIExternalHelperAppService,
  nsPIExternalAppLauncher,
  nsIExternalProtocolService,
  nsIMIMEService,
  nsIObserver,
  nsISupportsWeakReference)

nsExternalHelperAppService::nsExternalHelperAppService()
{
}
nsresult nsExternalHelperAppService::Init()
{
  // Add an observer for profile change
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs)
    return NS_ERROR_FAILURE;

#ifdef PR_LOGGING
  if (!mLog) {
    mLog = PR_NewLogModule("HelperAppService");
    if (!mLog)
      return NS_ERROR_OUT_OF_MEMORY;
  }
#endif

  nsresult rv = obs->AddObserver(this, "profile-before-change", true);
  NS_ENSURE_SUCCESS(rv, rv);
  return obs->AddObserver(this, "last-pb-context-exited", true);
}

nsExternalHelperAppService::~nsExternalHelperAppService()
{
}

static PRInt64 GetContentLengthAsInt64(nsIRequest *request)
{
  PRInt64 contentLength = -1;
  nsresult rv;
  nsCOMPtr<nsIPropertyBag2> props(do_QueryInterface(request, &rv));
  if (props)
    rv = props->GetPropertyAsInt64(NS_CHANNEL_PROP_CONTENT_LENGTH, &contentLength);

  if (NS_FAILED(rv)) {
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    if (channel) {
      PRInt32 smallLen;
      channel->GetContentLength(&smallLen);
      contentLength = smallLen;
    }
  }

  return contentLength;
}

NS_IMETHODIMP nsExternalHelperAppService::DoContent(const nsACString& aMimeContentType,
                                                    nsIRequest *aRequest,
                                                    nsIInterfaceRequestor *aWindowContext,
                                                    bool aForceSave,
                                                    nsIStreamListener ** aStreamListener)
{
  nsAutoString fileName;
  nsCAutoString fileExtension;
  PRUint32 reason = nsIHelperAppLauncherDialog::REASON_CANTHANDLE;
  nsresult rv;

  // Get the file extension and name that we will need later
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  nsCOMPtr<nsIURI> uri;
  if (channel)
    channel->GetURI(getter_AddRefs(uri));

  PRInt64 contentLength = GetContentLengthAsInt64(aRequest);
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    // We need to get a hold of a ContentChild so that we can begin forwarding
    // this data to the parent.  In the HTTP case, this is unfortunate, since
    // we're actually passing data from parent->child->parent wastefully, but
    // the Right Fix will eventually be to short-circuit those channels on the
    // parent side based on some sort of subscription concept.
    using mozilla::dom::ContentChild;
    using mozilla::dom::ExternalHelperAppChild;
    ContentChild *child = ContentChild::GetSingleton();
    if (!child)
      return NS_ERROR_FAILURE;

    nsCString disp;
    if (channel)
      channel->GetContentDispositionHeader(disp);

    nsCOMPtr<nsIURI> referrer;
    rv = NS_GetReferrerFromChannel(channel, getter_AddRefs(referrer));

    // Now we build a protocol for forwarding our data to the parent.  The
    // protocol will act as a listener on the child-side and create a "real"
    // helperAppService listener on the parent-side, via another call to
    // DoContent.
    mozilla::dom::PExternalHelperAppChild *pc;
    pc = child->SendPExternalHelperAppConstructor(IPC::URI(uri),
                                                  nsCString(aMimeContentType),
                                                  disp,
                                                  aForceSave, contentLength,
                                                  IPC::URI(referrer));
    ExternalHelperAppChild *childListener = static_cast<ExternalHelperAppChild *>(pc);

    NS_ADDREF(*aStreamListener = childListener);

    nsRefPtr<nsExternalAppHandler> handler =
      new nsExternalAppHandler(nsnull, EmptyCString(), aWindowContext, this,
                               fileName,
                               reason, aForceSave);
    if (!handler)
      return NS_ERROR_OUT_OF_MEMORY;
    
    childListener->SetHandler(handler);

    return NS_OK;
  }

  if (channel) {
    // Check if we have a POST request, in which case we don't want to use
    // the url's extension
    bool allowURLExt = true;
    nsCOMPtr<nsIHttpChannel> httpChan = do_QueryInterface(channel);
    if (httpChan) {
      nsCAutoString requestMethod;
      httpChan->GetRequestMethod(requestMethod);
      allowURLExt = !requestMethod.Equals("POST");
    }

    // Check if we had a query string - we don't want to check the URL
    // extension if a query is present in the URI
    // If we already know we don't want to check the URL extension, don't
    // bother checking the query
    if (uri && allowURLExt) {
      nsCOMPtr<nsIURL> url = do_QueryInterface(uri);

      if (url) {
        nsCAutoString query;

        // We only care about the query for HTTP and HTTPS URLs
        bool isHTTP, isHTTPS;
        rv = uri->SchemeIs("http", &isHTTP);
        if (NS_FAILED(rv))
          isHTTP = false;
        rv = uri->SchemeIs("https", &isHTTPS);
        if (NS_FAILED(rv))
          isHTTPS = false;

        if (isHTTP || isHTTPS)
          url->GetQuery(query);

        // Only get the extension if the query is empty; if it isn't, then the
        // extension likely belongs to a cgi script and isn't helpful
        allowURLExt = query.IsEmpty();
      }
    }
    // Extract name & extension
    bool isAttachment = GetFilenameAndExtensionFromChannel(channel, fileName,
                                                             fileExtension,
                                                             allowURLExt);
    LOG(("Found extension '%s' (filename is '%s', handling attachment: %i)",
         fileExtension.get(), NS_ConvertUTF16toUTF8(fileName).get(),
         isAttachment));
    if (isAttachment)
      reason = nsIHelperAppLauncherDialog::REASON_SERVERREQUEST;
  }

  LOG(("HelperAppService::DoContent: mime '%s', extension '%s'\n",
       PromiseFlatCString(aMimeContentType).get(), fileExtension.get()));

  // we get the mime service here even though we're the default implementation of it,
  // so it's possible to override only the mime service and not need to reimplement the
  // whole external helper app service itself
  nsCOMPtr<nsIMIMEService> mimeSvc(do_GetService(NS_MIMESERVICE_CONTRACTID));
  NS_ENSURE_TRUE(mimeSvc, NS_ERROR_FAILURE);

  // Try to find a mime object by looking at the mime type/extension
  nsCOMPtr<nsIMIMEInfo> mimeInfo;
  if (aMimeContentType.Equals(APPLICATION_GUESS_FROM_EXT, nsCaseInsensitiveCStringComparator())) {
    nsCAutoString mimeType;
    if (!fileExtension.IsEmpty()) {
      mimeSvc->GetFromTypeAndExtension(EmptyCString(), fileExtension, getter_AddRefs(mimeInfo));
      if (mimeInfo) {
        mimeInfo->GetMIMEType(mimeType);

        LOG(("OS-Provided mime type '%s' for extension '%s'\n", 
             mimeType.get(), fileExtension.get()));
      }
    }

    if (fileExtension.IsEmpty() || mimeType.IsEmpty()) {
      // Extension lookup gave us no useful match
      mimeSvc->GetFromTypeAndExtension(NS_LITERAL_CSTRING(APPLICATION_OCTET_STREAM), fileExtension,
                                       getter_AddRefs(mimeInfo));
      mimeType.AssignLiteral(APPLICATION_OCTET_STREAM);
    }
    if (channel)
      channel->SetContentType(mimeType);
    // Don't overwrite SERVERREQUEST
    if (reason == nsIHelperAppLauncherDialog::REASON_CANTHANDLE)
      reason = nsIHelperAppLauncherDialog::REASON_TYPESNIFFED;
  } 
  else {
    mimeSvc->GetFromTypeAndExtension(aMimeContentType, fileExtension,
                                     getter_AddRefs(mimeInfo));
  } 
  LOG(("Type/Ext lookup found 0x%p\n", mimeInfo.get()));

  // No mimeinfo -> we can't continue. probably OOM.
  if (!mimeInfo)
    return NS_ERROR_OUT_OF_MEMORY;

  *aStreamListener = nsnull;
  // We want the mimeInfo's primary extension to pass it to
  // nsExternalAppHandler
  nsCAutoString buf;
  mimeInfo->GetPrimaryExtension(buf);

  nsExternalAppHandler * handler = new nsExternalAppHandler(mimeInfo,
                                                            buf,
                                                            aWindowContext,
                                                            this,
                                                            fileName,
                                                            reason,
                                                            aForceSave);
  if (!handler)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aStreamListener = handler);
  
  return NS_OK;
}

NS_IMETHODIMP nsExternalHelperAppService::ApplyDecodingForExtension(const nsACString& aExtension,
                                                                    const nsACString& aEncodingType,
                                                                    bool *aApplyDecoding)
{
  *aApplyDecoding = true;
  PRUint32 i;
  for(i = 0; i < ArrayLength(nonDecodableExtensions); ++i) {
    if (aExtension.LowerCaseEqualsASCII(nonDecodableExtensions[i].mFileExtension) &&
        aEncodingType.LowerCaseEqualsASCII(nonDecodableExtensions[i].mMimeType)) {
      *aApplyDecoding = false;
      break;
    }
  }
  return NS_OK;
}

nsresult nsExternalHelperAppService::GetFileTokenForPath(const PRUnichar * aPlatformAppPath,
                                                         nsIFile ** aFile)
{
  nsDependentString platformAppPath(aPlatformAppPath);
  // First, check if we have an absolute path
  nsIFile* localFile = nsnull;
  nsresult rv = NS_NewLocalFile(platformAppPath, true, &localFile);
  if (NS_SUCCEEDED(rv)) {
    *aFile = localFile;
    bool exists;
    if (NS_FAILED((*aFile)->Exists(&exists)) || !exists) {
      NS_RELEASE(*aFile);
      return NS_ERROR_FILE_NOT_FOUND;
    }
    return NS_OK;
  }


  // Second, check if file exists in mozilla program directory
  rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR, aFile);
  if (NS_SUCCEEDED(rv)) {
    rv = (*aFile)->Append(platformAppPath);
    if (NS_SUCCEEDED(rv)) {
      bool exists = false;
      rv = (*aFile)->Exists(&exists);
      if (NS_SUCCEEDED(rv) && exists)
        return NS_OK;
    }
    NS_RELEASE(*aFile);
  }


  return NS_ERROR_NOT_AVAILABLE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// begin external protocol service default implementation...
//////////////////////////////////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsExternalHelperAppService::ExternalProtocolHandlerExists(const char * aProtocolScheme,
                                                                        bool * aHandlerExists)
{
  nsCOMPtr<nsIHandlerInfo> handlerInfo;
  nsresult rv = GetProtocolHandlerInfo(nsDependentCString(aProtocolScheme), 
                                       getter_AddRefs(handlerInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  // See if we have any known possible handler apps for this
  nsCOMPtr<nsIMutableArray> possibleHandlers;
  handlerInfo->GetPossibleApplicationHandlers(getter_AddRefs(possibleHandlers));

  PRUint32 length;
  possibleHandlers->GetLength(&length);
  if (length) {
    *aHandlerExists = true;
    return NS_OK;
  }

  // if not, fall back on an os-based handler
  return OSProtocolHandlerExists(aProtocolScheme, aHandlerExists);
}

NS_IMETHODIMP nsExternalHelperAppService::IsExposedProtocol(const char * aProtocolScheme, bool * aResult)
{
  // check the per protocol setting first.  it always takes precedence.
  // if not set, then use the global setting.

  nsCAutoString prefName("network.protocol-handler.expose.");
  prefName += aProtocolScheme;
  bool val;
  if (NS_SUCCEEDED(Preferences::GetBool(prefName.get(), &val))) {
    *aResult = val;
    return NS_OK;
  }

  // by default, no protocol is exposed.  i.e., by default all link clicks must
  // go through the external protocol service.  most applications override this
  // default behavior.
  *aResult =
    Preferences::GetBool("network.protocol-handler.expose-all", false);

  return NS_OK;
}

NS_IMETHODIMP nsExternalHelperAppService::LoadUrl(nsIURI * aURL)
{
  return LoadURI(aURL, nsnull);
}

static const char kExternalProtocolPrefPrefix[]  = "network.protocol-handler.external.";
static const char kExternalProtocolDefaultPref[] = "network.protocol-handler.external-default";

NS_IMETHODIMP 
nsExternalHelperAppService::LoadURI(nsIURI *aURI,
                                    nsIInterfaceRequestor *aWindowContext)
{
  NS_ENSURE_ARG_POINTER(aURI);

  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    mozilla::dom::ContentChild::GetSingleton()->SendLoadURIExternal(aURI);
    return NS_OK;
  }

  nsCAutoString spec;
  aURI->GetSpec(spec);

  if (spec.Find("%00") != -1)
    return NS_ERROR_MALFORMED_URI;

  spec.ReplaceSubstring("\"", "%22");
  spec.ReplaceSubstring("`", "%60");
  
  nsCOMPtr<nsIIOService> ios(do_GetIOService());
  nsCOMPtr<nsIURI> uri;
  nsresult rv = ios->NewURI(spec, nsnull, nsnull, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString scheme;
  uri->GetScheme(scheme);
  if (scheme.IsEmpty())
    return NS_OK; // must have a scheme

  // Deny load if the prefs say to do so
  nsCAutoString externalPref(kExternalProtocolPrefPrefix);
  externalPref += scheme;
  bool allowLoad  = false;
  if (NS_FAILED(Preferences::GetBool(externalPref.get(), &allowLoad))) {
    // no scheme-specific value, check the default
    if (NS_FAILED(Preferences::GetBool(kExternalProtocolDefaultPref,
                                       &allowLoad))) {
      return NS_OK; // missing default pref
    }
  }

  if (!allowLoad) {
    return NS_OK; // explicitly denied
  }

 
  nsCOMPtr<nsIHandlerInfo> handler;
  rv = GetProtocolHandlerInfo(scheme, getter_AddRefs(handler));
  NS_ENSURE_SUCCESS(rv, rv);

  nsHandlerInfoAction preferredAction;
  handler->GetPreferredAction(&preferredAction);
  bool alwaysAsk = true;
  handler->GetAlwaysAskBeforeHandling(&alwaysAsk);

  // if we are not supposed to ask, and the preferred action is to use
  // a helper app or the system default, we just launch the URI.
  if (!alwaysAsk && (preferredAction == nsIHandlerInfo::useHelperApp ||
                     preferredAction == nsIHandlerInfo::useSystemDefault))
    return handler->LaunchWithURI(uri, aWindowContext);
  
  nsCOMPtr<nsIContentDispatchChooser> chooser =
    do_CreateInstance("@mozilla.org/content-dispatch-chooser;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  return chooser->Ask(handler, aWindowContext, uri,
                      nsIContentDispatchChooser::REASON_CANNOT_HANDLE);
}

NS_IMETHODIMP nsExternalHelperAppService::GetApplicationDescription(const nsACString& aScheme, nsAString& _retval)
{
  // this method should only be implemented by each OS specific implementation of this service.
  return NS_ERROR_NOT_IMPLEMENTED;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
// Methods related to deleting temporary files on exit
//////////////////////////////////////////////////////////////////////////////////////////////////////

/* static */
nsresult
nsExternalHelperAppService::DeleteTemporaryFileHelper(nsIFile * aTemporaryFile,
                                                      nsCOMArray<nsIFile> &aFileList)
{
  bool isFile = false;

  // as a safety measure, make sure the nsIFile is really a file and not a directory object.
  aTemporaryFile->IsFile(&isFile);
  if (!isFile) return NS_OK;

  aFileList.AppendObject(aTemporaryFile);

  return NS_OK;
}

NS_IMETHODIMP
nsExternalHelperAppService::DeleteTemporaryFileOnExit(nsIFile* aTemporaryFile)
{
  return DeleteTemporaryFileHelper(aTemporaryFile, mTemporaryFilesList);
}

NS_IMETHODIMP
nsExternalHelperAppService::DeleteTemporaryPrivateFileWhenPossible(nsIFile* aTemporaryFile)
{
  return DeleteTemporaryFileHelper(aTemporaryFile, mTemporaryPrivateFilesList);
}

void nsExternalHelperAppService::FixFilePermissions(nsIFile* aFile)
{
  // This space intentionally left blank
}

void nsExternalHelperAppService::ExpungeTemporaryFilesHelper(nsCOMArray<nsIFile> &fileList)
{
  PRInt32 numEntries = fileList.Count();
  nsIFile* localFile;
  for (PRInt32 index = 0; index < numEntries; index++)
  {
    localFile = fileList[index];
    if (localFile) {
      // First make the file writable, since the temp file is probably readonly.
      localFile->SetPermissions(0600);
      localFile->Remove(false);
    }
  }

  fileList.Clear();
}

void nsExternalHelperAppService::ExpungeTemporaryFiles()
{
  ExpungeTemporaryFilesHelper(mTemporaryFilesList);
}

void nsExternalHelperAppService::ExpungeTemporaryPrivateFiles()
{
  ExpungeTemporaryFilesHelper(mTemporaryPrivateFilesList);
}

static const char kExternalWarningPrefPrefix[] = 
  "network.protocol-handler.warn-external.";
static const char kExternalWarningDefaultPref[] = 
  "network.protocol-handler.warn-external-default";

NS_IMETHODIMP
nsExternalHelperAppService::GetProtocolHandlerInfo(const nsACString &aScheme,
                                                   nsIHandlerInfo **aHandlerInfo)
{
  // XXX enterprise customers should be able to turn this support off with a
  // single master pref (maybe use one of the "exposed" prefs here?)

  bool exists;
  nsresult rv = GetProtocolHandlerInfoFromOS(aScheme, &exists, aHandlerInfo);
  if (NS_FAILED(rv)) {
    // Either it knows nothing, or we ran out of memory
    return NS_ERROR_FAILURE;
  }
  
  nsCOMPtr<nsIHandlerService> handlerSvc = do_GetService(NS_HANDLERSERVICE_CONTRACTID);
  if (handlerSvc) {
    bool hasHandler = false;
    (void) handlerSvc->Exists(*aHandlerInfo, &hasHandler);
    if (hasHandler) {
      rv = handlerSvc->FillHandlerInfo(*aHandlerInfo, EmptyCString());
      if (NS_SUCCEEDED(rv))
        return NS_OK;
    }
  }
  
  return SetProtocolHandlerDefaults(*aHandlerInfo, exists);
}

NS_IMETHODIMP
nsExternalHelperAppService::GetProtocolHandlerInfoFromOS(const nsACString &aScheme,
                                                         bool *found,
                                                         nsIHandlerInfo **aHandlerInfo)
{
  // intended to be implemented by the subclass
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsExternalHelperAppService::SetProtocolHandlerDefaults(nsIHandlerInfo *aHandlerInfo,
                                                       bool aOSHandlerExists)
{
  // this type isn't in our database, so we've only got an OS default handler,
  // if one exists

  if (aOSHandlerExists) {
    // we've got a default, so use it
    aHandlerInfo->SetPreferredAction(nsIHandlerInfo::useSystemDefault);

    // whether or not to ask the user depends on the warning preference
    nsCAutoString scheme;
    aHandlerInfo->GetType(scheme);
    
    nsCAutoString warningPref(kExternalWarningPrefPrefix);
    warningPref += scheme;
    bool warn;
    if (NS_FAILED(Preferences::GetBool(warningPref.get(), &warn))) {
      // no scheme-specific value, check the default
      warn = Preferences::GetBool(kExternalWarningDefaultPref, true);
    }
    aHandlerInfo->SetAlwaysAskBeforeHandling(warn);
  } else {
    // If no OS default existed, we set the preferred action to alwaysAsk. 
    // This really means not initialized (i.e. there's no available handler)
    // to all the code...
    aHandlerInfo->SetPreferredAction(nsIHandlerInfo::alwaysAsk);
  }

  return NS_OK;
}
 
// XPCOM profile change observer
NS_IMETHODIMP
nsExternalHelperAppService::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData )
{
  if (!strcmp(aTopic, "profile-before-change")) {
    ExpungeTemporaryFiles();
  } else if (!strcmp(aTopic, "last-pb-context-exited")) {
    ExpungeTemporaryPrivateFiles();
  }
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// begin external app handler implementation 
//////////////////////////////////////////////////////////////////////////////////////////////////////

NS_IMPL_THREADSAFE_ADDREF(nsExternalAppHandler)
NS_IMPL_THREADSAFE_RELEASE(nsExternalAppHandler)

NS_INTERFACE_MAP_BEGIN(nsExternalAppHandler)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
   NS_INTERFACE_MAP_ENTRY(nsIHelperAppLauncher)   
   NS_INTERFACE_MAP_ENTRY(nsICancelable)
   NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
NS_INTERFACE_MAP_END_THREADSAFE

nsExternalAppHandler::nsExternalAppHandler(nsIMIMEInfo * aMIMEInfo,
                                           const nsCSubstring& aTempFileExtension,
                                           nsIInterfaceRequestor* aWindowContext,
                                           nsExternalHelperAppService *aExtProtSvc,
                                           const nsAString& aSuggestedFilename,
                                           PRUint32 aReason, bool aForceSave)
: mMimeInfo(aMIMEInfo)
, mWindowContext(aWindowContext)
, mWindowToClose(nsnull)
, mSuggestedFileName(aSuggestedFilename)
, mForceSave(aForceSave)
, mCanceled(false)
, mShouldCloseWindow(false)
, mReceivedDispositionInfo(false)
, mStopRequestIssued(false)
, mProgressListenerInitialized(false)
, mReason(aReason)
, mContentLength(-1)
, mProgress(0)
, mDataBuffer(nsnull)
, mKeepRequestAlive(false)
, mRequest(nsnull)
, mExtProtSvc(aExtProtSvc)
{

  // make sure the extention includes the '.'
  if (!aTempFileExtension.IsEmpty() && aTempFileExtension.First() != '.')
    mTempFileExtension = PRUnichar('.');
  AppendUTF8toUTF16(aTempFileExtension, mTempFileExtension);

  // replace platform specific path separator and illegal characters to avoid any confusion
  mSuggestedFileName.ReplaceChar(FILE_PATH_SEPARATOR FILE_ILLEGAL_CHARACTERS, '_');
  mTempFileExtension.ReplaceChar(FILE_PATH_SEPARATOR FILE_ILLEGAL_CHARACTERS, '_');

  // Remove unsafe bidi characters which might have spoofing implications (bug 511521).
  const PRUnichar unsafeBidiCharacters[] = {
    PRUnichar(0x202a), // Left-to-Right Embedding
    PRUnichar(0x202b), // Right-to-Left Embedding
    PRUnichar(0x202c), // Pop Directional Formatting
    PRUnichar(0x202d), // Left-to-Right Override
    PRUnichar(0x202e)  // Right-to-Left Override
  };
  for (PRUint32 i = 0; i < ArrayLength(unsafeBidiCharacters); ++i) {
    mSuggestedFileName.ReplaceChar(unsafeBidiCharacters[i], '_');
    mTempFileExtension.ReplaceChar(unsafeBidiCharacters[i], '_');
  }
  
  // Make sure extension is correct.
  EnsureSuggestedFileName();

  mBufferSize = Preferences::GetUint("network.buffer.cache.size", 4096);
  mDataBuffer = (char*) malloc(mBufferSize);
  if (!mDataBuffer)
    return;
}

nsExternalAppHandler::~nsExternalAppHandler()
{
  if (mDataBuffer)
    free(mDataBuffer);
}

NS_IMETHODIMP nsExternalAppHandler::SetWebProgressListener(nsIWebProgressListener2 * aWebProgressListener)
{ 
  // this call back means we've successfully brought up the 
  // progress window so set the appropriate flag, even though
  // aWebProgressListener might be null
  
  if (mReceivedDispositionInfo)
    mProgressListenerInitialized = true;

  // Go ahead and register the progress listener....
  mWebProgressListener = aWebProgressListener;

  // while we were bringing up the progress dialog, we actually finished processing the
  // url. If that's the case then mStopRequestIssued will be true. We need to execute the
  // operation since we are actually done now.
  if (mStopRequestIssued && aWebProgressListener)
  {
    return ExecuteDesiredAction();
  }

  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::GetTargetFile(nsIFile** aTarget)
{
  if (mFinalFileDestination)
    *aTarget = mFinalFileDestination;
  else
    *aTarget = mTempFile;

  NS_IF_ADDREF(*aTarget);
  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::GetTargetFileIsExecutable(bool *aExec)
{
  // Use the real target if it's been set
  if (mFinalFileDestination)
    return mFinalFileDestination->IsExecutable(aExec);

  // Otherwise, use the stored executable-ness of the temporary
  *aExec = mTempFileIsExecutable;
  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::GetTimeDownloadStarted(PRTime* aTime)
{
  *aTime = mTimeDownloadStarted;
  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::GetContentLength(PRInt64 *aContentLength)
{
  *aContentLength = mContentLength;
  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::CloseProgressWindow()
{
  // release extra state...
  mWebProgressListener = nsnull;
  return NS_OK;
}

void nsExternalAppHandler::RetargetLoadNotifications(nsIRequest *request)
{
  // we are going to run the downloading of the helper app in our own little docloader / load group context. 
  // so go ahead and force the creation of a load group and doc loader for us to use...
  nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(request);
  if (!aChannel)
    return;

  // we need to store off the original (pre redirect!) channel that initiated the load. We do
  // this so later on, we can pass any refresh urls associated with the original channel back to the 
  // window context which started the whole process. More comments about that are listed below....
  // HACK ALERT: it's pretty bogus that we are getting the document channel from the doc loader. 
  // ideally we should be able to just use mChannel (the channel we are extracting content from) or
  // the default load channel associated with the original load group. Unfortunately because
  // a redirect may have occurred, the doc loader is the only one with a ptr to the original channel 
  // which is what we really want....

  // Note that we need to do this before removing aChannel from the loadgroup,
  // since that would mess with the original channel on the loader.
  nsCOMPtr<nsIDocumentLoader> origContextLoader =
    do_GetInterface(mWindowContext);
  if (origContextLoader)
    origContextLoader->GetDocumentChannel(getter_AddRefs(mOriginalChannel));

  nsCOMPtr<nsILoadGroup> oldLoadGroup;
  aChannel->GetLoadGroup(getter_AddRefs(oldLoadGroup));

  if(oldLoadGroup)
     oldLoadGroup->RemoveRequest(request, nsnull, NS_BINDING_RETARGETED);
      
  aChannel->SetLoadGroup(nsnull);
  aChannel->SetNotificationCallbacks(nsnull);
}

/**
 * Make mTempFileExtension contain an extension exactly when its previous value
 * is different from mSuggestedFileName's extension, so that it can be appended
 * to mSuggestedFileName and form a valid, useful leaf name.
 * This is required so that the (renamed) temporary file has the correct extension
 * after downloading to make sure the OS will launch the application corresponding
 * to the MIME type (which was used to calculate mTempFileExtension).  This prevents
 * a cgi-script named foobar.exe that returns application/zip from being named
 * foobar.exe and executed as an executable file. It also blocks content that
 * a web site might provide with a content-disposition header indicating
 * filename="foobar.exe" from being downloaded to a file with extension .exe
 * and executed.
 */
void nsExternalAppHandler::EnsureSuggestedFileName()
{
  // Make sure there is a mTempFileExtension (not "" or ".").
  // Remember that mTempFileExtension will always have the leading "."
  // (the check for empty is just to be safe).
  if (mTempFileExtension.Length() > 1)
  {
    // Get mSuggestedFileName's current extension.
    nsAutoString fileExt;
    PRInt32 pos = mSuggestedFileName.RFindChar('.');
    if (pos != kNotFound)
      mSuggestedFileName.Right(fileExt, mSuggestedFileName.Length() - pos);

    // Now, compare fileExt to mTempFileExtension.
    if (fileExt.Equals(mTempFileExtension, nsCaseInsensitiveStringComparator()))
    {
      // Matches -> mTempFileExtension can be empty
      mTempFileExtension.Truncate();
    }
  }
}

nsresult nsExternalAppHandler::SetUpTempFile(nsIChannel * aChannel)
{
  // First we need to try to get the destination directory for the temporary
  // file.
  nsresult rv = GetDownloadDirectory(getter_AddRefs(mTempFile));
  NS_ENSURE_SUCCESS(rv, rv);

  // At this point, we do not have a filename for the temp file.  For security
  // purposes, this cannot be predictable, so we must use a cryptographic
  // quality PRNG to generate one.
  // We will request raw random bytes, and transform that to a base64 string,
  // as all characters from the base64 set are acceptable for filenames.  For
  // each three bytes of random data, we will get four bytes of ASCII.  Request
  // a bit more, to be safe, and truncate to the length we want in the end.

  const PRUint32 wantedFileNameLength = 8;
  const PRUint32 requiredBytesLength =
    static_cast<PRUint32>((wantedFileNameLength + 1) / 4 * 3);

  nsCOMPtr<nsIRandomGenerator> rg =
    do_GetService("@mozilla.org/security/random-generator;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint8 *buffer;
  rv = rg->GenerateRandomBytes(requiredBytesLength, &buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  char *b64 = PL_Base64Encode(reinterpret_cast<const char *>(buffer),
                              requiredBytesLength, nsnull);
  NS_Free(buffer);
  buffer = nsnull;

  if (!b64)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ASSERTION(strlen(b64) >= wantedFileNameLength,
               "not enough bytes produced for conversion!");

  nsCAutoString tempLeafName(b64, wantedFileNameLength);
  PR_Free(b64);
  b64 = nsnull;

  // Base64 characters are alphanumeric (a-zA-Z0-9) and '+' and '/', so we need
  // to replace illegal characters -- notably '/'
  tempLeafName.ReplaceChar(FILE_PATH_SEPARATOR FILE_ILLEGAL_CHARACTERS, '_');

  // now append our extension.
  nsCAutoString ext;
  mMimeInfo->GetPrimaryExtension(ext);
  if (!ext.IsEmpty()) {
    ext.ReplaceChar(FILE_PATH_SEPARATOR FILE_ILLEGAL_CHARACTERS, '_');
    if (ext.First() != '.')
      tempLeafName.Append('.');
    tempLeafName.Append(ext);
  }

  // We need to temporarily create a dummy file with the correct
  // file extension to determine the executable-ness, so do this before adding
  // the extra .part extension.
  nsCOMPtr<nsIFile> dummyFile;
  rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(dummyFile));
  NS_ENSURE_SUCCESS(rv, rv);

  // Set the file name without .part
  rv = dummyFile->Append(NS_ConvertUTF8toUTF16(tempLeafName));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = dummyFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
  NS_ENSURE_SUCCESS(rv, rv);

  // Store executable-ness then delete
  dummyFile->IsExecutable(&mTempFileIsExecutable);
  dummyFile->Remove(false);

  // Add an additional .part to prevent the OS from running this file in the
  // default application.
  tempLeafName.Append(NS_LITERAL_CSTRING(".part"));

  rv = mTempFile->Append(NS_ConvertUTF8toUTF16(tempLeafName));
  // make this file unique!!!
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mTempFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIOutputStream> outputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), mTempFile,
                                   PR_WRONLY | PR_CREATE_FILE, 0600);
  if (NS_FAILED(rv)) {
    mTempFile->Remove(false);
    return rv;
  }

  mOutStream = NS_BufferOutputStream(outputStream, BUFFERED_OUTPUT_SIZE);

#if defined(XP_MACOSX) && !defined(__LP64__)
    nsCAutoString contentType;
    mMimeInfo->GetMIMEType(contentType);
    if (contentType.LowerCaseEqualsLiteral(APPLICATION_APPLEFILE) ||
        contentType.LowerCaseEqualsLiteral(MULTIPART_APPLEDOUBLE))
    {
      nsCOMPtr<nsIAppleFileDecoder> appleFileDecoder = do_CreateInstance(NS_IAPPLEFILEDECODER_CONTRACTID, &rv);
      if (NS_SUCCEEDED(rv))
      {
        rv = appleFileDecoder->Initialize(mOutStream, mTempFile);
        if (NS_SUCCEEDED(rv))
          mOutStream = do_QueryInterface(appleFileDecoder, &rv);
      }
    }
#endif

  return rv;
}

NS_IMETHODIMP nsExternalAppHandler::OnStartRequest(nsIRequest *request, nsISupports * aCtxt)
{
  NS_PRECONDITION(request, "OnStartRequest without request?");

  // Set mTimeDownloadStarted here as the download has already started and
  // we want to record the start time before showing the filepicker.
  mTimeDownloadStarted = PR_Now();

  mRequest = request;

  nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(request);
  
  nsresult rv;
  
  nsCOMPtr<nsIFileChannel> fileChan(do_QueryInterface(request));
  mIsFileChannel = fileChan != nsnull;

  // Get content length
  mContentLength = GetContentLengthAsInt64(request);

  nsCOMPtr<nsIPropertyBag2> props(do_QueryInterface(request, &rv));
  // Determine whether a new window was opened specifically for this request
  if (props) {
    bool tmp = false;
    props->GetPropertyAsBool(NS_LITERAL_STRING("docshell.newWindowTarget"),
                             &tmp);
    mShouldCloseWindow = tmp;
  }

  // Now get the URI
  if (aChannel)
  {
    aChannel->GetURI(getter_AddRefs(mSourceUrl));
  }

  // retarget all load notifications to our docloader instead of the original window's docloader...
  RetargetLoadNotifications(request);

  // Check to see if there is a refresh header on the original channel.
  if (mOriginalChannel) {
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mOriginalChannel));
    if (httpChannel) {
      nsCAutoString refreshHeader;
      httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("refresh"),
                                     refreshHeader);
      if (!refreshHeader.IsEmpty()) {
        mShouldCloseWindow = false;
      }
    }
  }

  // Close the underlying DOMWindow if there is no refresh header
  // and it was opened specifically for the download
  MaybeCloseWindow();

  // In an IPC setting, we're allowing the child process, here, to make
  // decisions about decoding the channel (e.g. decompression).  It will
  // still forward the decoded (uncompressed) data back to the parent.
  // Con: Uncompressed data means more IPC overhead.
  // Pros: ExternalHelperAppParent doesn't need to implement nsIEncodedChannel.
  //       Parent process doesn't need to expect CPU time on decompression.
  nsCOMPtr<nsIEncodedChannel> encChannel = do_QueryInterface( aChannel );
  if (encChannel) 
  {
    // Turn off content encoding conversions if needed
    bool applyConversion = true;

    nsCOMPtr<nsIURL> sourceURL(do_QueryInterface(mSourceUrl));
    if (sourceURL)
    {
      nsCAutoString extension;
      sourceURL->GetFileExtension(extension);
      if (!extension.IsEmpty())
      {
        nsCOMPtr<nsIUTF8StringEnumerator> encEnum;
        encChannel->GetContentEncodings(getter_AddRefs(encEnum));
        if (encEnum)
        {
          bool hasMore;
          rv = encEnum->HasMore(&hasMore);
          if (NS_SUCCEEDED(rv) && hasMore)
          {
            nsCAutoString encType;
            rv = encEnum->GetNext(encType);
            if (NS_SUCCEEDED(rv) && !encType.IsEmpty())
            {
              mExtProtSvc->ApplyDecodingForExtension(extension, encType,
                                                     &applyConversion);
            }
          }
        }
      }    
    }

    encChannel->SetApplyConversion( applyConversion );
  }

  // At this point, the child process has done everything it can usefully do
  // for OnStartRequest.
  if (XRE_GetProcessType() == GeckoProcessType_Content)
     return NS_OK;

  rv = SetUpTempFile(aChannel);
  if (NS_FAILED(rv)) {
    mCanceled = true;
    request->Cancel(rv);
    nsAutoString path;
    if (mTempFile)
      mTempFile->GetPath(path);
    SendStatusChange(kWriteError, rv, request, path);
    return NS_OK;
  }

  // Inform channel it is open on behalf of a download to prevent caching.
  nsCOMPtr<nsIHttpChannelInternal> httpInternal = do_QueryInterface(aChannel);
  if (httpInternal) {
    httpInternal->SetChannelIsForDownload(true);
  }

  // now that the temp file is set up, find out if we need to invoke a dialog
  // asking the user what they want us to do with this content...

  // We can get here for three reasons: "can't handle", "sniffed type", or
  // "server sent content-disposition:attachment".  In the first case we want
  // to honor the user's "always ask" pref; in the other two cases we want to
  // honor it only if the default action is "save".  Opening attachments in
  // helper apps by default breaks some websites (especially if the attachment
  // is one part of a multipart document).  Opening sniffed content in helper
  // apps by default introduces security holes that we'd rather not have.

  // So let's find out whether the user wants to be prompted.  If he does not,
  // check mReason and the preferred action to see what we should do.

  bool alwaysAsk = true;
  mMimeInfo->GetAlwaysAskBeforeHandling(&alwaysAsk);
  if (alwaysAsk)
  {
    // But we *don't* ask if this mimeInfo didn't come from
    // our user configuration datastore and the user has said
    // at some point in the distant past that they don't
    // want to be asked.  The latter fact would have been
    // stored in pref strings back in the old days.

    bool mimeTypeIsInDatastore = false;
    nsCOMPtr<nsIHandlerService> handlerSvc = do_GetService(NS_HANDLERSERVICE_CONTRACTID);
    if (handlerSvc)
      handlerSvc->Exists(mMimeInfo, &mimeTypeIsInDatastore);
    if (!handlerSvc || !mimeTypeIsInDatastore)
    {
      nsCAutoString MIMEType;
      mMimeInfo->GetMIMEType(MIMEType);

      if (!GetNeverAskFlagFromPref(NEVER_ASK_FOR_SAVE_TO_DISK_PREF, MIMEType.get()))
      {
        // Don't need to ask after all.
        alwaysAsk = false;
        // Make sure action matches pref (save to disk).
        mMimeInfo->SetPreferredAction(nsIMIMEInfo::saveToDisk);
      }
      else if (!GetNeverAskFlagFromPref(NEVER_ASK_FOR_OPEN_FILE_PREF, MIMEType.get()))
      {
        // Don't need to ask after all.
        alwaysAsk = false;
      }
    }
  }

  PRInt32 action = nsIMIMEInfo::saveToDisk;
  mMimeInfo->GetPreferredAction( &action );

  // OK, now check why we're here
  if (!alwaysAsk && mReason != nsIHelperAppLauncherDialog::REASON_CANTHANDLE) {
    // Force asking if we're not saving.  See comment back when we fetched the
    // alwaysAsk boolean for details.
    alwaysAsk = (action != nsIMIMEInfo::saveToDisk);
  }

  // if we were told that we _must_ save to disk without asking, all the stuff
  // before this is irrelevant; override it
  if (mForceSave) {
    alwaysAsk = false;
    action = nsIMIMEInfo::saveToDisk;
  }
  
  if (alwaysAsk)
  {
    // do this first! make sure we don't try to take an action until the user tells us what they want to do
    // with it...
    mReceivedDispositionInfo = false; 
    mKeepRequestAlive = true;

    // invoke the dialog!!!!! use mWindowContext as the window context parameter for the dialog request
    mDialog = do_CreateInstance( NS_HELPERAPPLAUNCHERDLG_CONTRACTID, &rv );
    NS_ENSURE_SUCCESS(rv, rv);

    // this will create a reference cycle (the dialog holds a reference to us as
    // nsIHelperAppLauncher), which will be broken in Cancel or
    // CreateProgressListener.
    rv = mDialog->Show( this, mWindowContext, mReason );

    // what do we do if the dialog failed? I guess we should call Cancel and abort the load....
  }
  else
  {
    mReceivedDispositionInfo = true; // no need to wait for a response from the user

    // We need to do the save/open immediately, then.
#ifdef XP_WIN
    /* We need to see whether the file we've got here could be
     * executable.  If it could, we had better not try to open it!
     * We can skip this check, though, if we have a setting to open in a
     * helper app.
     * This code mirrors the code in
     * nsExternalAppHandler::LaunchWithApplication so that what we
     * test here is as close as possible to what will really be
     * happening if we decide to execute
     */
    nsCOMPtr<nsIHandlerApp> prefApp;
    mMimeInfo->GetPreferredApplicationHandler(getter_AddRefs(prefApp));
    if (action != nsIMIMEInfo::useHelperApp || !prefApp) {
      nsCOMPtr<nsIFile> fileToTest;
      GetTargetFile(getter_AddRefs(fileToTest));
      if (fileToTest) {
        bool isExecutable;
        rv = fileToTest->IsExecutable(&isExecutable);
        if (NS_FAILED(rv) || isExecutable) {  // checking NS_FAILED, because paranoia is good
          action = nsIMIMEInfo::saveToDisk;
        }
      } else {   // Paranoia is good here too, though this really should not happen
        NS_WARNING("GetDownloadInfo returned a null file after the temp file has been set up! ");
        action = nsIMIMEInfo::saveToDisk;
      }
    }

#endif
    if (action == nsIMIMEInfo::useHelperApp ||
        action == nsIMIMEInfo::useSystemDefault)
    {
        rv = LaunchWithApplication(nsnull, false);
    }
    else // Various unknown actions go here too
    {
        rv = SaveToDisk(nsnull, false);
    }
  }

  return NS_OK;
}

// Convert error info into proper message text and send OnStatusChange notification
// to the web progress listener.
void nsExternalAppHandler::SendStatusChange(ErrorType type, nsresult rv, nsIRequest *aRequest, const nsAFlatString &path)
{
    nsAutoString msgId;
    switch(rv)
    {
    case NS_ERROR_OUT_OF_MEMORY:
        // No memory
        msgId.AssignLiteral("noMemory");
        break;

    case NS_ERROR_FILE_DISK_FULL:
    case NS_ERROR_FILE_NO_DEVICE_SPACE:
        // Out of space on target volume.
        msgId.AssignLiteral("diskFull");
        break;

    case NS_ERROR_FILE_READ_ONLY:
        // Attempt to write to read/only file.
        msgId.AssignLiteral("readOnly");
        break;

    case NS_ERROR_FILE_ACCESS_DENIED:
        if (type == kWriteError) {
          // Attempt to write without sufficient permissions.
          msgId.AssignLiteral("accessError");
        }
        else
        {
          msgId.AssignLiteral("launchError");
        }
        break;

    case NS_ERROR_FILE_NOT_FOUND:
    case NS_ERROR_FILE_TARGET_DOES_NOT_EXIST:
    case NS_ERROR_FILE_UNRECOGNIZED_PATH:
        // Helper app not found, let's verify this happened on launch
        if (type == kLaunchError) {
          msgId.AssignLiteral("helperAppNotFound");
          break;
        }
        // fall through

    default:
        // Generic read/write/launch error message.
        switch(type)
        {
        case kReadError:
          msgId.AssignLiteral("readError");
          break;
        case kWriteError:
          msgId.AssignLiteral("writeError");
          break;
        case kLaunchError:
          msgId.AssignLiteral("launchError");
          break;
        }
        break;
    }
    PR_LOG(nsExternalHelperAppService::mLog, PR_LOG_ERROR,
        ("Error: %s, type=%i, listener=0x%p, rv=0x%08X\n",
         NS_LossyConvertUTF16toASCII(msgId).get(), type, mWebProgressListener.get(), rv));
    PR_LOG(nsExternalHelperAppService::mLog, PR_LOG_ERROR,
        ("       path='%s'\n", NS_ConvertUTF16toUTF8(path).get()));

    // Get properties file bundle and extract status string.
    nsCOMPtr<nsIStringBundleService> stringService =
        mozilla::services::GetStringBundleService();
    if (stringService)
    {
        nsCOMPtr<nsIStringBundle> bundle;
        if (NS_SUCCEEDED(stringService->CreateBundle("chrome://global/locale/nsWebBrowserPersist.properties", getter_AddRefs(bundle))))
        {
            nsXPIDLString msgText;
            const PRUnichar *strings[] = { path.get() };
            if(NS_SUCCEEDED(bundle->FormatStringFromName(msgId.get(), strings, 1, getter_Copies(msgText))))
            {
              if (mWebProgressListener)
              {
                // We have a listener, let it handle the error.
                mWebProgressListener->OnStatusChange(nsnull, (type == kReadError) ? aRequest : nsnull, rv, msgText);
              }
              else
              if (XRE_GetProcessType() == GeckoProcessType_Default) {
                // We don't have a listener.  Simply show the alert ourselves.
                nsCOMPtr<nsIPrompt> prompter(do_GetInterface(mWindowContext));
                nsXPIDLString title;
                bundle->FormatStringFromName(NS_LITERAL_STRING("title").get(),
                                             strings,
                                             1,
                                             getter_Copies(title));
                if (prompter)
                {
                  prompter->Alert(title, msgText);
                }
              }
            }
        }
    }
}

NS_IMETHODIMP nsExternalAppHandler::OnDataAvailable(nsIRequest *request, nsISupports * aCtxt,
                                                  nsIInputStream * inStr, PRUint32 sourceOffset, PRUint32 count)
{
  nsresult rv = NS_OK;
  // first, check to see if we've been canceled....
  if (mCanceled || !mDataBuffer) // then go cancel our underlying channel too
    return request->Cancel(NS_BINDING_ABORTED);

  // read the data out of the stream and write it to the temp file.
  if (mOutStream && count > 0)
  {
    PRUint32 numBytesRead = 0; 
    PRUint32 numBytesWritten = 0;
    mProgress += count;
    bool readError = true;
    while (NS_SUCCEEDED(rv) && count > 0) // while we still have bytes to copy...
    {
      readError = true;
      rv = inStr->Read(mDataBuffer, NS_MIN(count, mBufferSize - 1), &numBytesRead);
      if (NS_SUCCEEDED(rv))
      {
        if (count >= numBytesRead)
          count -= numBytesRead; // subtract off the number of bytes we just read
        else
          count = 0;
        readError = false;
        // Write out the data until something goes wrong, or, it is
        // all written.  We loop because for some errors (e.g., disk
        // full), we get NS_OK with some bytes written, then an error.
        // So, we want to write again in that case to get the actual
        // error code.
        const char *bufPtr = mDataBuffer; // Where to write from.
        while (NS_SUCCEEDED(rv) && numBytesRead)
        {
          numBytesWritten = 0;
          rv = mOutStream->Write(bufPtr, numBytesRead, &numBytesWritten);
          if (NS_SUCCEEDED(rv))
          {
            numBytesRead -= numBytesWritten;
            bufPtr += numBytesWritten;
            // Force an error if (for some reason) we get NS_OK but
            // no bytes written.
            if (!numBytesWritten)
            {
              rv = NS_ERROR_FAILURE;
            }
          }
        }
      }
    }
    if (NS_SUCCEEDED(rv))
    {
      // Send progress notification.
      if (mWebProgressListener)
      {
        mWebProgressListener->OnProgressChange64(nsnull, request, mProgress, mContentLength, mProgress, mContentLength);
      }
    }
    else
    {
      // An error occurred, notify listener.
      nsAutoString tempFilePath;
      if (mTempFile)
        mTempFile->GetPath(tempFilePath);
      SendStatusChange(readError ? kReadError : kWriteError, rv, request, tempFilePath);

      // Cancel the download.
      Cancel(rv);
    }
  }
  return rv;
}

NS_IMETHODIMP nsExternalAppHandler::OnStopRequest(nsIRequest *request, nsISupports *aCtxt, 
                                                  nsresult aStatus)
{
  mStopRequestIssued = true;

  if (!mKeepRequestAlive)
    mRequest = nsnull;

  // Cancel if the request did not complete successfully.
  if (!mCanceled && NS_FAILED(aStatus))
  {
    // Send error notification.
    nsAutoString tempFilePath;
    if (mTempFile)
      mTempFile->GetPath(tempFilePath);
    SendStatusChange( kReadError, aStatus, request, tempFilePath );

    Cancel(aStatus);
  }

  // first, check to see if we've been canceled....
  if (mCanceled)
    return NS_OK;

  // close the stream...
  if (mOutStream)
  {
    mOutStream->Close();
    mOutStream = nsnull;
  }

  // Do what the user asked for
  ExecuteDesiredAction();

  // At this point, the channel should still own us. So releasing the reference
  // to us in the nsITransfer should be ok.
  // This nsITransfer object holds a reference to us (we are its observer), so
  // we need to release the reference to break a reference cycle (and therefore
  // to prevent leaking)
  mWebProgressListener = nsnull;

  return NS_OK;
}

nsresult nsExternalAppHandler::ExecuteDesiredAction()
{
  nsresult rv = NS_OK;
  if (mProgressListenerInitialized && !mCanceled)
  {
    rv = MoveFile(mFinalFileDestination);
    if (NS_SUCCEEDED(rv))
    {
      nsHandlerInfoAction action = nsIMIMEInfo::saveToDisk;
      mMimeInfo->GetPreferredAction(&action);
      if (action == nsIMIMEInfo::useHelperApp ||
          action == nsIMIMEInfo::useSystemDefault)
      {
        rv = OpenWithApplication();
      }
      else if(action == nsIMIMEInfo::saveToDisk)
      {
        mExtProtSvc->FixFilePermissions(mFinalFileDestination);
      }
    }

    // Notify dialog that download is complete.
    // By waiting till this point, it ensures that the progress dialog doesn't indicate
    // success until we're really done.
    if(mWebProgressListener)
    {
      if (!mCanceled)
      {
        mWebProgressListener->OnProgressChange64(nsnull, nsnull, mProgress, mContentLength, mProgress, mContentLength);
      }
      mWebProgressListener->OnStateChange(nsnull, nsnull,
        nsIWebProgressListener::STATE_STOP |
        nsIWebProgressListener::STATE_IS_REQUEST |
        nsIWebProgressListener::STATE_IS_NETWORK, NS_OK);
    }
  }

  return rv;
}

NS_IMETHODIMP nsExternalAppHandler::GetMIMEInfo(nsIMIMEInfo ** aMIMEInfo)
{
  *aMIMEInfo = mMimeInfo;
  NS_ADDREF(*aMIMEInfo);
  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::GetSource(nsIURI ** aSourceURI)
{
  NS_ENSURE_ARG(aSourceURI);
  *aSourceURI = mSourceUrl;
  NS_IF_ADDREF(*aSourceURI);
  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::GetSuggestedFileName(nsAString& aSuggestedFileName)
{
  aSuggestedFileName = mSuggestedFileName;
  return NS_OK;
}

nsresult nsExternalAppHandler::InitializeDownload(nsITransfer* aTransfer)
{
  nsresult rv;
  
  nsCOMPtr<nsIURI> target;
  rv = NS_NewFileURI(getter_AddRefs(target), mFinalFileDestination);
  if (NS_FAILED(rv)) return rv;
  
  rv = aTransfer->Init(mSourceUrl, target, EmptyString(),
                       mMimeInfo, mTimeDownloadStarted, mTempFile, this);
  if (NS_FAILED(rv)) return rv;

  // Now let's add the download to history
  nsCOMPtr<nsIDownloadHistory> dh(do_GetService(NS_DOWNLOADHISTORY_CONTRACTID));
  if (dh) {
    nsCOMPtr<nsIURI> referrer;
    if (mRequest) {
      nsCOMPtr<nsIChannel> channel = do_QueryInterface(mRequest);
      NS_GetReferrerFromChannel(channel, getter_AddRefs(referrer));
    }

    dh->AddDownload(mSourceUrl, referrer, mTimeDownloadStarted, target);
  }

  return rv;
}

nsresult nsExternalAppHandler::CreateProgressListener()
{
  // we are back from the helper app dialog (where the user chooses to save or open), but we aren't
  // done processing the load. in this case, throw up a progress dialog so the user can see what's going on...
  // Also, release our reference to mDialog. We don't need it anymore, and we
  // need to break the reference cycle.
  mDialog = nsnull;
  nsresult rv;
  
  nsCOMPtr<nsITransfer> tr = do_CreateInstance(NS_TRANSFER_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
    InitializeDownload(tr);

  if (tr)
    tr->OnStateChange(nsnull, mRequest, nsIWebProgressListener::STATE_START |
      nsIWebProgressListener::STATE_IS_REQUEST |
      nsIWebProgressListener::STATE_IS_NETWORK, NS_OK);

  // note we might not have a listener here if the QI() failed, or if
  // there is no nsITransfer object, but we still call
  // SetWebProgressListener() to make sure our progress state is sane
  // NOTE: This will set up a reference cycle (this nsITransfer has us set up as
  // its observer). This cycle will be broken in Cancel, CloseProgressWindow or
  // OnStopRequest.
  SetWebProgressListener(tr);

  mRequest = nsnull;

  return rv;
}

nsresult nsExternalAppHandler::PromptForSaveToFile(nsIFile ** aNewFile, const nsAFlatString &aDefaultFile, const nsAFlatString &aFileExtension)
{
  // invoke the dialog!!!!! use mWindowContext as the window context parameter for the dialog request
  // Convert to use file picker? No, then embeddors could not do any sort of
  // "AutoDownload" w/o showing a prompt
  nsresult rv = NS_OK;
  if (!mDialog)
  {
    // Get helper app launcher dialog.
    mDialog = do_CreateInstance( NS_HELPERAPPLAUNCHERDLG_CONTRACTID, &rv );
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // we want to explicitly unescape aDefaultFile b4 passing into the dialog. we can't unescape
  // it because the dialog is implemented by a JS component which doesn't have a window so no unescape routine is defined...

  // Now, be sure to keep |this| alive, and the dialog
  // If we don't do this, users that close the helper app dialog while the file
  // picker is up would cause Cancel() to be called, and the dialog would be
  // released, which would release this object too, which would crash.
  // See Bug 249143
  nsRefPtr<nsExternalAppHandler> kungFuDeathGrip(this);
  nsCOMPtr<nsIHelperAppLauncherDialog> dlg(mDialog);
  rv = mDialog->PromptForSaveToFile(this, 
                                    mWindowContext,
                                    aDefaultFile.get(),
                                    aFileExtension.get(),
                                    mForceSave, aNewFile);
  return rv;
}

nsresult nsExternalAppHandler::MoveFile(nsIFile * aNewFileLocation)
{
  nsresult rv = NS_OK;
  NS_ASSERTION(mStopRequestIssued, "uhoh, how did we get here if we aren't done getting data?");
 
  // if the on stop request was actually issued then it's now time to actually perform the file move....
  if (mStopRequestIssued && aNewFileLocation)
  {
    // Unfortunately, MoveTo will fail if a file already exists at the user specified location....
    // but the user has told us, this is where they want the file! (when we threw up the save to file dialog,
    // it told them the file already exists and do they wish to over write it. So it should be okay to delete
    // fileToUse if it already exists.
    bool equalToTempFile = false;
    bool filetoUseAlreadyExists = false;
    aNewFileLocation->Equals(mTempFile, &equalToTempFile);
    aNewFileLocation->Exists(&filetoUseAlreadyExists);
    if (filetoUseAlreadyExists && !equalToTempFile)
      aNewFileLocation->Remove(false);

     // extract the new leaf name from the file location
     nsAutoString fileName;
     aNewFileLocation->GetLeafName(fileName);
     nsCOMPtr<nsIFile> directoryLocation;
     rv = aNewFileLocation->GetParent(getter_AddRefs(directoryLocation));
     if (directoryLocation)
     {
       rv = mTempFile->MoveTo(directoryLocation, fileName);
     }
     if (NS_FAILED(rv))
     {
       // Send error notification.        
       nsAutoString path;
       aNewFileLocation->GetPath(path);
       SendStatusChange(kWriteError, rv, nsnull, path);
       Cancel(rv); // Cancel (and clean up temp file).
     }
#if defined(XP_OS2)
     else
     {
       // tag the file with its source URI
       nsCOMPtr<nsILocalFileOS2> localFileOS2 = do_QueryInterface(aNewFileLocation);
       if (localFileOS2)
       {
         nsCAutoString url;
         mSourceUrl->GetSpec(url);
         localFileOS2->SetFileSource(url);
       }
     }
#endif
  }

  return rv;
}

// SaveToDisk should only be called by the helper app dialog which allows
// the user to say launch with application or save to disk. It doesn't actually 
// perform the save, it just prompts for the destination file name. The actual save
// won't happen until we are done downloading the content and are sure we've 
// shown a progress dialog. This was done to simplify the 
// logic that was showing up in this method. Internal callers who actually want
// to preform the save should call ::MoveFile

NS_IMETHODIMP nsExternalAppHandler::SaveToDisk(nsIFile * aNewFileLocation, bool aRememberThisPreference)
{
  nsresult rv = NS_OK;
  if (mCanceled)
    return NS_OK;

  mMimeInfo->SetPreferredAction(nsIMIMEInfo::saveToDisk);

  // The helper app dialog has told us what to do.
  mReceivedDispositionInfo = true;

  nsCOMPtr<nsIFile> fileToUse = do_QueryInterface(aNewFileLocation);
  if (!fileToUse)
  {
    nsAutoString leafName;
    mTempFile->GetLeafName(leafName);
    if (mSuggestedFileName.IsEmpty())
      rv = PromptForSaveToFile(getter_AddRefs(fileToUse), leafName, mTempFileExtension);
    else
    {
      nsAutoString fileExt;
      PRInt32 pos = mSuggestedFileName.RFindChar('.');
      if (pos >= 0)
        mSuggestedFileName.Right(fileExt, mSuggestedFileName.Length() - pos);
      if (fileExt.IsEmpty())
        fileExt = mTempFileExtension;

      rv = PromptForSaveToFile(getter_AddRefs(fileToUse), mSuggestedFileName, fileExt);
    }

    if (NS_FAILED(rv) || !fileToUse) {
      Cancel(NS_BINDING_ABORTED);
      return NS_ERROR_FAILURE;
    }
  }
  
  mFinalFileDestination = do_QueryInterface(fileToUse);

  // Move what we have in the final directory, but append .part
  // to it, to indicate that it's unfinished.
  // do not do that if we're already done
  if (mFinalFileDestination && !mStopRequestIssued)
  {
    nsCOMPtr<nsIFile> movedFile;
    mFinalFileDestination->Clone(getter_AddRefs(movedFile));
    if (movedFile) {
      // Get the old leaf name and append .part to it
      nsAutoString name;
      mFinalFileDestination->GetLeafName(name);
      name.AppendLiteral(".part");
      movedFile->SetLeafName(name);

      nsCOMPtr<nsIFile> dir;
      movedFile->GetParent(getter_AddRefs(dir));

      mOutStream->Close();

      rv = mTempFile->MoveTo(dir, name);
      if (NS_SUCCEEDED(rv)) // if it failed, we just continue with $TEMP
        mTempFile = movedFile;

      nsCOMPtr<nsIOutputStream> outputStream;
      rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), mTempFile,
                                       PR_WRONLY | PR_APPEND, 0600);
      if (NS_FAILED(rv)) { // (Re-)opening the output stream failed. bad luck.
        nsAutoString path;
        mTempFile->GetPath(path);
        SendStatusChange(kWriteError, rv, nsnull, path);
        Cancel(rv);
        return NS_OK;
      }

      mOutStream = NS_BufferOutputStream(outputStream, BUFFERED_OUTPUT_SIZE);
    }
  }

  if (!mProgressListenerInitialized)
    CreateProgressListener();

  // now that the user has chosen the file location to save to, it's okay to fire the refresh tag
  // if there is one. We don't want to do this before the save as dialog goes away because this dialog
  // is modal and we do bad things if you try to load a web page in the underlying window while a modal
  // dialog is still up. 
  ProcessAnyRefreshTags();

  return NS_OK;
}


nsresult nsExternalAppHandler::OpenWithApplication()
{
  nsresult rv = NS_OK;
  if (mCanceled)
    return NS_OK;
  
  // we only should have gotten here if the on stop request had been fired already.

  NS_ASSERTION(mStopRequestIssued, "uhoh, how did we get here if we aren't done getting data?");
  // if a stop request was already issued then proceed with launching the application.
  if (mStopRequestIssued)
  {

    // Note for the default value:
    // Mac users have been very verbal about temp files being deleted on
    // app exit - they don't like it - but we'll continue to do this on
    // other platforms for now.
    bool deleteTempFileOnExit =
      Preferences::GetBool("browser.helperApps.deleteTempFileOnExit",
#if !defined(XP_MACOSX)
                           true);
#else
                           false);
#endif

    // See whether the channel has been opened in private browsing mode
    bool inPrivateBrowsing = false;
    NS_ASSERTION(mRequest, "This should never be called with a null request");
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(mRequest);
    if (channel) {
      nsCOMPtr<nsILoadContext> ctx;
      NS_QueryNotificationCallbacks(channel, ctx);
      if (ctx) {
        inPrivateBrowsing = ctx->UsePrivateBrowsing();
      }
    }

    // make the tmp file readonly so users won't edit it and lose the changes
    // only if we're going to delete the file
    if (deleteTempFileOnExit || inPrivateBrowsing)
      mFinalFileDestination->SetPermissions(0400);

    rv = mMimeInfo->LaunchWithFile(mFinalFileDestination);
    if (NS_FAILED(rv))
    {
      // Send error notification.
      nsAutoString path;
      mFinalFileDestination->GetPath(path);
      SendStatusChange(kLaunchError, rv, nsnull, path);
      Cancel(rv); // Cancel, and clean up temp file.
    }
    // Always schedule files to be deleted at the end of the private browsing
    // mode, regardless of the value of the pref.
    else if (deleteTempFileOnExit) {
      mExtProtSvc->DeleteTemporaryFileOnExit(mFinalFileDestination);
    }
    else if (inPrivateBrowsing) {
      mExtProtSvc->DeleteTemporaryPrivateFileWhenPossible(mFinalFileDestination);
    }
  }

  return rv;
}

// LaunchWithApplication should only be called by the helper app dialog which allows
// the user to say launch with application or save to disk. It doesn't actually 
// perform launch with application. That won't happen until we are done downloading
// the content and are sure we've shown a progress dialog. This was done to simplify the 
// logic that was showing up in this method. 
NS_IMETHODIMP nsExternalAppHandler::LaunchWithApplication(nsIFile * aApplication, bool aRememberThisPreference)
{
  if (mCanceled)
    return NS_OK;

  // user has chosen to launch using an application, fire any refresh tags now...
  ProcessAnyRefreshTags(); 
  
  mReceivedDispositionInfo = true; 
  if (mMimeInfo && aApplication) {
    PlatformLocalHandlerApp_t *handlerApp =
      new PlatformLocalHandlerApp_t(EmptyString(), aApplication);
    mMimeInfo->SetPreferredApplicationHandler(handlerApp);
  }

  // Now check if the file is local, in which case we won't bother with saving
  // it to a temporary directory and just launch it from where it is
  nsCOMPtr<nsIFileURL> fileUrl(do_QueryInterface(mSourceUrl));
  if (fileUrl && mIsFileChannel)
  {
    Cancel(NS_BINDING_ABORTED);
    nsCOMPtr<nsIFile> file;
    nsresult rv = fileUrl->GetFile(getter_AddRefs(file));

    if (NS_SUCCEEDED(rv))
    {
      rv = mMimeInfo->LaunchWithFile(file);
      if (NS_SUCCEEDED(rv))
        return NS_OK;
    }
    nsAutoString path;
    if (file)
      file->GetPath(path);
    // If we get here, an error happened
    SendStatusChange(kLaunchError, rv, nsnull, path);
    return rv;
  }

  // Now that the user has elected to launch the downloaded file with a helper app, we're justified in
  // removing the 'salted' name.  We'll rename to what was specified in mSuggestedFileName after the
  // download is done prior to launching the helper app.  So that any existing file of that name won't
  // be overwritten we call CreateUnique() before calling MoveFile().  Also note that we use the same
  // directory as originally downloaded to so that MoveFile() just does an in place rename.

  nsCOMPtr<nsIFile> fileToUse;
  (void) GetDownloadDirectory(getter_AddRefs(fileToUse));

  if (mSuggestedFileName.IsEmpty())
  {
    // Keep using the leafname of the temp file, since we're just starting a helper
    mTempFile->GetLeafName(mSuggestedFileName);
  }

#ifdef XP_WIN
  fileToUse->Append(mSuggestedFileName + mTempFileExtension);
#else
  fileToUse->Append(mSuggestedFileName);  
#endif

  nsresult rv = fileToUse->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
  if(NS_SUCCEEDED(rv))
  {
    mFinalFileDestination = do_QueryInterface(fileToUse);
    // launch the progress window now that the user has picked the desired action.
    if (!mProgressListenerInitialized)
      CreateProgressListener();
  }
  else
  {
    // Cancel the download and report an error.  We do not want to end up in
    // a state where it appears that we have a normal download that is
    // pointing to a file that we did not actually create.
    nsAutoString path;
    mTempFile->GetPath(path);
    SendStatusChange(kWriteError, rv, nsnull, path);
    Cancel(rv);
  }
  return rv;
}

NS_IMETHODIMP nsExternalAppHandler::Cancel(nsresult aReason)
{
  NS_ENSURE_ARG(NS_FAILED(aReason));
  // XXX should not ignore the reason

  mCanceled = true;
  // Break our reference cycle with the helper app dialog (set up in
  // OnStartRequest)
  mDialog = nsnull;

  mRequest = nsnull;

  // shutdown our stream to the temp file
  if (mOutStream)
  {
    mOutStream->Close();
    mOutStream = nsnull;
  }

  // Clean up after ourselves and delete the temp file only if the user
  // canceled the helper app dialog (we didn't get the disposition info yet).
  // We leave the partial file for everything else because it could be useful
  // e.g., resume a download
  if (mTempFile && !mReceivedDispositionInfo)
  {
    mTempFile->Remove(false);
    mTempFile = nsnull;
  }

  // If we have already created a final destination file, we remove it as well
  if (mFinalFileDestination)
  {
    mFinalFileDestination->Remove(false);
    mFinalFileDestination = nsnull;
  }

  // Release the listener, to break the reference cycle with it (we are the
  // observer of the listener).
  mWebProgressListener = nsnull;

  return NS_OK;
}

void nsExternalAppHandler::ProcessAnyRefreshTags()
{
   // one last thing, try to see if the original window context supports a refresh interface...
   // Sometimes, when you download content that requires an external handler, there is
   // a refresh header associated with the download. This refresh header points to a page
   // the content provider wants the user to see after they download the content. How do we
   // pass this refresh information back to the caller? For now, try to get the refresh URI 
   // interface. If the window context where the request originated came from supports this
   // then we can force it to process the refresh information (if there is any) from this channel.
   if (mWindowContext && mOriginalChannel)
   {
     nsCOMPtr<nsIRefreshURI> refreshHandler (do_GetInterface(mWindowContext));
     if (refreshHandler) {
        refreshHandler->SetupRefreshURI(mOriginalChannel);
     }
     mOriginalChannel = nsnull;
   }
}

bool nsExternalAppHandler::GetNeverAskFlagFromPref(const char * prefName, const char * aContentType)
{
  // Search the obsolete pref strings.
  nsAdoptingCString prefCString = Preferences::GetCString(prefName);
  if (prefCString.IsEmpty()) {
    // Default is true, if not found in the pref string.
    return true;
  }

  NS_UnescapeURL(prefCString);
  nsACString::const_iterator start, end;
  prefCString.BeginReading(start);
  prefCString.EndReading(end);
  return !CaseInsensitiveFindInReadable(nsDependentCString(aContentType),
                                        start, end);
}

nsresult nsExternalAppHandler::MaybeCloseWindow()
{
  nsCOMPtr<nsIDOMWindow> window = do_GetInterface(mWindowContext);
  NS_ENSURE_STATE(window);

  if (mShouldCloseWindow) {
    // Reset the window context to the opener window so that the dependent
    // dialogs have a parent
    nsCOMPtr<nsIDOMWindow> opener;
    window->GetOpener(getter_AddRefs(opener));

    bool isClosed;
    if (opener && NS_SUCCEEDED(opener->GetClosed(&isClosed)) && !isClosed) {
      mWindowContext = do_GetInterface(opener);

      // Now close the old window.  Do it on a timer so that we don't run
      // into issues trying to close the window before it has fully opened.
      NS_ASSERTION(!mTimer, "mTimer was already initialized once!");
      mTimer = do_CreateInstance("@mozilla.org/timer;1");
      if (!mTimer) {
        return NS_ERROR_FAILURE;
      }

      mTimer->InitWithCallback(this, 0, nsITimer::TYPE_ONE_SHOT);
      mWindowToClose = window;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsExternalAppHandler::Notify(nsITimer* timer)
{
  NS_ASSERTION(mWindowToClose, "No window to close after timer fired");

  mWindowToClose->Close();
  mWindowToClose = nsnull;
  mTimer = nsnull;

  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The following section contains our nsIMIMEService implementation and related methods.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

// nsIMIMEService methods
NS_IMETHODIMP nsExternalHelperAppService::GetFromTypeAndExtension(const nsACString& aMIMEType, const nsACString& aFileExt, nsIMIMEInfo **_retval) 
{
  NS_PRECONDITION(!aMIMEType.IsEmpty() ||
                  !aFileExt.IsEmpty(), 
                  "Give me something to work with");
  LOG(("Getting mimeinfo from type '%s' ext '%s'\n",
        PromiseFlatCString(aMIMEType).get(), PromiseFlatCString(aFileExt).get()));

  *_retval = nsnull;

  // OK... we need a type. Get one.
  nsCAutoString typeToUse(aMIMEType);
  if (typeToUse.IsEmpty()) {
    nsresult rv = GetTypeFromExtension(aFileExt, typeToUse);
    if (NS_FAILED(rv))
      return NS_ERROR_NOT_AVAILABLE;
  }

  // We promise to only send lower case mime types to the OS
  ToLowerCase(typeToUse);

  // (1) Ask the OS for a mime info
  bool found;
  *_retval = GetMIMEInfoFromOS(typeToUse, aFileExt, &found).get();
  LOG(("OS gave back 0x%p - found: %i\n", *_retval, found));
  // If we got no mimeinfo, something went wrong. Probably lack of memory.
  if (!*_retval)
    return NS_ERROR_OUT_OF_MEMORY;

  // (2) Now, let's see if we can find something in our datastore
  // This will not overwrite the OS information that interests us
  // (i.e. default application, default app. description)
  nsresult rv;
  nsCOMPtr<nsIHandlerService> handlerSvc = do_GetService(NS_HANDLERSERVICE_CONTRACTID);
  if (handlerSvc) {
    bool hasHandler = false;
    (void) handlerSvc->Exists(*_retval, &hasHandler);
    if (hasHandler) {
      rv = handlerSvc->FillHandlerInfo(*_retval, EmptyCString());
      LOG(("Data source: Via type: retval 0x%08x\n", rv));
    } else {
      rv = NS_ERROR_NOT_AVAILABLE;
    }
 
    found = found || NS_SUCCEEDED(rv);

    if (!found || NS_FAILED(rv)) {
      // No type match, try extension match
      if (!aFileExt.IsEmpty()) {
        nsCAutoString overrideType;
        rv = handlerSvc->GetTypeFromExtension(aFileExt, overrideType);
        if (NS_SUCCEEDED(rv) && !overrideType.IsEmpty()) {
          // We can't check handlerSvc->Exists() here, because we have a
          // overideType. That's ok, it just results in some console noise.
          // (If there's no handler for the override type, it throws)
          rv = handlerSvc->FillHandlerInfo(*_retval, overrideType);
          LOG(("Data source: Via ext: retval 0x%08x\n", rv));
          found = found || NS_SUCCEEDED(rv);
        }
      }
    }
  }

  // (3) No match yet. Ask extras.
  if (!found) {
    rv = NS_ERROR_FAILURE;
#ifdef XP_WIN
    /* XXX Gross hack to wallpaper over the most common Win32
     * extension issues caused by the fix for bug 116938.  See bug
     * 120327, comment 271 for why this is needed.  Not even sure we
     * want to remove this once we have fixed all this stuff to work
     * right; any info we get from extras on this type is pretty much
     * useless....
     */
    if (!typeToUse.Equals(APPLICATION_OCTET_STREAM, nsCaseInsensitiveCStringComparator()))
#endif
      rv = FillMIMEInfoForMimeTypeFromExtras(typeToUse, *_retval);
    LOG(("Searched extras (by type), rv 0x%08X\n", rv));
    // If that didn't work out, try file extension from extras
    if (NS_FAILED(rv) && !aFileExt.IsEmpty()) {
      rv = FillMIMEInfoForExtensionFromExtras(aFileExt, *_retval);
      LOG(("Searched extras (by ext), rv 0x%08X\n", rv));
    }
    // If that still didn't work, set the file description to "ext File"
    if (NS_FAILED(rv) && !aFileExt.IsEmpty()) {
      // XXXzpao This should probably be localized
      nsCAutoString desc(aFileExt);
      desc.Append(" File");
      (*_retval)->SetDescription(NS_ConvertASCIItoUTF16(desc));
      LOG(("Falling back to 'File' file description\n"));
    }
  }

  // Finally, check if we got a file extension and if yes, if it is an
  // extension on the mimeinfo, in which case we want it to be the primary one
  if (!aFileExt.IsEmpty()) {
    bool matches = false;
    (*_retval)->ExtensionExists(aFileExt, &matches);
    LOG(("Extension '%s' matches mime info: %i\n", PromiseFlatCString(aFileExt).get(), matches));
    if (matches)
      (*_retval)->SetPrimaryExtension(aFileExt);
  }

#ifdef PR_LOGGING
  if (LOG_ENABLED()) {
    nsCAutoString type;
    (*_retval)->GetMIMEType(type);

    nsCAutoString ext;
    (*_retval)->GetPrimaryExtension(ext);
    LOG(("MIME Info Summary: Type '%s', Primary Ext '%s'\n", type.get(), ext.get()));
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP nsExternalHelperAppService::GetTypeFromExtension(const nsACString& aFileExt, nsACString& aContentType) 
{
  // OK. We want to try the following sources of mimetype information, in this order:
  // 1. defaultMimeEntries array
  // 2. User-set preferences (managed by the handler service)
  // 3. OS-provided information
  // 4. our "extras" array
  // 5. Information from plugins
  // 6. The "ext-to-type-mapping" category

  nsresult rv = NS_OK;
  // First of all, check our default entries
  for (size_t i = 0; i < ArrayLength(defaultMimeEntries); i++)
  {
    if (aFileExt.LowerCaseEqualsASCII(defaultMimeEntries[i].mFileExtension)) {
      aContentType = defaultMimeEntries[i].mMimeType;
      return rv;
    }
  }

  // Check user-set prefs
  nsCOMPtr<nsIHandlerService> handlerSvc = do_GetService(NS_HANDLERSERVICE_CONTRACTID);
  if (handlerSvc)
    rv = handlerSvc->GetTypeFromExtension(aFileExt, aContentType);
  if (NS_SUCCEEDED(rv) && !aContentType.IsEmpty())
    return NS_OK;

  // Ask OS.
  bool found = false;
  nsCOMPtr<nsIMIMEInfo> mi = GetMIMEInfoFromOS(EmptyCString(), aFileExt, &found);
  if (mi && found)
    return mi->GetMIMEType(aContentType);

  // Check extras array.
  found = GetTypeFromExtras(aFileExt, aContentType);
  if (found)
    return NS_OK;

  const nsCString& flatExt = PromiseFlatCString(aFileExt);
  // Try the plugins
  const char* mimeType;
  nsCOMPtr<nsIPluginHost> pluginHostCOM(do_GetService(MOZ_PLUGIN_HOST_CONTRACTID, &rv));
  nsPluginHost* pluginHost = static_cast<nsPluginHost*>(pluginHostCOM.get());
  if (NS_SUCCEEDED(rv)) {
    if (NS_SUCCEEDED(pluginHost->IsPluginEnabledForExtension(flatExt.get(), mimeType))) {
      aContentType = mimeType;
      return NS_OK;
    }
  }
  
  rv = NS_OK;
  // Let's see if an extension added something
  nsCOMPtr<nsICategoryManager> catMan(do_GetService("@mozilla.org/categorymanager;1"));
  if (catMan) {
    // The extension in the category entry is always stored as lowercase
    nsCAutoString lowercaseFileExt(aFileExt);
    ToLowerCase(lowercaseFileExt);
    // Read the MIME type from the category entry, if available
    nsXPIDLCString type;
    rv = catMan->GetCategoryEntry("ext-to-type-mapping", lowercaseFileExt.get(),
                                  getter_Copies(type));
    aContentType = type;
  }
  else {
    rv = NS_ERROR_NOT_AVAILABLE;
  }
  
  return rv;
}

NS_IMETHODIMP nsExternalHelperAppService::GetPrimaryExtension(const nsACString& aMIMEType, const nsACString& aFileExt, nsACString& _retval)
{
  NS_ENSURE_ARG(!aMIMEType.IsEmpty());

  nsCOMPtr<nsIMIMEInfo> mi;
  nsresult rv = GetFromTypeAndExtension(aMIMEType, aFileExt, getter_AddRefs(mi));
  if (NS_FAILED(rv))
    return rv;

  return mi->GetPrimaryExtension(_retval);
}

NS_IMETHODIMP nsExternalHelperAppService::GetTypeFromURI(nsIURI *aURI, nsACString& aContentType) 
{
  NS_ENSURE_ARG_POINTER(aURI);
  nsresult rv = NS_ERROR_NOT_AVAILABLE;
  aContentType.Truncate();

  // First look for a file to use.  If we have one, we just use that.
  nsCOMPtr<nsIFileURL> fileUrl = do_QueryInterface(aURI);
  if (fileUrl) {
    nsCOMPtr<nsIFile> file;
    rv = fileUrl->GetFile(getter_AddRefs(file));
    if (NS_SUCCEEDED(rv)) {
      rv = GetTypeFromFile(file, aContentType);
      if (NS_SUCCEEDED(rv)) {
        // we got something!
        return rv;
      }
    }
  }

  // Now try to get an nsIURL so we don't have to do our own parsing
  nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);
  if (url) {
    nsCAutoString ext;
    rv = url->GetFileExtension(ext);
    if (NS_FAILED(rv))
      return rv;
    if (ext.IsEmpty())
      return NS_ERROR_NOT_AVAILABLE;

    UnescapeFragment(ext, url, ext);

    return GetTypeFromExtension(ext, aContentType);
  }
    
  // no url, let's give the raw spec a shot
  nsCAutoString specStr;
  rv = aURI->GetSpec(specStr);
  if (NS_FAILED(rv))
    return rv;
  UnescapeFragment(specStr, aURI, specStr);

  // find the file extension (if any)
  PRInt32 extLoc = specStr.RFindChar('.');
  PRInt32 specLength = specStr.Length();
  if (-1 != extLoc &&
      extLoc != specLength - 1 &&
      // nothing over 20 chars long can be sanely considered an
      // extension.... Dat dere would be just data.
      specLength - extLoc < 20) 
  {
    return GetTypeFromExtension(Substring(specStr, extLoc + 1), aContentType);
  }

  // We found no information; say so.
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP nsExternalHelperAppService::GetTypeFromFile(nsIFile* aFile, nsACString& aContentType)
{
  NS_ENSURE_ARG_POINTER(aFile);
  nsresult rv;
  nsCOMPtr<nsIMIMEInfo> info;

  // Get the Extension
  nsAutoString fileName;
  rv = aFile->GetLeafName(fileName);
  if (NS_FAILED(rv)) return rv;
 
  nsCAutoString fileExt;
  if (!fileName.IsEmpty())
  {
    PRInt32 len = fileName.Length(); 
    for (PRInt32 i = len; i >= 0; i--) 
    {
      if (fileName[i] == PRUnichar('.'))
      {
        CopyUTF16toUTF8(fileName.get() + i + 1, fileExt);
        break;
      }
    }
  }

  if (fileExt.IsEmpty())
    return NS_ERROR_FAILURE;

  return GetTypeFromExtension(fileExt, aContentType);
}

nsresult nsExternalHelperAppService::FillMIMEInfoForMimeTypeFromExtras(
  const nsACString& aContentType, nsIMIMEInfo * aMIMEInfo)
{
  NS_ENSURE_ARG( aMIMEInfo );

  NS_ENSURE_ARG( !aContentType.IsEmpty() );

  // Look for default entry with matching mime type.
  nsCAutoString MIMEType(aContentType);
  ToLowerCase(MIMEType);
  PRInt32 numEntries = ArrayLength(extraMimeEntries);
  for (PRInt32 index = 0; index < numEntries; index++)
  {
      if ( MIMEType.Equals(extraMimeEntries[index].mMimeType) )
      {
          // This is the one. Set attributes appropriately.
          aMIMEInfo->SetFileExtensions(nsDependentCString(extraMimeEntries[index].mFileExtensions));
          aMIMEInfo->SetDescription(NS_ConvertASCIItoUTF16(extraMimeEntries[index].mDescription));
          return NS_OK;
      }
  }

  return NS_ERROR_NOT_AVAILABLE;
}

nsresult nsExternalHelperAppService::FillMIMEInfoForExtensionFromExtras(
  const nsACString& aExtension, nsIMIMEInfo * aMIMEInfo)
{
  nsCAutoString type;
  bool found = GetTypeFromExtras(aExtension, type);
  if (!found)
    return NS_ERROR_NOT_AVAILABLE;
  return FillMIMEInfoForMimeTypeFromExtras(type, aMIMEInfo);
}

bool nsExternalHelperAppService::GetTypeFromExtras(const nsACString& aExtension, nsACString& aMIMEType)
{
  NS_ASSERTION(!aExtension.IsEmpty(), "Empty aExtension parameter!");

  // Look for default entry with matching extension.
  nsDependentCString::const_iterator start, end, iter;
  PRInt32 numEntries = ArrayLength(extraMimeEntries);
  for (PRInt32 index = 0; index < numEntries; index++)
  {
      nsDependentCString extList(extraMimeEntries[index].mFileExtensions);
      extList.BeginReading(start);
      extList.EndReading(end);
      iter = start;
      while (start != end)
      {
          FindCharInReadable(',', iter, end);
          if (Substring(start, iter).Equals(aExtension,
                                            nsCaseInsensitiveCStringComparator()))
          {
              aMIMEType = extraMimeEntries[index].mMimeType;
              return true;
          }
          if (iter != end) {
            ++iter;
          }
          start = iter;
      }
  }

  return false;
}
