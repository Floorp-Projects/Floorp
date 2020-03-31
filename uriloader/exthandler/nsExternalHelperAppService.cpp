/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:cin:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

/* This must occur *after* base/basictypes.h to avoid typedefs conflicts. */
#include "mozilla/ArrayUtils.h"
#include "mozilla/Base64.h"
#include "mozilla/ResultExtensions.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "nsXULAppAPI.h"

#include "nsExternalHelperAppService.h"
#include "nsCExternalHandlerService.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsIChannel.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsICategoryManager.h"
#include "nsDependentSubstring.h"
#include "nsString.h"
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
#include "nsIRedirectHistoryEntry.h"
#include "nsOSHelperAppService.h"
#include "nsOSHelperAppServiceChild.h"

// used to access our datastore of user-configured helper applications
#include "nsIHandlerService.h"
#include "nsIMIMEInfo.h"
#include "nsIHelperAppLauncherDialog.h"
#include "nsIContentDispatchChooser.h"
#include "nsNetUtil.h"
#include "nsIPrivateBrowsingChannel.h"
#include "nsIIOService.h"
#include "nsNetCID.h"

#include "nsIApplicationReputation.h"

#include "nsDSURIContentListener.h"
#include "nsMimeTypes.h"
// used for header disposition information.
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIEncodedChannel.h"
#include "nsIMultiPartChannel.h"
#include "nsIFileChannel.h"
#include "nsIObserverService.h"  // so we can be a profile change observer
#include "nsIPropertyBag2.h"     // for the 64-bit content length

#ifdef XP_MACOSX
#  include "nsILocalFileMac.h"
#endif

#include "nsPluginHost.h"
#include "nsEscape.h"

#include "nsIStringBundle.h"  // XXX needed to localize error msgs
#include "nsIPrompt.h"

#include "nsITextToSubURI.h"  // to unescape the filename

#include "nsDocShellCID.h"

#include "nsCRT.h"
#include "nsLocalHandlerApp.h"

#include "nsIRandomGenerator.h"

#include "ContentChild.h"
#include "nsXULAppAPI.h"
#include "nsPIDOMWindow.h"
#include "ExternalHelperAppChild.h"

#ifdef XP_WIN
#  include "nsWindowsHelpers.h"
#endif

#include "mozilla/Components.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/ipc/URIUtils.h"

using namespace mozilla;
using namespace mozilla::ipc;
using namespace mozilla::dom;

// Download Folder location constants
#define NS_PREF_DOWNLOAD_DIR "browser.download.dir"
#define NS_PREF_DOWNLOAD_FOLDERLIST "browser.download.folderList"
enum {
  NS_FOLDER_VALUE_DESKTOP = 0,
  NS_FOLDER_VALUE_DOWNLOADS = 1,
  NS_FOLDER_VALUE_CUSTOM = 2
};

LazyLogModule nsExternalHelperAppService::mLog("HelperAppService");

// Using level 3 here because the OSHelperAppServices use a log level
// of LogLevel::Debug (4), and we want less detailed output here
// Using 3 instead of LogLevel::Warning because we don't output warnings
#undef LOG
#define LOG(args) \
  MOZ_LOG(nsExternalHelperAppService::mLog, mozilla::LogLevel::Info, args)
#define LOG_ENABLED() \
  MOZ_LOG_TEST(nsExternalHelperAppService::mLog, mozilla::LogLevel::Info)

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
                                 nsAString& aResult) {
  // We need the unescaper
  nsresult rv;
  nsCOMPtr<nsITextToSubURI> textToSubURI =
      do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return textToSubURI->UnEscapeURIForUI(aFragment, aResult);
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
                                 nsACString& aResult) {
  nsAutoString result;
  nsresult rv = UnescapeFragment(aFragment, aURI, result);
  if (NS_SUCCEEDED(rv)) CopyUTF16toUTF8(result, aResult);
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
                                               bool aAllowURLExtension = true) {
  aExtension.Truncate();
  /*
   * If the channel is an http or part of a multipart channel and we
   * have a content disposition header set, then use the file name
   * suggested there as the preferred file name to SUGGEST to the
   * user.  we shouldn't actually use that without their
   * permission... otherwise just use our temp file
   */
  bool handleExternally = false;
  uint32_t disp;
  nsresult rv = aChannel->GetContentDisposition(&disp);
  if (NS_SUCCEEDED(rv)) {
    aChannel->GetContentDispositionFilename(aFileName);
    if (disp == nsIChannel::DISPOSITION_ATTACHMENT) handleExternally = true;
  }

  // If the disposition header didn't work, try the filename from nsIURL
  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));
  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (url && aFileName.IsEmpty()) {
    if (aAllowURLExtension) {
      url->GetFileExtension(aExtension);
      UnescapeFragment(aExtension, url, aExtension);

      // Windows ignores terminating dots. So we have to as well, so
      // that our security checks do "the right thing"
      // In case the aExtension consisted only of the dot, the code below will
      // extract an aExtension from the filename
      aExtension.Trim(".", false);
    }

    // try to extract the file name from the url and use that as a first pass as
    // the leaf name of our temp file...
    nsAutoCString leafName;
    url->GetFileName(leafName);
    if (!leafName.IsEmpty()) {
      rv = UnescapeFragment(leafName, url, aFileName);
      if (NS_FAILED(rv)) {
        CopyUTF8toUTF16(leafName, aFileName);  // use escaped name
      }
    }
  }

  // Extract Extension, if we have a filename; otherwise,
  // truncate the string
  if (aExtension.IsEmpty()) {
    if (!aFileName.IsEmpty()) {
      // Windows ignores terminating dots. So we have to as well, so
      // that our security checks do "the right thing"
      aFileName.Trim(".", false);

      // XXX RFindCharInReadable!!
      nsAutoString fileNameStr(aFileName);
      int32_t idx = fileNameStr.RFindChar(char16_t('.'));
      if (idx != kNotFound)
        CopyUTF16toUTF8(StringTail(fileNameStr, fileNameStr.Length() - idx - 1),
                        aExtension);
    }
  }

  return handleExternally;
}

/**
 * Obtains the directory to use.  This tends to vary per platform, and
 * needs to be consistent throughout our codepaths. For platforms where
 * helper apps use the downloads directory, this should be kept in
 * sync with DownloadIntegration.jsm.
 *
 * Optionally skip availability of the directory and storage.
 */
static nsresult GetDownloadDirectory(nsIFile** _directory,
                                     bool aSkipChecks = false) {
  nsCOMPtr<nsIFile> dir;
#ifdef XP_MACOSX
  // On OS X, we first try to get the users download location, if it's set.
  switch (Preferences::GetInt(NS_PREF_DOWNLOAD_FOLDERLIST, -1)) {
    case NS_FOLDER_VALUE_DESKTOP:
      (void)NS_GetSpecialDirectory(NS_OS_DESKTOP_DIR, getter_AddRefs(dir));
      break;
    case NS_FOLDER_VALUE_CUSTOM: {
      Preferences::GetComplex(NS_PREF_DOWNLOAD_DIR, NS_GET_IID(nsIFile),
                              getter_AddRefs(dir));
      if (!dir) break;

      // If we're not checking for availability we're done.
      if (aSkipChecks) {
        dir.forget(_directory);
        return NS_OK;
      }

      // We have the directory, and now we need to make sure it exists
      bool dirExists = false;
      (void)dir->Exists(&dirExists);
      if (dirExists) break;

      nsresult rv = dir->Create(nsIFile::DIRECTORY_TYPE, 0755);
      if (NS_FAILED(rv)) {
        dir = nullptr;
        break;
      }
    } break;
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
  return NS_ERROR_FAILURE;
#else
  // On all other platforms, we default to the systems temporary directory.
  nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(dir));
  NS_ENSURE_SUCCESS(rv, rv);

#  if defined(XP_UNIX)
  // Ensuring that only the current user can read the file names we end up
  // creating. Note that Creating directories with specified permission only
  // supported on Unix platform right now. That's why above if exists.

  uint32_t permissions;
  rv = dir->GetPermissions(&permissions);
  NS_ENSURE_SUCCESS(rv, rv);

  if (permissions != PR_IRWXU) {
    const char* userName = PR_GetEnv("USERNAME");
    if (!userName || !*userName) {
      userName = PR_GetEnv("USER");
    }
    if (!userName || !*userName) {
      userName = PR_GetEnv("LOGNAME");
    }
    if (!userName || !*userName) {
      userName = "mozillaUser";
    }

    nsAutoString userDir;
    userDir.AssignLiteral("mozilla_");
    userDir.AppendASCII(userName);
    userDir.ReplaceChar(FILE_PATH_SEPARATOR FILE_ILLEGAL_CHARACTERS, '_');

    int counter = 0;
    bool pathExists;
    nsCOMPtr<nsIFile> finalPath;

    while (true) {
      nsAutoString countedUserDir(userDir);
      countedUserDir.AppendInt(counter, 10);
      dir->Clone(getter_AddRefs(finalPath));
      finalPath->Append(countedUserDir);

      rv = finalPath->Exists(&pathExists);
      NS_ENSURE_SUCCESS(rv, rv);

      if (pathExists) {
        // If this path has the right permissions, use it.
        rv = finalPath->GetPermissions(&permissions);
        NS_ENSURE_SUCCESS(rv, rv);

        // Ensuring the path is writable by the current user.
        bool isWritable;
        rv = finalPath->IsWritable(&isWritable);
        NS_ENSURE_SUCCESS(rv, rv);

        if (permissions == PR_IRWXU && isWritable) {
          dir = finalPath;
          break;
        }
      }

      rv = finalPath->Create(nsIFile::DIRECTORY_TYPE, PR_IRWXU);
      if (NS_SUCCEEDED(rv)) {
        dir = finalPath;
        break;
      } else if (rv != NS_ERROR_FILE_ALREADY_EXISTS) {
        // Unexpected error.
        return rv;
      }

      counter++;
    }
  }

#  endif
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
static const nsDefaultMimeTypeEntry defaultMimeEntries[] = {
    // The following are those extensions that we're asked about during startup,
    // sorted by order used
    {IMAGE_GIF, "gif"},
    {TEXT_XML, "xml"},
    {APPLICATION_RDF, "rdf"},
    {IMAGE_PNG, "png"},
    // -- end extensions used during startup
    {TEXT_CSS, "css"},
    {IMAGE_JPEG, "jpeg"},
    {IMAGE_JPEG, "jpg"},
    {IMAGE_SVG_XML, "svg"},
    {TEXT_HTML, "html"},
    {TEXT_HTML, "htm"},
    {APPLICATION_XPINSTALL, "xpi"},
    {"application/xhtml+xml", "xhtml"},
    {"application/xhtml+xml", "xht"},
    {TEXT_PLAIN, "txt"},
    {APPLICATION_JSON, "json"},
    {APPLICATION_XJAVASCRIPT, "js"},
    {APPLICATION_XJAVASCRIPT, "jsm"},
    {VIDEO_OGG, "ogv"},
    {VIDEO_OGG, "ogg"},
    {APPLICATION_OGG, "ogg"},
    {AUDIO_OGG, "oga"},
    {AUDIO_OGG, "opus"},
    {APPLICATION_PDF, "pdf"},
    {VIDEO_WEBM, "webm"},
    {AUDIO_WEBM, "webm"},
    {IMAGE_ICO, "ico"},
    {TEXT_PLAIN, "properties"},
    {TEXT_PLAIN, "locale"},
    {TEXT_PLAIN, "ftl"},
#if defined(MOZ_WMF)
    {VIDEO_MP4, "mp4"},
    {AUDIO_MP4, "m4a"},
    {AUDIO_MP3, "mp3"},
#endif
#ifdef MOZ_RAW
    {VIDEO_RAW, "yuv"}
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
#  define MAC_TYPE(x) x
#else
#  define MAC_TYPE(x) 0
#endif

/**
 * This table lists all of the 'extra' content types that we can deduce from
 * particular file extensions.  These entries also ensure that we provide a good
 * descriptive name when we encounter files with these content types and/or
 * extensions.  These can be overridden by user helper app prefs. If you add
 * types here, make sure they are lowercase, or you'll regret it.
 */
static const nsExtraMimeTypeEntry extraMimeEntries[] = {
#if defined(XP_MACOSX)  // don't define .bin on the mac...use internet config to
                        // look that up...
    {APPLICATION_OCTET_STREAM, "exe,com", "Binary File"},
#else
    {APPLICATION_OCTET_STREAM, "exe,com,bin", "Binary File"},
#endif
    {APPLICATION_GZIP2, "gz", "gzip"},
    {"application/x-arj", "arj", "ARJ file"},
    {"application/rtf", "rtf", "Rich Text Format File"},
    {APPLICATION_XPINSTALL, "xpi", "XPInstall Install"},
    {APPLICATION_PDF, "pdf", "Portable Document Format"},
    {APPLICATION_POSTSCRIPT, "ps,eps,ai", "Postscript File"},
    {APPLICATION_XJAVASCRIPT, "js", "Javascript Source File"},
    {APPLICATION_XJAVASCRIPT, "jsm,mjs", "Javascript Module Source File"},
#ifdef MOZ_WIDGET_ANDROID
    {"application/vnd.android.package-archive", "apk", "Android Package"},
#endif
    {IMAGE_ART, "art", "ART Image"},
    {IMAGE_BMP, "bmp", "BMP Image"},
    {IMAGE_GIF, "gif", "GIF Image"},
    {IMAGE_ICO, "ico,cur", "ICO Image"},
    {IMAGE_JPEG, "jpeg,jpg,jfif,pjpeg,pjp", "JPEG Image"},
    {IMAGE_PNG, "png", "PNG Image"},
    {IMAGE_APNG, "apng", "APNG Image"},
    {IMAGE_TIFF, "tiff,tif", "TIFF Image"},
    {IMAGE_XBM, "xbm", "XBM Image"},
    {IMAGE_SVG_XML, "svg", "Scalable Vector Graphics"},
    {IMAGE_WEBP, "webp", "WebP Image"},
    {MESSAGE_RFC822, "eml", "RFC-822 data"},
    {TEXT_PLAIN, "txt,text", "Text File"},
    {APPLICATION_JSON, "json", "JavaScript Object Notation"},
    {TEXT_VTT, "vtt", "Web Video Text Tracks"},
    {TEXT_CACHE_MANIFEST, "appcache", "Application Cache Manifest"},
    {TEXT_HTML, "html,htm,shtml,ehtml", "HyperText Markup Language"},
    {"application/xhtml+xml", "xhtml,xht",
     "Extensible HyperText Markup Language"},
    {APPLICATION_MATHML_XML, "mml", "Mathematical Markup Language"},
    {APPLICATION_RDF, "rdf", "Resource Description Framework"},
    {TEXT_XML, "xml,xsl,xbl", "Extensible Markup Language"},
    {TEXT_CSS, "css", "Style Sheet"},
    {TEXT_VCARD, "vcf,vcard", "Contact Information"},
    {VIDEO_OGG, "ogv", "Ogg Video"},
    {VIDEO_OGG, "ogg", "Ogg Video"},
    {APPLICATION_OGG, "ogg", "Ogg Video"},
    {AUDIO_OGG, "oga", "Ogg Audio"},
    {AUDIO_OGG, "opus", "Opus Audio"},
    {VIDEO_WEBM, "webm", "Web Media Video"},
    {AUDIO_WEBM, "webm", "Web Media Audio"},
    {AUDIO_MP3, "mp3", "MPEG Audio"},
    {VIDEO_MP4, "mp4", "MPEG-4 Video"},
    {AUDIO_MP4, "m4a", "MPEG-4 Audio"},
    {VIDEO_RAW, "yuv", "Raw YUV Video"},
    {AUDIO_WAV, "wav", "Waveform Audio"},
    {VIDEO_3GPP, "3gpp,3gp", "3GPP Video"},
    {VIDEO_3GPP2, "3g2", "3GPP2 Video"},
    {AUDIO_MIDI, "mid", "Standard MIDI Audio"},
    {APPLICATION_WASM, "wasm", "WebAssembly Module"}};

#undef MAC_TYPE

/**
 * File extensions for which decoding should be disabled.
 * NOTE: These MUST be lower-case and ASCII.
 */
static const nsDefaultMimeTypeEntry nonDecodableExtensions[] = {
    {APPLICATION_GZIP, "gz"},
    {APPLICATION_GZIP, "tgz"},
    {APPLICATION_ZIP, "zip"},
    {APPLICATION_COMPRESS, "z"},
    {APPLICATION_GZIP, "svgz"}};

static StaticRefPtr<nsExternalHelperAppService> sExtHelperAppSvcSingleton;

/**
 * On Mac child processes, return an nsOSHelperAppServiceChild for remoting
 * OS calls to the parent process. On all other platforms use
 * nsOSHelperAppService.
 */
/* static */
already_AddRefed<nsExternalHelperAppService>
nsExternalHelperAppService::GetSingleton() {
  if (!sExtHelperAppSvcSingleton) {
#ifdef XP_MACOSX
    if (XRE_IsParentProcess()) {
      sExtHelperAppSvcSingleton = new nsOSHelperAppService();
    } else {
      sExtHelperAppSvcSingleton = new nsOSHelperAppServiceChild();
    }
#else
    sExtHelperAppSvcSingleton = new nsOSHelperAppService();
#endif /* XP_MACOSX */
    ClearOnShutdown(&sExtHelperAppSvcSingleton);
  }

  return do_AddRef(sExtHelperAppSvcSingleton);
}

NS_IMPL_ISUPPORTS(nsExternalHelperAppService, nsIExternalHelperAppService,
                  nsPIExternalAppLauncher, nsIExternalProtocolService,
                  nsIMIMEService, nsIObserver, nsISupportsWeakReference)

nsExternalHelperAppService::nsExternalHelperAppService() {}
nsresult nsExternalHelperAppService::Init() {
  // Add an observer for profile change
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) return NS_ERROR_FAILURE;

  nsresult rv = obs->AddObserver(this, "profile-before-change", true);
  NS_ENSURE_SUCCESS(rv, rv);
  return obs->AddObserver(this, "last-pb-context-exited", true);
}

nsExternalHelperAppService::~nsExternalHelperAppService() {}

nsresult nsExternalHelperAppService::DoContentContentProcessHelper(
    const nsACString& aMimeContentType, nsIRequest* aRequest,
    BrowsingContext* aContentContext, bool aForceSave,
    nsIInterfaceRequestor* aWindowContext,
    nsIStreamListener** aStreamListener) {
  // We need to get a hold of a ContentChild so that we can begin forwarding
  // this data to the parent.  In the HTTP case, this is unfortunate, since
  // we're actually passing data from parent->child->parent wastefully, but
  // the Right Fix will eventually be to short-circuit those channels on the
  // parent side based on some sort of subscription concept.
  using mozilla::dom::ContentChild;
  using mozilla::dom::ExternalHelperAppChild;
  ContentChild* child = ContentChild::GetSingleton();
  if (!child) {
    return NS_ERROR_FAILURE;
  }

  nsCString disp;
  nsCOMPtr<nsIURI> uri;
  int64_t contentLength = -1;
  bool wasFileChannel = false;
  uint32_t contentDisposition = -1;
  nsAutoString fileName;
  nsCOMPtr<nsILoadInfo> loadInfo;

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  if (channel) {
    channel->GetURI(getter_AddRefs(uri));
    channel->GetContentLength(&contentLength);
    channel->GetContentDisposition(&contentDisposition);
    channel->GetContentDispositionFilename(fileName);
    channel->GetContentDispositionHeader(disp);
    loadInfo = channel->LoadInfo();

    nsCOMPtr<nsIFileChannel> fileChan(do_QueryInterface(aRequest));
    wasFileChannel = fileChan != nullptr;
  }

  nsCOMPtr<nsIURI> referrer;
  NS_GetReferrerFromChannel(channel, getter_AddRefs(referrer));

  Maybe<URIParams> uriParams, referrerParams;
  SerializeURI(uri, uriParams);
  SerializeURI(referrer, referrerParams);

  Maybe<mozilla::net::LoadInfoArgs> loadInfoArgs;
  MOZ_ALWAYS_SUCCEEDS(LoadInfoToLoadInfoArgs(loadInfo, &loadInfoArgs));

  nsCOMPtr<nsIPropertyBag2> props(do_QueryInterface(aRequest));
  // Determine whether a new window was opened specifically for this request
  bool shouldCloseWindow = false;
  if (props) {
    props->GetPropertyAsBool(NS_LITERAL_STRING("docshell.newWindowTarget"),
                             &shouldCloseWindow);
  }

  // Now we build a protocol for forwarding our data to the parent.  The
  // protocol will act as a listener on the child-side and create a "real"
  // helperAppService listener on the parent-side, via another call to
  // DoContent.
  RefPtr<ExternalHelperAppChild> childListener = new ExternalHelperAppChild();
  MOZ_ALWAYS_TRUE(child->SendPExternalHelperAppConstructor(
      childListener, uriParams, loadInfoArgs, nsCString(aMimeContentType), disp,
      contentDisposition, fileName, aForceSave, contentLength, wasFileChannel,
      referrerParams, aContentContext, shouldCloseWindow));

  NS_ADDREF(*aStreamListener = childListener);

  uint32_t reason = nsIHelperAppLauncherDialog::REASON_CANTHANDLE;

  RefPtr<nsExternalAppHandler> handler = new nsExternalAppHandler(
      nullptr, EmptyCString(), aContentContext, aWindowContext, this, fileName,
      reason, aForceSave);
  if (!handler) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  childListener->SetHandler(handler);
  return NS_OK;
}

NS_IMETHODIMP nsExternalHelperAppService::CreateListener(
    const nsACString& aMimeContentType, nsIRequest* aRequest,
    BrowsingContext* aContentContext, bool aForceSave,
    nsIInterfaceRequestor* aWindowContext,
    nsExternalAppHandler** aStreamListener) {
  MOZ_ASSERT(!XRE_IsContentProcess());

  nsAutoString fileName;
  nsAutoCString fileExtension;
  uint32_t reason = nsIHelperAppLauncherDialog::REASON_CANTHANDLE;
  uint32_t contentDisposition = -1;

  // Get the file extension and name that we will need later
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  nsCOMPtr<nsIURI> uri;
  int64_t contentLength = -1;
  if (channel) {
    channel->GetURI(getter_AddRefs(uri));
    channel->GetContentLength(&contentLength);
    channel->GetContentDisposition(&contentDisposition);
    channel->GetContentDispositionFilename(fileName);

    // Check if we have a POST request, in which case we don't want to use
    // the url's extension
    bool allowURLExt = !net::ChannelIsPost(channel);

    // Check if we had a query string - we don't want to check the URL
    // extension if a query is present in the URI
    // If we already know we don't want to check the URL extension, don't
    // bother checking the query
    if (uri && allowURLExt) {
      nsCOMPtr<nsIURL> url = do_QueryInterface(uri);

      if (url) {
        nsAutoCString query;

        // We only care about the query for HTTP and HTTPS URLs
        if (uri->SchemeIs("http") || uri->SchemeIs("https")) {
          url->GetQuery(query);
        }

        // Only get the extension if the query is empty; if it isn't, then the
        // extension likely belongs to a cgi script and isn't helpful
        allowURLExt = query.IsEmpty();
      }
    }
    // Extract name & extension
    bool isAttachment = GetFilenameAndExtensionFromChannel(
        channel, fileName, fileExtension, allowURLExt);
    LOG(("Found extension '%s' (filename is '%s', handling attachment: %i)",
         fileExtension.get(), NS_ConvertUTF16toUTF8(fileName).get(),
         isAttachment));
    if (isAttachment) {
      reason = nsIHelperAppLauncherDialog::REASON_SERVERREQUEST;
    }
  }

  LOG(("HelperAppService::DoContent: mime '%s', extension '%s'\n",
       PromiseFlatCString(aMimeContentType).get(), fileExtension.get()));

  // We get the mime service here even though we're the default implementation
  // of it, so it's possible to override only the mime service and not need to
  // reimplement the whole external helper app service itself.
  nsCOMPtr<nsIMIMEService> mimeSvc(do_GetService(NS_MIMESERVICE_CONTRACTID));
  NS_ENSURE_TRUE(mimeSvc, NS_ERROR_FAILURE);

  // Try to find a mime object by looking at the mime type/extension
  nsCOMPtr<nsIMIMEInfo> mimeInfo;
  if (aMimeContentType.Equals(APPLICATION_GUESS_FROM_EXT,
                              nsCaseInsensitiveCStringComparator())) {
    nsAutoCString mimeType;
    if (!fileExtension.IsEmpty()) {
      mimeSvc->GetFromTypeAndExtension(EmptyCString(), fileExtension,
                                       getter_AddRefs(mimeInfo));
      if (mimeInfo) {
        mimeInfo->GetMIMEType(mimeType);

        LOG(("OS-Provided mime type '%s' for extension '%s'\n", mimeType.get(),
             fileExtension.get()));
      }
    }

    if (fileExtension.IsEmpty() || mimeType.IsEmpty()) {
      // Extension lookup gave us no useful match
      mimeSvc->GetFromTypeAndExtension(
          NS_LITERAL_CSTRING(APPLICATION_OCTET_STREAM), fileExtension,
          getter_AddRefs(mimeInfo));
      mimeType.AssignLiteral(APPLICATION_OCTET_STREAM);
    }

    if (channel) {
      channel->SetContentType(mimeType);
    }

    // Don't overwrite SERVERREQUEST
    if (reason == nsIHelperAppLauncherDialog::REASON_CANTHANDLE) {
      reason = nsIHelperAppLauncherDialog::REASON_TYPESNIFFED;
    }
  } else {
    mimeSvc->GetFromTypeAndExtension(aMimeContentType, fileExtension,
                                     getter_AddRefs(mimeInfo));
  }
  LOG(("Type/Ext lookup found 0x%p\n", mimeInfo.get()));

  // No mimeinfo -> we can't continue. probably OOM.
  if (!mimeInfo) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aStreamListener = nullptr;
  // We want the mimeInfo's primary extension to pass it to
  // nsExternalAppHandler
  nsAutoCString buf;
  mimeInfo->GetPrimaryExtension(buf);

  // NB: ExternalHelperAppParent depends on this listener always being an
  // nsExternalAppHandler. If this changes, make sure to update that code.
  nsExternalAppHandler* handler =
      new nsExternalAppHandler(mimeInfo, buf, aContentContext, aWindowContext,
                               this, fileName, reason, aForceSave);
  if (!handler) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aStreamListener = handler);
  return NS_OK;
}

NS_IMETHODIMP nsExternalHelperAppService::DoContent(
    const nsACString& aMimeContentType, nsIRequest* aRequest,
    nsIInterfaceRequestor* aContentContext, bool aForceSave,
    nsIInterfaceRequestor* aWindowContext,
    nsIStreamListener** aStreamListener) {
  // Scripted interface requestors cannot return an instance of the
  // (non-scriptable) nsPIDOMWindowOuter or nsPIDOMWindowInner interfaces, so
  // get to the window via `nsIDOMWindow`.  Unfortunately, at that point we
  // don't know whether the thing we got is an inner or outer window, so have to
  // work with either one.
  RefPtr<BrowsingContext> bc;
  nsCOMPtr<nsIDOMWindow> domWindow = do_GetInterface(aContentContext);
  if (nsCOMPtr<nsPIDOMWindowOuter> outerWindow = do_QueryInterface(domWindow)) {
    bc = outerWindow->GetBrowsingContext();
  } else if (nsCOMPtr<nsPIDOMWindowInner> innerWindow =
                 do_QueryInterface(domWindow)) {
    bc = innerWindow->GetBrowsingContext();
  }

  if (XRE_IsContentProcess()) {
    return DoContentContentProcessHelper(aMimeContentType, aRequest, bc,
                                         aForceSave, aWindowContext,
                                         aStreamListener);
  }

  RefPtr<nsExternalAppHandler> handler;
  nsresult rv = CreateListener(aMimeContentType, aRequest, bc, aForceSave,
                               aWindowContext, getter_AddRefs(handler));
  handler.forget(aStreamListener);
  return rv;
}

NS_IMETHODIMP nsExternalHelperAppService::ApplyDecodingForExtension(
    const nsACString& aExtension, const nsACString& aEncodingType,
    bool* aApplyDecoding) {
  *aApplyDecoding = true;
  uint32_t i;
  for (i = 0; i < ArrayLength(nonDecodableExtensions); ++i) {
    if (aExtension.LowerCaseEqualsASCII(
            nonDecodableExtensions[i].mFileExtension) &&
        aEncodingType.LowerCaseEqualsASCII(
            nonDecodableExtensions[i].mMimeType)) {
      *aApplyDecoding = false;
      break;
    }
  }
  return NS_OK;
}

nsresult nsExternalHelperAppService::GetFileTokenForPath(
    const char16_t* aPlatformAppPath, nsIFile** aFile) {
  nsDependentString platformAppPath(aPlatformAppPath);
  // First, check if we have an absolute path
  nsIFile* localFile = nullptr;
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
      if (NS_SUCCEEDED(rv) && exists) return NS_OK;
    }
    NS_RELEASE(*aFile);
  }

  return NS_ERROR_NOT_AVAILABLE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// begin external protocol service default implementation...
//////////////////////////////////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsExternalHelperAppService::ExternalProtocolHandlerExists(
    const char* aProtocolScheme, bool* aHandlerExists) {
  nsCOMPtr<nsIHandlerInfo> handlerInfo;
  nsresult rv = GetProtocolHandlerInfo(nsDependentCString(aProtocolScheme),
                                       getter_AddRefs(handlerInfo));
  if (NS_SUCCEEDED(rv)) {
    // See if we have any known possible handler apps for this
    nsCOMPtr<nsIMutableArray> possibleHandlers;
    handlerInfo->GetPossibleApplicationHandlers(
        getter_AddRefs(possibleHandlers));

    uint32_t length;
    possibleHandlers->GetLength(&length);
    if (length) {
      *aHandlerExists = true;
      return NS_OK;
    }
  }

  // if not, fall back on an os-based handler
  return OSProtocolHandlerExists(aProtocolScheme, aHandlerExists);
}

NS_IMETHODIMP nsExternalHelperAppService::IsExposedProtocol(
    const char* aProtocolScheme, bool* aResult) {
  // check the per protocol setting first.  it always takes precedence.
  // if not set, then use the global setting.

  nsAutoCString prefName("network.protocol-handler.expose.");
  prefName += aProtocolScheme;
  bool val;
  if (NS_SUCCEEDED(Preferences::GetBool(prefName.get(), &val))) {
    *aResult = val;
    return NS_OK;
  }

  // by default, no protocol is exposed.  i.e., by default all link clicks must
  // go through the external protocol service.  most applications override this
  // default behavior.
  *aResult = Preferences::GetBool("network.protocol-handler.expose-all", false);

  return NS_OK;
}

static const char kExternalProtocolPrefPrefix[] =
    "network.protocol-handler.external.";
static const char kExternalProtocolDefaultPref[] =
    "network.protocol-handler.external-default";

NS_IMETHODIMP
nsExternalHelperAppService::LoadURI(nsIURI* aURI,
                                    nsIInterfaceRequestor* aWindowContext) {
  NS_ENSURE_ARG_POINTER(aURI);

  if (XRE_IsContentProcess()) {
    URIParams uri;
    SerializeURI(aURI, uri);

    nsCOMPtr<nsIBrowserChild> browserChild(do_GetInterface(aWindowContext));
    mozilla::dom::ContentChild::GetSingleton()->SendLoadURIExternal(
        uri, static_cast<dom::BrowserChild*>(browserChild.get()));
    return NS_OK;
  }

  nsAutoCString spec;
  aURI->GetSpec(spec);

  if (spec.Find("%00") != -1) return NS_ERROR_MALFORMED_URI;

  spec.ReplaceSubstring("\"", "%22");
  spec.ReplaceSubstring("`", "%60");

  nsCOMPtr<nsIIOService> ios(do_GetIOService());
  nsCOMPtr<nsIURI> uri;
  nsresult rv = ios->NewURI(spec, nullptr, nullptr, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString scheme;
  uri->GetScheme(scheme);
  if (scheme.IsEmpty()) return NS_OK;  // must have a scheme

  // Deny load if the prefs say to do so
  nsAutoCString externalPref(kExternalProtocolPrefPrefix);
  externalPref += scheme;
  bool allowLoad = false;
  if (NS_FAILED(Preferences::GetBool(externalPref.get(), &allowLoad))) {
    // no scheme-specific value, check the default
    if (NS_FAILED(
            Preferences::GetBool(kExternalProtocolDefaultPref, &allowLoad))) {
      return NS_OK;  // missing default pref
    }
  }

  if (!allowLoad) {
    return NS_OK;  // explicitly denied
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
                     preferredAction == nsIHandlerInfo::useSystemDefault)) {
    rv = handler->LaunchWithURI(uri, aWindowContext);
    // We are not supposed to ask, but when file not found the user most likely
    // uninstalled the application which handles the uri so we will continue
    // by application chooser dialog.
    if (rv != NS_ERROR_FILE_NOT_FOUND) {
      return rv;
    }
  }

  nsCOMPtr<nsIContentDispatchChooser> chooser =
      do_CreateInstance("@mozilla.org/content-dispatch-chooser;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return chooser->Ask(handler, aWindowContext, uri,
                      nsIContentDispatchChooser::REASON_CANNOT_HANDLE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Methods related to deleting temporary files on exit
//////////////////////////////////////////////////////////////////////////////////////////////////////

/* static */
nsresult nsExternalHelperAppService::DeleteTemporaryFileHelper(
    nsIFile* aTemporaryFile, nsCOMArray<nsIFile>& aFileList) {
  bool isFile = false;

  // as a safety measure, make sure the nsIFile is really a file and not a
  // directory object.
  aTemporaryFile->IsFile(&isFile);
  if (!isFile) return NS_OK;

  aFileList.AppendObject(aTemporaryFile);

  return NS_OK;
}

NS_IMETHODIMP
nsExternalHelperAppService::DeleteTemporaryFileOnExit(nsIFile* aTemporaryFile) {
  return DeleteTemporaryFileHelper(aTemporaryFile, mTemporaryFilesList);
}

NS_IMETHODIMP
nsExternalHelperAppService::DeleteTemporaryPrivateFileWhenPossible(
    nsIFile* aTemporaryFile) {
  return DeleteTemporaryFileHelper(aTemporaryFile, mTemporaryPrivateFilesList);
}

void nsExternalHelperAppService::ExpungeTemporaryFilesHelper(
    nsCOMArray<nsIFile>& fileList) {
  int32_t numEntries = fileList.Count();
  nsIFile* localFile;
  for (int32_t index = 0; index < numEntries; index++) {
    localFile = fileList[index];
    if (localFile) {
      // First make the file writable, since the temp file is probably readonly.
      localFile->SetPermissions(0600);
      localFile->Remove(false);
    }
  }

  fileList.Clear();
}

void nsExternalHelperAppService::ExpungeTemporaryFiles() {
  ExpungeTemporaryFilesHelper(mTemporaryFilesList);
}

void nsExternalHelperAppService::ExpungeTemporaryPrivateFiles() {
  ExpungeTemporaryFilesHelper(mTemporaryPrivateFilesList);
}

static const char kExternalWarningPrefPrefix[] =
    "network.protocol-handler.warn-external.";
static const char kExternalWarningDefaultPref[] =
    "network.protocol-handler.warn-external-default";

NS_IMETHODIMP
nsExternalHelperAppService::GetProtocolHandlerInfo(
    const nsACString& aScheme, nsIHandlerInfo** aHandlerInfo) {
  // XXX enterprise customers should be able to turn this support off with a
  // single master pref (maybe use one of the "exposed" prefs here?)

  bool exists;
  nsresult rv = GetProtocolHandlerInfoFromOS(aScheme, &exists, aHandlerInfo);
  if (NS_FAILED(rv)) {
    // Either it knows nothing, or we ran out of memory
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIHandlerService> handlerSvc =
      do_GetService(NS_HANDLERSERVICE_CONTRACTID);
  if (handlerSvc) {
    bool hasHandler = false;
    (void)handlerSvc->Exists(*aHandlerInfo, &hasHandler);
    if (hasHandler) {
      rv = handlerSvc->FillHandlerInfo(*aHandlerInfo, EmptyCString());
      if (NS_SUCCEEDED(rv)) return NS_OK;
    }
  }

  return SetProtocolHandlerDefaults(*aHandlerInfo, exists);
}

NS_IMETHODIMP
nsExternalHelperAppService::SetProtocolHandlerDefaults(
    nsIHandlerInfo* aHandlerInfo, bool aOSHandlerExists) {
  // this type isn't in our database, so we've only got an OS default handler,
  // if one exists

  if (aOSHandlerExists) {
    // we've got a default, so use it
    aHandlerInfo->SetPreferredAction(nsIHandlerInfo::useSystemDefault);

    // whether or not to ask the user depends on the warning preference
    nsAutoCString scheme;
    aHandlerInfo->GetType(scheme);

    nsAutoCString warningPref(kExternalWarningPrefPrefix);
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
nsExternalHelperAppService::Observe(nsISupports* aSubject, const char* aTopic,
                                    const char16_t* someData) {
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

NS_IMPL_ADDREF(nsExternalAppHandler)
NS_IMPL_RELEASE(nsExternalAppHandler)

NS_INTERFACE_MAP_BEGIN(nsExternalAppHandler)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIHelperAppLauncher)
  NS_INTERFACE_MAP_ENTRY(nsICancelable)
  NS_INTERFACE_MAP_ENTRY(nsIBackgroundFileSaverObserver)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
NS_INTERFACE_MAP_END

nsExternalAppHandler::nsExternalAppHandler(
    nsIMIMEInfo* aMIMEInfo, const nsACString& aTempFileExtension,
    BrowsingContext* aBrowsingContext, nsIInterfaceRequestor* aWindowContext,
    nsExternalHelperAppService* aExtProtSvc,
    const nsAString& aSuggestedFilename, uint32_t aReason, bool aForceSave)
    : mMimeInfo(aMIMEInfo),
      mBrowsingContext(aBrowsingContext),
      mWindowContext(aWindowContext),
      mSuggestedFileName(aSuggestedFilename),
      mForceSave(aForceSave),
      mCanceled(false),
      mStopRequestIssued(false),
      mIsFileChannel(false),
      mShouldCloseWindow(false),
      mReason(aReason),
      mTempFileIsExecutable(false),
      mTimeDownloadStarted(0),
      mContentLength(-1),
      mProgress(0),
      mSaver(nullptr),
      mDialogProgressListener(nullptr),
      mTransfer(nullptr),
      mRequest(nullptr),
      mExtProtSvc(aExtProtSvc) {
  // make sure the extention includes the '.'
  if (!aTempFileExtension.IsEmpty() && aTempFileExtension.First() != '.')
    mTempFileExtension = char16_t('.');
  AppendUTF8toUTF16(aTempFileExtension, mTempFileExtension);

  // Get mSuggestedFileName's current file extension.
  nsAutoString originalFileExt;
  int32_t pos = mSuggestedFileName.RFindChar('.');
  if (pos != kNotFound) {
    mSuggestedFileName.Right(originalFileExt,
                             mSuggestedFileName.Length() - pos);
  }

  // replace platform specific path separator and illegal characters to avoid
  // any confusion.
  // Try to keep the use of spaces or underscores in sync with the Downloads
  // code sanitization in DownloadPaths.jsm
  mSuggestedFileName.ReplaceChar(KNOWN_PATH_SEPARATORS, '_');
  mSuggestedFileName.ReplaceChar(FILE_ILLEGAL_CHARACTERS, ' ');
  mTempFileExtension.ReplaceChar(KNOWN_PATH_SEPARATORS, '_');
  mTempFileExtension.ReplaceChar(FILE_ILLEGAL_CHARACTERS, ' ');

  // Remove unsafe bidi characters which might have spoofing implications (bug
  // 511521).
  const char16_t unsafeBidiCharacters[] = {
      char16_t(0x061c),  // Arabic Letter Mark
      char16_t(0x200e),  // Left-to-Right Mark
      char16_t(0x200f),  // Right-to-Left Mark
      char16_t(0x202a),  // Left-to-Right Embedding
      char16_t(0x202b),  // Right-to-Left Embedding
      char16_t(0x202c),  // Pop Directional Formatting
      char16_t(0x202d),  // Left-to-Right Override
      char16_t(0x202e),  // Right-to-Left Override
      char16_t(0x2066),  // Left-to-Right Isolate
      char16_t(0x2067),  // Right-to-Left Isolate
      char16_t(0x2068),  // First Strong Isolate
      char16_t(0x2069),  // Pop Directional Isolate
      char16_t(0)};
  mSuggestedFileName.ReplaceChar(unsafeBidiCharacters, '_');
  mTempFileExtension.ReplaceChar(unsafeBidiCharacters, '_');

  // Remove trailing or leading spaces that we may have generated while
  // sanitizing.
  mSuggestedFileName.CompressWhitespace();
  mTempFileExtension.CompressWhitespace();

  // Append after removing trailing whitespaces from the name.
  if (originalFileExt.FindCharInSet(
          KNOWN_PATH_SEPARATORS FILE_ILLEGAL_CHARACTERS) != kNotFound) {
    // The file extension contains invalid characters and using it would
    // generate an unusable file, thus use mTempFileExtension instead.
    mSuggestedFileName.Append(mTempFileExtension);
    originalFileExt = mTempFileExtension;
  }

  // Make sure later we won't append mTempFileExtension if it's identical to
  // the currently used file extension.
  EnsureTempFileExtension(originalFileExt);

  mBufferSize = Preferences::GetUint("network.buffer.cache.size", 4096);
}

nsExternalAppHandler::~nsExternalAppHandler() {
  MOZ_ASSERT(!mSaver, "Saver should hold a reference to us until deleted");
}

void nsExternalAppHandler::DidDivertRequest(nsIRequest* request) {
  MOZ_ASSERT(XRE_IsContentProcess(), "in child process");
  // Remove our request from the child loadGroup
  RetargetLoadNotifications(request);
}

NS_IMETHODIMP nsExternalAppHandler::SetWebProgressListener(
    nsIWebProgressListener2* aWebProgressListener) {
  // This is always called by nsHelperDlg.js. Go ahead and register the
  // progress listener. At this point, we don't have mTransfer.
  mDialogProgressListener = aWebProgressListener;
  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::GetTargetFile(nsIFile** aTarget) {
  if (mFinalFileDestination)
    *aTarget = mFinalFileDestination;
  else
    *aTarget = mTempFile;

  NS_IF_ADDREF(*aTarget);
  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::GetTargetFileIsExecutable(bool* aExec) {
  // Use the real target if it's been set
  if (mFinalFileDestination) return mFinalFileDestination->IsExecutable(aExec);

  // Otherwise, use the stored executable-ness of the temporary
  *aExec = mTempFileIsExecutable;
  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::GetTimeDownloadStarted(PRTime* aTime) {
  *aTime = mTimeDownloadStarted;
  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::GetContentLength(int64_t* aContentLength) {
  *aContentLength = mContentLength;
  return NS_OK;
}

void nsExternalAppHandler::RetargetLoadNotifications(nsIRequest* request) {
  // we are going to run the downloading of the helper app in our own little
  // docloader / load group context. so go ahead and force the creation of a
  // load group and doc loader for us to use...
  nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(request);
  if (!aChannel) return;

  bool isPrivate = NS_UsePrivateBrowsing(aChannel);

  nsCOMPtr<nsILoadGroup> oldLoadGroup;
  aChannel->GetLoadGroup(getter_AddRefs(oldLoadGroup));

  if (oldLoadGroup) {
    oldLoadGroup->RemoveRequest(request, nullptr, NS_BINDING_RETARGETED);
  }

  aChannel->SetLoadGroup(nullptr);
  aChannel->SetNotificationCallbacks(nullptr);

  nsCOMPtr<nsIPrivateBrowsingChannel> pbChannel = do_QueryInterface(aChannel);
  if (pbChannel) {
    pbChannel->SetPrivate(isPrivate);
  }
}

/**
 * Make mTempFileExtension contain an extension exactly when its previous value
 * is different from mSuggestedFileName's extension, so that it can be appended
 * to mSuggestedFileName and form a valid, useful leaf name.
 * This is required so that the (renamed) temporary file has the correct
 * extension after downloading to make sure the OS will launch the application
 * corresponding to the MIME type (which was used to calculate
 * mTempFileExtension).  This prevents a cgi-script named foobar.exe that
 * returns application/zip from being named foobar.exe and executed as an
 * executable file. It also blocks content that a web site might provide with a
 * content-disposition header indicating filename="foobar.exe" from being
 * downloaded to a file with extension .exe and executed.
 */
void nsExternalAppHandler::EnsureTempFileExtension(const nsString& aFileExt) {
  // Make sure there is a mTempFileExtension (not "" or ".").
  // Remember that mTempFileExtension will always have the leading "."
  // (the check for empty is just to be safe).
  if (mTempFileExtension.Length() > 1) {
    // Now, compare fileExt to mTempFileExtension.
    if (aFileExt.Equals(mTempFileExtension,
                        nsCaseInsensitiveStringComparator())) {
      // Matches -> mTempFileExtension can be empty
      mTempFileExtension.Truncate();
    }
  }
}

nsresult nsExternalAppHandler::SetUpTempFile(nsIChannel* aChannel) {
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

  const uint32_t wantedFileNameLength = 8;
  const uint32_t requiredBytesLength =
      static_cast<uint32_t>((wantedFileNameLength + 1) / 4 * 3);

  nsCOMPtr<nsIRandomGenerator> rg =
      do_GetService("@mozilla.org/security/random-generator;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  uint8_t* buffer;
  rv = rg->GenerateRandomBytes(requiredBytesLength, &buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString tempLeafName;
  nsDependentCSubstring randomData(reinterpret_cast<const char*>(buffer),
                                   requiredBytesLength);
  rv = Base64Encode(randomData, tempLeafName);
  free(buffer);
  buffer = nullptr;
  NS_ENSURE_SUCCESS(rv, rv);

  tempLeafName.Truncate(wantedFileNameLength);

  // Base64 characters are alphanumeric (a-zA-Z0-9) and '+' and '/', so we need
  // to replace illegal characters -- notably '/'
  tempLeafName.ReplaceChar(KNOWN_PATH_SEPARATORS FILE_ILLEGAL_CHARACTERS, '_');

  // now append our extension.
  nsAutoCString ext;
  mMimeInfo->GetPrimaryExtension(ext);
  if (!ext.IsEmpty()) {
    ext.ReplaceChar(KNOWN_PATH_SEPARATORS FILE_ILLEGAL_CHARACTERS, '_');
    if (ext.First() != '.') tempLeafName.Append('.');
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
  tempLeafName.AppendLiteral(".part");

  rv = mTempFile->Append(NS_ConvertUTF8toUTF16(tempLeafName));
  // make this file unique!!!
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mTempFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now save the temp leaf name, minus the ".part" bit, so we can use it later.
  // This is a bit broken in the case when createUnique actually had to append
  // some numbers, because then we now have a filename like foo.bar-1.part and
  // we'll end up with foo.bar-1.bar as our final filename if we end up using
  // this.  But the other options are all bad too....  Ideally we'd have a way
  // to tell createUnique to put its unique marker before the extension that
  // comes before ".part" or something.
  rv = mTempFile->GetLeafName(mTempLeafName);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(StringEndsWith(mTempLeafName, NS_LITERAL_STRING(".part")),
                 NS_ERROR_UNEXPECTED);

  // Strip off the ".part" from mTempLeafName
  mTempLeafName.Truncate(mTempLeafName.Length() - ArrayLength(".part") + 1);

  MOZ_ASSERT(!mSaver, "Output file initialization called more than once!");
  mSaver =
      do_CreateInstance(NS_BACKGROUNDFILESAVERSTREAMLISTENER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mSaver->SetObserver(this);
  if (NS_FAILED(rv)) {
    mSaver = nullptr;
    return rv;
  }

  rv = mSaver->EnableSha256();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mSaver->EnableSignatureInfo();
  NS_ENSURE_SUCCESS(rv, rv);
  LOG(("Enabled hashing and signature verification"));

  rv = mSaver->SetTarget(mTempFile, false);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

void nsExternalAppHandler::MaybeApplyDecodingForExtension(
    nsIRequest* aRequest) {
  MOZ_ASSERT(aRequest);

  nsCOMPtr<nsIEncodedChannel> encChannel = do_QueryInterface(aRequest);
  if (!encChannel) {
    return;
  }

  // Turn off content encoding conversions if needed
  bool applyConversion = true;

  // First, check to see if conversion is already disabled.  If so, we
  // have nothing to do here.
  encChannel->GetApplyConversion(&applyConversion);
  if (!applyConversion) {
    return;
  }

  nsCOMPtr<nsIURL> sourceURL(do_QueryInterface(mSourceUrl));
  if (sourceURL) {
    nsAutoCString extension;
    sourceURL->GetFileExtension(extension);
    if (!extension.IsEmpty()) {
      nsCOMPtr<nsIUTF8StringEnumerator> encEnum;
      encChannel->GetContentEncodings(getter_AddRefs(encEnum));
      if (encEnum) {
        bool hasMore;
        nsresult rv = encEnum->HasMore(&hasMore);
        if (NS_SUCCEEDED(rv) && hasMore) {
          nsAutoCString encType;
          rv = encEnum->GetNext(encType);
          if (NS_SUCCEEDED(rv) && !encType.IsEmpty()) {
            MOZ_ASSERT(mExtProtSvc);
            mExtProtSvc->ApplyDecodingForExtension(extension, encType,
                                                   &applyConversion);
          }
        }
      }
    }
  }

  encChannel->SetApplyConversion(applyConversion);
}

already_AddRefed<nsIInterfaceRequestor>
nsExternalAppHandler::GetDialogParent() {
  nsCOMPtr<nsIInterfaceRequestor> dialogParent = mWindowContext;

  if (!dialogParent && mBrowsingContext) {
    dialogParent = do_QueryInterface(mBrowsingContext->GetDOMWindow());
  }
  if (!dialogParent && mBrowsingContext && XRE_IsParentProcess()) {
    RefPtr<Element> element = mBrowsingContext->Top()->GetEmbedderElement();
    if (element) {
      dialogParent = do_QueryInterface(element->OwnerDoc()->GetWindow());
    }
  }
  return dialogParent.forget();
}

NS_IMETHODIMP nsExternalAppHandler::OnStartRequest(nsIRequest* request) {
  MOZ_ASSERT(request, "OnStartRequest without request?");

  // Set mTimeDownloadStarted here as the download has already started and
  // we want to record the start time before showing the filepicker.
  mTimeDownloadStarted = PR_Now();

  mRequest = request;

  nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(request);

  nsresult rv;

  nsCOMPtr<nsIFileChannel> fileChan(do_QueryInterface(request));
  mIsFileChannel = fileChan != nullptr;
  if (!mIsFileChannel) {
    // It's possible that this request came from the child process and the
    // file channel actually lives there. If this returns true, then our
    // mSourceUrl will be an nsIFileURL anyway.
    nsCOMPtr<dom::nsIExternalHelperAppParent> parent(
        do_QueryInterface(request));
    mIsFileChannel = parent && parent->WasFileChannel();
  }

  // Get content length
  if (aChannel) {
    aChannel->GetContentLength(&mContentLength);
  }

  if (mBrowsingContext) {
    mMaybeCloseWindowHelper = new MaybeCloseWindowHelper(mBrowsingContext);
    mMaybeCloseWindowHelper->SetShouldCloseWindow(mShouldCloseWindow);

    nsCOMPtr<nsIPropertyBag2> props(do_QueryInterface(request, &rv));
    // Determine whether a new window was opened specifically for this request
    if (props) {
      bool tmp = false;
      if (NS_SUCCEEDED(props->GetPropertyAsBool(
              NS_LITERAL_STRING("docshell.newWindowTarget"), &tmp))) {
        mMaybeCloseWindowHelper->SetShouldCloseWindow(tmp);
      }
    }
  }

  // Now get the URI
  if (aChannel) {
    aChannel->GetURI(getter_AddRefs(mSourceUrl));
  }

  // retarget all load notifications to our docloader instead of the original
  // window's docloader...
  RetargetLoadNotifications(request);

  // Close the underlying DOMWindow if it was opened specifically for the
  // download. We don't run this in the content process, since we have
  // an instance running in the parent as well, which will handle this
  // if needed.
  if (!XRE_IsContentProcess() && mMaybeCloseWindowHelper) {
    mBrowsingContext = mMaybeCloseWindowHelper->MaybeCloseWindow();
  }

  // In an IPC setting, we're allowing the child process, here, to make
  // decisions about decoding the channel (e.g. decompression).  It will
  // still forward the decoded (uncompressed) data back to the parent.
  // Con: Uncompressed data means more IPC overhead.
  // Pros: ExternalHelperAppParent doesn't need to implement nsIEncodedChannel.
  //       Parent process doesn't need to expect CPU time on decompression.
  MaybeApplyDecodingForExtension(aChannel);

  // At this point, the child process has done everything it can usefully do
  // for OnStartRequest.
  if (XRE_IsContentProcess()) {
    return NS_OK;
  }

  rv = SetUpTempFile(aChannel);
  if (NS_FAILED(rv)) {
    nsresult transferError = rv;

    rv = CreateFailedTransfer(aChannel && NS_UsePrivateBrowsing(aChannel));
    if (NS_FAILED(rv)) {
      LOG(
          ("Failed to create transfer to report failure."
           "Will fallback to prompter!"));
    }

    mCanceled = true;
    request->Cancel(transferError);

    nsAutoString path;
    if (mTempFile) mTempFile->GetPath(path);

    SendStatusChange(kWriteError, transferError, request, path);

    return NS_OK;
  }

  // Inform channel it is open on behalf of a download to throttle it during
  // page loads and prevent its caching.
  nsCOMPtr<nsIHttpChannelInternal> httpInternal = do_QueryInterface(aChannel);
  if (httpInternal) {
    rv = httpInternal->SetChannelIsForDownload(true);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
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
  nsAutoCString MIMEType;
  mMimeInfo->GetMIMEType(MIMEType);
  if (alwaysAsk) {
    // But we *don't* ask if this mimeInfo didn't come from
    // our user configuration datastore and the user has said
    // at some point in the distant past that they don't
    // want to be asked.  The latter fact would have been
    // stored in pref strings back in the old days.

    bool mimeTypeIsInDatastore = false;
    nsCOMPtr<nsIHandlerService> handlerSvc =
        do_GetService(NS_HANDLERSERVICE_CONTRACTID);
    if (handlerSvc) {
      handlerSvc->Exists(mMimeInfo, &mimeTypeIsInDatastore);
    }
    if (!handlerSvc || !mimeTypeIsInDatastore) {
      if (!GetNeverAskFlagFromPref(NEVER_ASK_FOR_SAVE_TO_DISK_PREF,
                                   MIMEType.get())) {
        // Don't need to ask after all.
        alwaysAsk = false;
        // Make sure action matches pref (save to disk).
        mMimeInfo->SetPreferredAction(nsIMIMEInfo::saveToDisk);
      } else if (!GetNeverAskFlagFromPref(NEVER_ASK_FOR_OPEN_FILE_PREF,
                                          MIMEType.get())) {
        // Don't need to ask after all.
        alwaysAsk = false;
      }
    }
  } else if (MIMEType.EqualsLiteral("text/plain")) {
    nsAutoCString ext;
    mMimeInfo->GetPrimaryExtension(ext);
    // If people are sending us ApplicationReputation-eligible files with
    // text/plain mimetypes, enforce asking the user what to do.
    if (!ext.IsEmpty()) {
      nsAutoCString dummyFileName("f");
      if (ext.First() != '.') {
        dummyFileName.Append(".");
      }
      ext.ReplaceChar(KNOWN_PATH_SEPARATORS FILE_ILLEGAL_CHARACTERS, '_');
      nsCOMPtr<nsIApplicationReputationService> appRep =
          components::ApplicationReputation::Service();
      appRep->IsBinary(dummyFileName + ext, &alwaysAsk);
    }
  }

  int32_t action = nsIMIMEInfo::saveToDisk;
  mMimeInfo->GetPreferredAction(&action);

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

  if (alwaysAsk) {
    // Display the dialog
    mDialog = do_CreateInstance(NS_HELPERAPPLAUNCHERDLG_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // this will create a reference cycle (the dialog holds a reference to us as
    // nsIHelperAppLauncher), which will be broken in Cancel or CreateTransfer.
    nsCOMPtr<nsIInterfaceRequestor> dialogParent = GetDialogParent();
    rv = mDialog->Show(this, dialogParent, mReason);

    // what do we do if the dialog failed? I guess we should call Cancel and
    // abort the load....
  } else {
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
        if (NS_FAILED(rv) ||
            isExecutable) {  // checking NS_FAILED, because paranoia is good
          action = nsIMIMEInfo::saveToDisk;
        }
      } else {  // Paranoia is good here too, though this really should not
                // happen
        NS_WARNING(
            "GetDownloadInfo returned a null file after the temp file has been "
            "set up! ");
        action = nsIMIMEInfo::saveToDisk;
      }
    }

#endif
    if (action == nsIMIMEInfo::useHelperApp ||
        action == nsIMIMEInfo::useSystemDefault) {
      rv = LaunchWithApplication(nullptr, false);
    } else {
      rv = SaveToDisk(nullptr, false);
    }
  }

  return NS_OK;
}

// Convert error info into proper message text and send OnStatusChange
// notification to the dialog progress listener or nsITransfer implementation.
void nsExternalAppHandler::SendStatusChange(ErrorType type, nsresult rv,
                                            nsIRequest* aRequest,
                                            const nsString& path) {
  const char* msgId = nullptr;
  switch (rv) {
    case NS_ERROR_OUT_OF_MEMORY:
      // No memory
      msgId = "noMemory";
      break;

    case NS_ERROR_FILE_DISK_FULL:
    case NS_ERROR_FILE_NO_DEVICE_SPACE:
      // Out of space on target volume.
      msgId = "diskFull";
      break;

    case NS_ERROR_FILE_READ_ONLY:
      // Attempt to write to read/only file.
      msgId = "readOnly";
      break;

    case NS_ERROR_FILE_ACCESS_DENIED:
      if (type == kWriteError) {
        // Attempt to write without sufficient permissions.
#if defined(ANDROID)
        // On Android this means the SD card is present but
        // unavailable (read-only).
        msgId = "SDAccessErrorCardReadOnly";
#else
        msgId = "accessError";
#endif
      } else {
        msgId = "launchError";
      }
      break;

    case NS_ERROR_FILE_NOT_FOUND:
    case NS_ERROR_FILE_TARGET_DOES_NOT_EXIST:
    case NS_ERROR_FILE_UNRECOGNIZED_PATH:
      // Helper app not found, let's verify this happened on launch
      if (type == kLaunchError) {
        msgId = "helperAppNotFound";
        break;
      }
#if defined(ANDROID)
      else if (type == kWriteError) {
        // On Android this means the SD card is missing (not in
        // SD slot).
        msgId = "SDAccessErrorCardMissing";
        break;
      }
#endif
      [[fallthrough]];

    default:
      // Generic read/write/launch error message.
      switch (type) {
        case kReadError:
          msgId = "readError";
          break;
        case kWriteError:
          msgId = "writeError";
          break;
        case kLaunchError:
          msgId = "launchError";
          break;
      }
      break;
  }

  MOZ_LOG(
      nsExternalHelperAppService::mLog, LogLevel::Error,
      ("Error: %s, type=%i, listener=0x%p, transfer=0x%p, rv=0x%08" PRIX32 "\n",
       msgId, type, mDialogProgressListener.get(), mTransfer.get(),
       static_cast<uint32_t>(rv)));

  MOZ_LOG(nsExternalHelperAppService::mLog, LogLevel::Error,
          ("       path='%s'\n", NS_ConvertUTF16toUTF8(path).get()));

  // Get properties file bundle and extract status string.
  nsCOMPtr<nsIStringBundleService> stringService =
      mozilla::services::GetStringBundleService();
  if (stringService) {
    nsCOMPtr<nsIStringBundle> bundle;
    if (NS_SUCCEEDED(stringService->CreateBundle(
            "chrome://global/locale/nsWebBrowserPersist.properties",
            getter_AddRefs(bundle)))) {
      nsAutoString msgText;
      AutoTArray<nsString, 1> strings = {path};
      if (NS_SUCCEEDED(bundle->FormatStringFromName(msgId, strings, msgText))) {
        if (mDialogProgressListener) {
          // We have a listener, let it handle the error.
          mDialogProgressListener->OnStatusChange(
              nullptr, (type == kReadError) ? aRequest : nullptr, rv,
              msgText.get());
        } else if (mTransfer) {
          mTransfer->OnStatusChange(nullptr,
                                    (type == kReadError) ? aRequest : nullptr,
                                    rv, msgText.get());
        } else if (XRE_IsParentProcess()) {
          // We don't have a listener.  Simply show the alert ourselves.
          nsCOMPtr<nsIInterfaceRequestor> dialogParent = GetDialogParent();
          nsresult qiRv;
          nsCOMPtr<nsIPrompt> prompter(do_GetInterface(dialogParent, &qiRv));
          nsAutoString title;
          bundle->FormatStringFromName("title", strings, title);

          MOZ_LOG(
              nsExternalHelperAppService::mLog, LogLevel::Debug,
              ("mBrowsingContext=0x%p, prompter=0x%p, qi rv=0x%08" PRIX32
               ", title='%s', msg='%s'",
               mBrowsingContext.get(), prompter.get(),
               static_cast<uint32_t>(qiRv), NS_ConvertUTF16toUTF8(title).get(),
               NS_ConvertUTF16toUTF8(msgText).get()));

          // If we didn't have a prompter we will try and get a window
          // instead, get it's docshell and use it to alert the user.
          if (!prompter) {
            nsCOMPtr<nsPIDOMWindowOuter> window(do_GetInterface(dialogParent));
            if (!window || !window->GetDocShell()) {
              return;
            }

            prompter = do_GetInterface(window->GetDocShell(), &qiRv);

            MOZ_LOG(nsExternalHelperAppService::mLog, LogLevel::Debug,
                    ("No prompter from mBrowsingContext, using DocShell, "
                     "window=0x%p, docShell=0x%p, "
                     "prompter=0x%p, qi rv=0x%08" PRIX32,
                     window.get(), window->GetDocShell(), prompter.get(),
                     static_cast<uint32_t>(qiRv)));

            // If we still don't have a prompter, there's nothing else we
            // can do so just return.
            if (!prompter) {
              MOZ_LOG(nsExternalHelperAppService::mLog, LogLevel::Error,
                      ("No prompter from DocShell, no way to alert user"));
              return;
            }
          }

          // We should always have a prompter at this point.
          prompter->Alert(title.get(), msgText.get());
        }
      }
    }
  }
}

NS_IMETHODIMP
nsExternalAppHandler::OnDataAvailable(nsIRequest* request,
                                      nsIInputStream* inStr,
                                      uint64_t sourceOffset, uint32_t count) {
  nsresult rv = NS_OK;
  // first, check to see if we've been canceled....
  if (mCanceled || !mSaver) {
    // then go cancel our underlying channel too
    return request->Cancel(NS_BINDING_ABORTED);
  }

  // read the data out of the stream and write it to the temp file.
  if (count > 0) {
    mProgress += count;

    nsCOMPtr<nsIStreamListener> saver = do_QueryInterface(mSaver);
    rv = saver->OnDataAvailable(request, inStr, sourceOffset, count);
    if (NS_SUCCEEDED(rv)) {
      // Send progress notification.
      if (mTransfer) {
        mTransfer->OnProgressChange64(nullptr, request, mProgress,
                                      mContentLength, mProgress,
                                      mContentLength);
      }
    } else {
      // An error occurred, notify listener.
      nsAutoString tempFilePath;
      if (mTempFile) {
        mTempFile->GetPath(tempFilePath);
      }
      SendStatusChange(kReadError, rv, request, tempFilePath);

      // Cancel the download.
      Cancel(rv);
    }
  }
  return rv;
}

NS_IMETHODIMP nsExternalAppHandler::OnStopRequest(nsIRequest* request,
                                                  nsresult aStatus) {
  LOG(
      ("nsExternalAppHandler::OnStopRequest\n"
       "  mCanceled=%d, mTransfer=0x%p, aStatus=0x%08" PRIX32 "\n",
       mCanceled, mTransfer.get(), static_cast<uint32_t>(aStatus)));

  mStopRequestIssued = true;

  // Cancel if the request did not complete successfully.
  if (!mCanceled && NS_FAILED(aStatus)) {
    // Send error notification.
    nsAutoString tempFilePath;
    if (mTempFile) mTempFile->GetPath(tempFilePath);
    SendStatusChange(kReadError, aStatus, request, tempFilePath);

    Cancel(aStatus);
  }

  // first, check to see if we've been canceled....
  if (mCanceled || !mSaver) {
    return NS_OK;
  }

  return mSaver->Finish(NS_OK);
}

NS_IMETHODIMP
nsExternalAppHandler::OnTargetChange(nsIBackgroundFileSaver* aSaver,
                                     nsIFile* aTarget) {
  return NS_OK;
}

NS_IMETHODIMP
nsExternalAppHandler::OnSaveComplete(nsIBackgroundFileSaver* aSaver,
                                     nsresult aStatus) {
  LOG(
      ("nsExternalAppHandler::OnSaveComplete\n"
       "  aSaver=0x%p, aStatus=0x%08" PRIX32 ", mCanceled=%d, mTransfer=0x%p\n",
       aSaver, static_cast<uint32_t>(aStatus), mCanceled, mTransfer.get()));

  if (!mCanceled) {
    // Save the hash and signature information
    (void)mSaver->GetSha256Hash(mHash);
    (void)mSaver->GetSignatureInfo(mSignatureInfo);

    // Free the reference that the saver keeps on us, even if we couldn't get
    // the hash.
    mSaver = nullptr;

    // Save the redirect information.
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(mRequest);
    if (channel) {
      nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
      nsresult rv = NS_OK;
      nsCOMPtr<nsIMutableArray> redirectChain =
          do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      LOG(("nsExternalAppHandler: Got %zu redirects\n",
           loadInfo->RedirectChain().Length()));
      for (nsIRedirectHistoryEntry* entry : loadInfo->RedirectChain()) {
        redirectChain->AppendElement(entry);
      }
      mRedirects = redirectChain;
    }

    if (NS_FAILED(aStatus)) {
      nsAutoString path;
      mTempFile->GetPath(path);

      // It may happen when e10s is enabled that there will be no transfer
      // object available to communicate status as expected by the system.
      // Let's try and create a temporary transfer object to take care of this
      // for us, we'll fall back to using the prompt service if we absolutely
      // have to.
      if (!mTransfer) {
        // We don't care if this fails.
        CreateFailedTransfer(channel && NS_UsePrivateBrowsing(channel));
      }

      SendStatusChange(kWriteError, aStatus, nullptr, path);
      if (!mCanceled) Cancel(aStatus);
      return NS_OK;
    }
  }

  // Notify the transfer object that we are done if the user has chosen an
  // action. If the user hasn't chosen an action, the progress listener
  // (nsITransfer) will be notified in CreateTransfer.
  if (mTransfer) {
    NotifyTransfer(aStatus);
  }

  return NS_OK;
}

void nsExternalAppHandler::NotifyTransfer(nsresult aStatus) {
  MOZ_ASSERT(NS_IsMainThread(), "Must notify on main thread");
  MOZ_ASSERT(mTransfer, "We must have an nsITransfer");

  LOG(("Notifying progress listener"));

  if (NS_SUCCEEDED(aStatus)) {
    (void)mTransfer->SetSha256Hash(mHash);
    (void)mTransfer->SetSignatureInfo(mSignatureInfo);
    (void)mTransfer->SetRedirects(mRedirects);
    (void)mTransfer->OnProgressChange64(
        nullptr, nullptr, mProgress, mContentLength, mProgress, mContentLength);
  }

  (void)mTransfer->OnStateChange(nullptr, nullptr,
                                 nsIWebProgressListener::STATE_STOP |
                                     nsIWebProgressListener::STATE_IS_REQUEST |
                                     nsIWebProgressListener::STATE_IS_NETWORK,
                                 aStatus);

  // This nsITransfer object holds a reference to us (we are its observer), so
  // we need to release the reference to break a reference cycle (and therefore
  // to prevent leaking).  We do this even if the previous calls failed.
  mTransfer = nullptr;
}

NS_IMETHODIMP nsExternalAppHandler::GetMIMEInfo(nsIMIMEInfo** aMIMEInfo) {
  *aMIMEInfo = mMimeInfo;
  NS_ADDREF(*aMIMEInfo);
  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::GetSource(nsIURI** aSourceURI) {
  NS_ENSURE_ARG(aSourceURI);
  *aSourceURI = mSourceUrl;
  NS_IF_ADDREF(*aSourceURI);
  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::GetSuggestedFileName(
    nsAString& aSuggestedFileName) {
  aSuggestedFileName = mSuggestedFileName;
  return NS_OK;
}

nsresult nsExternalAppHandler::CreateTransfer() {
  LOG(("nsExternalAppHandler::CreateTransfer"));

  MOZ_ASSERT(NS_IsMainThread(), "Must create transfer on main thread");
  // We are back from the helper app dialog (where the user chooses to save or
  // open), but we aren't done processing the load. in this case, throw up a
  // progress dialog so the user can see what's going on.
  // Also, release our reference to mDialog. We don't need it anymore, and we
  // need to break the reference cycle.
  mDialog = nullptr;
  if (!mDialogProgressListener) {
    NS_WARNING("The dialog should nullify the dialog progress listener");
  }
  nsresult rv;

  // We must be able to create an nsITransfer object. If not, it doesn't matter
  // much that we can't launch the helper application or save to disk. Work on
  // a local copy rather than mTransfer until we know we succeeded, to make it
  // clearer that this function is re-entrant.
  nsCOMPtr<nsITransfer> transfer =
      do_CreateInstance(NS_TRANSFER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Initialize the download
  nsCOMPtr<nsIURI> target;
  rv = NS_NewFileURI(getter_AddRefs(target), mFinalFileDestination);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(mRequest);

  rv = transfer->Init(mSourceUrl, target, EmptyString(), mMimeInfo,
                      mTimeDownloadStarted, mTempFile, this,
                      channel && NS_UsePrivateBrowsing(channel));
  NS_ENSURE_SUCCESS(rv, rv);

  // If we were cancelled since creating the transfer, just return. It is
  // always ok to return NS_OK if we are cancelled. Callers of this function
  // must call Cancel if CreateTransfer fails, but there's no need to cancel
  // twice.
  if (mCanceled) {
    return NS_OK;
  }
  rv = transfer->OnStateChange(nullptr, mRequest,
                               nsIWebProgressListener::STATE_START |
                                   nsIWebProgressListener::STATE_IS_REQUEST |
                                   nsIWebProgressListener::STATE_IS_NETWORK,
                               NS_OK);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mCanceled) {
    return NS_OK;
  }

  mRequest = nullptr;
  // Finally, save the transfer to mTransfer.
  mTransfer = transfer;
  transfer = nullptr;

  // While we were bringing up the progress dialog, we actually finished
  // processing the url. If that's the case then mStopRequestIssued will be
  // true and OnSaveComplete has been called.
  if (mStopRequestIssued && !mSaver && mTransfer) {
    NotifyTransfer(NS_OK);
  }

  return rv;
}

nsresult nsExternalAppHandler::CreateFailedTransfer(bool aIsPrivateBrowsing) {
  nsresult rv;
  nsCOMPtr<nsITransfer> transfer =
      do_CreateInstance(NS_TRANSFER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we don't have a download directory we're kinda screwed but it's OK
  // we'll still report the error via the prompter.
  nsCOMPtr<nsIFile> pseudoFile;
  rv = GetDownloadDirectory(getter_AddRefs(pseudoFile), true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Append the default suggested filename. If the user restarts the transfer
  // we will re-trigger a filename check anyway to ensure that it is unique.
  rv = pseudoFile->Append(mSuggestedFileName);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> pseudoTarget;
  rv = NS_NewFileURI(getter_AddRefs(pseudoTarget), pseudoFile);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transfer->Init(mSourceUrl, pseudoTarget, EmptyString(), mMimeInfo,
                      mTimeDownloadStarted, nullptr, this, aIsPrivateBrowsing);
  NS_ENSURE_SUCCESS(rv, rv);

  // Our failed transfer is ready.
  mTransfer = std::move(transfer);

  return NS_OK;
}

nsresult nsExternalAppHandler::SaveDestinationAvailable(nsIFile* aFile) {
  if (aFile)
    ContinueSave(aFile);
  else
    Cancel(NS_BINDING_ABORTED);

  return NS_OK;
}

void nsExternalAppHandler::RequestSaveDestination(
    const nsString& aDefaultFile, const nsString& aFileExtension) {
  // Display the dialog
  // XXX Convert to use file picker? No, then embeddors could not do any sort of
  // "AutoDownload" w/o showing a prompt
  nsresult rv = NS_OK;
  if (!mDialog) {
    // Get helper app launcher dialog.
    mDialog = do_CreateInstance(NS_HELPERAPPLAUNCHERDLG_CONTRACTID, &rv);
    if (rv != NS_OK) {
      Cancel(NS_BINDING_ABORTED);
      return;
    }
  }

  // we want to explicitly unescape aDefaultFile b4 passing into the dialog. we
  // can't unescape it because the dialog is implemented by a JS component which
  // doesn't have a window so no unescape routine is defined...

  // Now, be sure to keep |this| alive, and the dialog
  // If we don't do this, users that close the helper app dialog while the file
  // picker is up would cause Cancel() to be called, and the dialog would be
  // released, which would release this object too, which would crash.
  // See Bug 249143
  RefPtr<nsExternalAppHandler> kungFuDeathGrip(this);
  nsCOMPtr<nsIHelperAppLauncherDialog> dlg(mDialog);
  nsCOMPtr<nsIInterfaceRequestor> dialogParent = GetDialogParent();

  rv = dlg->PromptForSaveToFileAsync(this, dialogParent, aDefaultFile.get(),
                                     aFileExtension.get(), mForceSave);
  if (NS_FAILED(rv)) {
    Cancel(NS_BINDING_ABORTED);
  }
}

// SaveToDisk should only be called by the helper app dialog which allows
// the user to say launch with application or save to disk. It doesn't actually
// perform the save, it just prompts for the destination file name.
NS_IMETHODIMP nsExternalAppHandler::SaveToDisk(nsIFile* aNewFileLocation,
                                               bool aRememberThisPreference) {
  if (mCanceled) return NS_OK;

  mMimeInfo->SetPreferredAction(nsIMIMEInfo::saveToDisk);

  if (!aNewFileLocation) {
    if (mSuggestedFileName.IsEmpty())
      RequestSaveDestination(mTempLeafName, mTempFileExtension);
    else {
      nsAutoString fileExt;
      int32_t pos = mSuggestedFileName.RFindChar('.');
      if (pos >= 0)
        mSuggestedFileName.Right(fileExt, mSuggestedFileName.Length() - pos);
      if (fileExt.IsEmpty()) fileExt = mTempFileExtension;

      RequestSaveDestination(mSuggestedFileName, fileExt);
    }
  } else {
    ContinueSave(aNewFileLocation);
  }

  return NS_OK;
}
nsresult nsExternalAppHandler::ContinueSave(nsIFile* aNewFileLocation) {
  if (mCanceled) return NS_OK;

  MOZ_ASSERT(aNewFileLocation, "Must be called with a non-null file");

  nsresult rv = NS_OK;
  nsCOMPtr<nsIFile> fileToUse = aNewFileLocation;
  mFinalFileDestination = fileToUse;

  // Move what we have in the final directory, but append .part
  // to it, to indicate that it's unfinished.  Do not call SetTarget on the
  // saver if we are done (Finish has been called) but OnSaverComplete has not
  // been called.
  if (mFinalFileDestination && mSaver && !mStopRequestIssued) {
    nsCOMPtr<nsIFile> movedFile;
    mFinalFileDestination->Clone(getter_AddRefs(movedFile));
    if (movedFile) {
      // Get the old leaf name and append .part to it
      nsAutoString name;
      mFinalFileDestination->GetLeafName(name);
      name.AppendLiteral(".part");
      movedFile->SetLeafName(name);

      rv = mSaver->SetTarget(movedFile, true);
      if (NS_FAILED(rv)) {
        nsAutoString path;
        mTempFile->GetPath(path);
        SendStatusChange(kWriteError, rv, nullptr, path);
        Cancel(rv);
        return NS_OK;
      }

      mTempFile = movedFile;
    }
  }

  // The helper app dialog has told us what to do and we have a final file
  // destination.
  rv = CreateTransfer();
  // If we fail to create the transfer, Cancel.
  if (NS_FAILED(rv)) {
    Cancel(rv);
    return rv;
  }

  return NS_OK;
}

// LaunchWithApplication should only be called by the helper app dialog which
// allows the user to say launch with application or save to disk. It doesn't
// actually perform launch with application.
NS_IMETHODIMP nsExternalAppHandler::LaunchWithApplication(
    nsIFile* aApplication, bool aRememberThisPreference) {
  if (mCanceled) return NS_OK;

  if (mMimeInfo && aApplication) {
    PlatformLocalHandlerApp_t* handlerApp =
        new PlatformLocalHandlerApp_t(EmptyString(), aApplication);
    mMimeInfo->SetPreferredApplicationHandler(handlerApp);
  }

  // Now check if the file is local, in which case we won't bother with saving
  // it to a temporary directory and just launch it from where it is
  nsCOMPtr<nsIFileURL> fileUrl(do_QueryInterface(mSourceUrl));
  if (fileUrl && mIsFileChannel) {
    Cancel(NS_BINDING_ABORTED);
    nsCOMPtr<nsIFile> file;
    nsresult rv = fileUrl->GetFile(getter_AddRefs(file));

    if (NS_SUCCEEDED(rv)) {
      rv = mMimeInfo->LaunchWithFile(file);
      if (NS_SUCCEEDED(rv)) return NS_OK;
    }
    nsAutoString path;
    if (file) file->GetPath(path);
    // If we get here, an error happened
    SendStatusChange(kLaunchError, rv, nullptr, path);
    return rv;
  }

  // Now that the user has elected to launch the downloaded file with a helper
  // app, we're justified in removing the 'salted' name.  We'll rename to what
  // was specified in mSuggestedFileName after the download is done prior to
  // launching the helper app.  So that any existing file of that name won't be
  // overwritten we call CreateUnique().  Also note that we use the same
  // directory as originally downloaded so the download can be renamed in place
  // later.
  nsCOMPtr<nsIFile> fileToUse;
  (void)GetDownloadDirectory(getter_AddRefs(fileToUse));

  if (mSuggestedFileName.IsEmpty()) {
    // Keep using the leafname of the temp file, since we're just starting a
    // helper
    mSuggestedFileName = mTempLeafName;
  }

#ifdef XP_WIN
  fileToUse->Append(mSuggestedFileName + mTempFileExtension);
#else
  fileToUse->Append(mSuggestedFileName);
#endif

  nsresult rv = fileToUse->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
  if (NS_SUCCEEDED(rv)) {
    mFinalFileDestination = fileToUse;
    // launch the progress window now that the user has picked the desired
    // action.
    rv = CreateTransfer();
    if (NS_FAILED(rv)) {
      Cancel(rv);
    }
  } else {
    // Cancel the download and report an error.  We do not want to end up in
    // a state where it appears that we have a normal download that is
    // pointing to a file that we did not actually create.
    nsAutoString path;
    mTempFile->GetPath(path);
    SendStatusChange(kWriteError, rv, nullptr, path);
    Cancel(rv);
  }
  return rv;
}

NS_IMETHODIMP nsExternalAppHandler::Cancel(nsresult aReason) {
  NS_ENSURE_ARG(NS_FAILED(aReason));

  if (mCanceled) {
    return NS_OK;
  }
  mCanceled = true;

  if (mSaver) {
    // We are still writing to the target file.  Give the saver a chance to
    // close the target file, then notify the transfer object if necessary in
    // the OnSaveComplete callback.
    mSaver->Finish(aReason);
    mSaver = nullptr;
  } else {
    if (mStopRequestIssued && mTempFile) {
      // This branch can only happen when the user cancels the helper app dialog
      // when the request has completed. The temp file has to be removed here,
      // because mSaver has been released at that time with the temp file left.
      (void)mTempFile->Remove(false);
    }

    // Notify the transfer object that the download has been canceled, if the
    // user has already chosen an action and we didn't notify already.
    if (mTransfer) {
      NotifyTransfer(aReason);
    }
  }

  // Break our reference cycle with the helper app dialog (set up in
  // OnStartRequest)
  mDialog = nullptr;

  mRequest = nullptr;

  // Release the listener, to break the reference cycle with it (we are the
  // observer of the listener).
  mDialogProgressListener = nullptr;

  return NS_OK;
}

bool nsExternalAppHandler::GetNeverAskFlagFromPref(const char* prefName,
                                                   const char* aContentType) {
  // Search the obsolete pref strings.
  nsAutoCString prefCString;
  Preferences::GetCString(prefName, prefCString);
  if (prefCString.IsEmpty()) {
    // Default is true, if not found in the pref string.
    return true;
  }

  NS_UnescapeURL(prefCString);
  nsACString::const_iterator start, end;
  prefCString.BeginReading(start);
  prefCString.EndReading(end);
  return !CaseInsensitiveFindInReadable(nsDependentCString(aContentType), start,
                                        end);
}

NS_IMETHODIMP
nsExternalAppHandler::GetName(nsACString& aName) {
  aName.AssignLiteral("nsExternalAppHandler");
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The following section contains our nsIMIMEService implementation and related
// methods.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

// nsIMIMEService methods
NS_IMETHODIMP nsExternalHelperAppService::GetFromTypeAndExtension(
    const nsACString& aMIMEType, const nsACString& aFileExt,
    nsIMIMEInfo** _retval) {
  MOZ_ASSERT(!aMIMEType.IsEmpty() || !aFileExt.IsEmpty(),
             "Give me something to work with");
  LOG(("Getting mimeinfo from type '%s' ext '%s'\n",
       PromiseFlatCString(aMIMEType).get(),
       PromiseFlatCString(aFileExt).get()));

  *_retval = nullptr;

  // OK... we need a type. Get one.
  nsAutoCString typeToUse(aMIMEType);
  if (typeToUse.IsEmpty()) {
    nsresult rv = GetTypeFromExtension(aFileExt, typeToUse);
    if (NS_FAILED(rv)) return NS_ERROR_NOT_AVAILABLE;
  }

  // We promise to only send lower case mime types to the OS
  ToLowerCase(typeToUse);

  // (1) Ask the OS for a mime info
  bool found;
  nsresult rv = GetMIMEInfoFromOS(typeToUse, aFileExt, &found, _retval);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  LOG(("OS gave back 0x%p - found: %i\n", *_retval, found));
  // If we got no mimeinfo, something went wrong. Probably lack of memory.
  if (!*_retval) return NS_ERROR_OUT_OF_MEMORY;

  // (2) Now, let's see if we can find something in our datastore
  // This will not overwrite the OS information that interests us
  // (i.e. default application, default app. description)
  nsCOMPtr<nsIHandlerService> handlerSvc =
      do_GetService(NS_HANDLERSERVICE_CONTRACTID);
  if (handlerSvc) {
    bool hasHandler = false;
    (void)handlerSvc->Exists(*_retval, &hasHandler);
    if (hasHandler) {
      rv = handlerSvc->FillHandlerInfo(*_retval, EmptyCString());
      LOG(("Data source: Via type: retval 0x%08" PRIx32 "\n",
           static_cast<uint32_t>(rv)));
    } else {
      rv = NS_ERROR_NOT_AVAILABLE;
    }

    found = found || NS_SUCCEEDED(rv);

    if (!found || NS_FAILED(rv)) {
      // No type match, try extension match
      if (!aFileExt.IsEmpty()) {
        nsAutoCString overrideType;
        rv = handlerSvc->GetTypeFromExtension(aFileExt, overrideType);
        if (NS_SUCCEEDED(rv) && !overrideType.IsEmpty()) {
          // We can't check handlerSvc->Exists() here, because we have a
          // overideType. That's ok, it just results in some console noise.
          // (If there's no handler for the override type, it throws)
          rv = handlerSvc->FillHandlerInfo(*_retval, overrideType);
          LOG(("Data source: Via ext: retval 0x%08" PRIx32 "\n",
               static_cast<uint32_t>(rv)));
          found = found || NS_SUCCEEDED(rv);
        }
      }
    }
  }

  // (3) No match yet. Ask extras.
  if (!found) {
    rv = NS_ERROR_FAILURE;
    // Getting info for application/octet-stream content-type from extras
    // does not make a sense because this tends to open all octet-streams
    // as Binary file with exe, com or bin extension regardless the real
    // extension.
    if (!typeToUse.Equals(APPLICATION_OCTET_STREAM,
                          nsCaseInsensitiveCStringComparator()))
      rv = FillMIMEInfoForMimeTypeFromExtras(typeToUse, *_retval);
    LOG(("Searched extras (by type), rv 0x%08" PRIX32 "\n",
         static_cast<uint32_t>(rv)));
    // If that didn't work out, try file extension from extras
    if (NS_FAILED(rv) && !aFileExt.IsEmpty()) {
      rv = FillMIMEInfoForExtensionFromExtras(aFileExt, *_retval);
      LOG(("Searched extras (by ext), rv 0x%08" PRIX32 "\n",
           static_cast<uint32_t>(rv)));
    }
    // If that still didn't work, set the file description to "ext File"
    if (NS_FAILED(rv) && !aFileExt.IsEmpty()) {
      // XXXzpao This should probably be localized
      nsAutoCString desc(aFileExt);
      desc.AppendLiteral(" File");
      (*_retval)->SetDescription(NS_ConvertASCIItoUTF16(desc));
      LOG(("Falling back to 'File' file description\n"));
    }
  }

  // Finally, check if we got a file extension and if yes, if it is an
  // extension on the mimeinfo, in which case we want it to be the primary one
  if (!aFileExt.IsEmpty()) {
    bool matches = false;
    (*_retval)->ExtensionExists(aFileExt, &matches);
    LOG(("Extension '%s' matches mime info: %i\n",
         PromiseFlatCString(aFileExt).get(), matches));
    if (matches) (*_retval)->SetPrimaryExtension(aFileExt);
  }

  if (LOG_ENABLED()) {
    nsAutoCString type;
    (*_retval)->GetMIMEType(type);

    nsAutoCString ext;
    (*_retval)->GetPrimaryExtension(ext);
    LOG(("MIME Info Summary: Type '%s', Primary Ext '%s'\n", type.get(),
         ext.get()));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsExternalHelperAppService::GetTypeFromExtension(const nsACString& aFileExt,
                                                 nsACString& aContentType) {
  // OK. We want to try the following sources of mimetype information, in this
  // order:
  // 1. defaultMimeEntries array
  // 2. OS-provided information
  // 3. our "extras" array
  // 4. Information from plugins
  // 5. The "ext-to-type-mapping" category
  // Note that, we are intentionally not looking at the handler service, because
  // that can be affected by websites, which leads to undesired behavior.

  // Early return if called with an empty extension parameter
  if (aFileExt.IsEmpty()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // First of all, check our default entries
  for (auto& entry : defaultMimeEntries) {
    if (aFileExt.LowerCaseEqualsASCII(entry.mFileExtension)) {
      aContentType = entry.mMimeType;
      return NS_OK;
    }
  }

  // Ask OS.
  if (GetMIMETypeFromOSForExtension(aFileExt, aContentType)) {
    return NS_OK;
  }

  // Check extras array.
  bool found = GetTypeFromExtras(aFileExt, aContentType);
  if (found) {
    return NS_OK;
  }

  // Try the plugins
  RefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();
  if (pluginHost &&
      pluginHost->HavePluginForExtension(aFileExt, aContentType)) {
    return NS_OK;
  }

  // Let's see if an extension added something
  nsCOMPtr<nsICategoryManager> catMan(
      do_GetService("@mozilla.org/categorymanager;1"));
  if (catMan) {
    // The extension in the category entry is always stored as lowercase
    nsAutoCString lowercaseFileExt(aFileExt);
    ToLowerCase(lowercaseFileExt);
    // Read the MIME type from the category entry, if available
    nsCString type;
    nsresult rv =
        catMan->GetCategoryEntry("ext-to-type-mapping", lowercaseFileExt, type);
    if (NS_SUCCEEDED(rv)) {
      aContentType = type;
      return NS_OK;
    }
  }

  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP nsExternalHelperAppService::GetPrimaryExtension(
    const nsACString& aMIMEType, const nsACString& aFileExt,
    nsACString& _retval) {
  NS_ENSURE_ARG(!aMIMEType.IsEmpty());

  nsCOMPtr<nsIMIMEInfo> mi;
  nsresult rv =
      GetFromTypeAndExtension(aMIMEType, aFileExt, getter_AddRefs(mi));
  if (NS_FAILED(rv)) return rv;

  return mi->GetPrimaryExtension(_retval);
}

NS_IMETHODIMP nsExternalHelperAppService::GetTypeFromURI(
    nsIURI* aURI, nsACString& aContentType) {
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
    nsAutoCString ext;
    rv = url->GetFileExtension(ext);
    if (NS_FAILED(rv)) return rv;
    if (ext.IsEmpty()) return NS_ERROR_NOT_AVAILABLE;

    UnescapeFragment(ext, url, ext);

    return GetTypeFromExtension(ext, aContentType);
  }

  // no url, let's give the raw spec a shot
  nsAutoCString specStr;
  rv = aURI->GetSpec(specStr);
  if (NS_FAILED(rv)) return rv;
  UnescapeFragment(specStr, aURI, specStr);

  // find the file extension (if any)
  int32_t extLoc = specStr.RFindChar('.');
  int32_t specLength = specStr.Length();
  if (-1 != extLoc && extLoc != specLength - 1 &&
      // nothing over 20 chars long can be sanely considered an
      // extension.... Dat dere would be just data.
      specLength - extLoc < 20) {
    return GetTypeFromExtension(Substring(specStr, extLoc + 1), aContentType);
  }

  // We found no information; say so.
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP nsExternalHelperAppService::GetTypeFromFile(
    nsIFile* aFile, nsACString& aContentType) {
  NS_ENSURE_ARG_POINTER(aFile);
  nsresult rv;

  // Get the Extension
  nsAutoString fileName;
  rv = aFile->GetLeafName(fileName);
  if (NS_FAILED(rv)) return rv;

  nsAutoCString fileExt;
  if (!fileName.IsEmpty()) {
    int32_t len = fileName.Length();
    for (int32_t i = len; i >= 0; i--) {
      if (fileName[i] == char16_t('.')) {
        CopyUTF16toUTF8(Substring(fileName, i + 1), fileExt);
        break;
      }
    }
  }

  if (fileExt.IsEmpty()) return NS_ERROR_FAILURE;

  return GetTypeFromExtension(fileExt, aContentType);
}

nsresult nsExternalHelperAppService::FillMIMEInfoForMimeTypeFromExtras(
    const nsACString& aContentType, nsIMIMEInfo* aMIMEInfo) {
  NS_ENSURE_ARG(aMIMEInfo);

  NS_ENSURE_ARG(!aContentType.IsEmpty());

  // Look for default entry with matching mime type.
  nsAutoCString MIMEType(aContentType);
  ToLowerCase(MIMEType);
  int32_t numEntries = ArrayLength(extraMimeEntries);
  for (int32_t index = 0; index < numEntries; index++) {
    if (MIMEType.Equals(extraMimeEntries[index].mMimeType)) {
      // This is the one. Set attributes appropriately.
      aMIMEInfo->SetFileExtensions(
          nsDependentCString(extraMimeEntries[index].mFileExtensions));
      aMIMEInfo->SetDescription(
          NS_ConvertASCIItoUTF16(extraMimeEntries[index].mDescription));
      return NS_OK;
    }
  }

  return NS_ERROR_NOT_AVAILABLE;
}

nsresult nsExternalHelperAppService::FillMIMEInfoForExtensionFromExtras(
    const nsACString& aExtension, nsIMIMEInfo* aMIMEInfo) {
  nsAutoCString type;
  bool found = GetTypeFromExtras(aExtension, type);
  if (!found) return NS_ERROR_NOT_AVAILABLE;
  return FillMIMEInfoForMimeTypeFromExtras(type, aMIMEInfo);
}

bool nsExternalHelperAppService::GetTypeFromExtras(const nsACString& aExtension,
                                                   nsACString& aMIMEType) {
  NS_ASSERTION(!aExtension.IsEmpty(), "Empty aExtension parameter!");

  // Look for default entry with matching extension.
  nsDependentCString::const_iterator start, end, iter;
  int32_t numEntries = ArrayLength(extraMimeEntries);
  for (int32_t index = 0; index < numEntries; index++) {
    nsDependentCString extList(extraMimeEntries[index].mFileExtensions);
    extList.BeginReading(start);
    extList.EndReading(end);
    iter = start;
    while (start != end) {
      FindCharInReadable(',', iter, end);
      if (Substring(start, iter)
              .Equals(aExtension, nsCaseInsensitiveCStringComparator())) {
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

bool nsExternalHelperAppService::GetMIMETypeFromOSForExtension(
    const nsACString& aExtension, nsACString& aMIMEType) {
  bool found = false;
  nsCOMPtr<nsIMIMEInfo> mimeInfo;
  nsresult rv = GetMIMEInfoFromOS(EmptyCString(), aExtension, &found,
                                  getter_AddRefs(mimeInfo));
  return NS_SUCCEEDED(rv) && found && mimeInfo &&
         NS_SUCCEEDED(mimeInfo->GetMIMEType(aMIMEType));
}

nsresult nsExternalHelperAppService::GetMIMEInfoFromOS(
    const nsACString& aMIMEType, const nsACString& aFileExt, bool* aFound,
    nsIMIMEInfo** aMIMEInfo) {
  *aMIMEInfo = nullptr;
  *aFound = false;
  return NS_ERROR_NOT_IMPLEMENTED;
}
