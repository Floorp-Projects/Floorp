/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:cin:
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 *   Bill Law <law@netscape.com>
 *   Christian Biesinger <cbiesinger@web.de>
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

#include "nsExternalHelperAppService.h"
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

// used to manage our in memory data source of helper applications
#ifdef MOZ_RDF
#include "nsRDFCID.h"
#include "rdf.h"
#include "nsIRDFService.h"
#include "nsIRDFRemoteDataSource.h"
#endif //MOZ_RDF
#include "nsHelperAppRDF.h"
#include "nsIMIMEInfo.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIRefreshURI.h" // XXX needed to redirect according to Refresh: URI
#include "nsIDocumentLoader.h" // XXX needed to get orig. channel and assoc. refresh uri
#include "nsIHelperAppLauncherDialog.h"
#include "nsNetUtil.h"
#include "nsIIOService.h"
#include "nsNetCID.h"
#include "nsChannelProperties.h"

#include "nsMimeTypes.h"
// used for header disposition information.
#include "nsIHttpChannel.h"
#include "nsIEncodedChannel.h"
#include "nsIMultiPartChannel.h"
#include "nsIFileChannel.h"
#include "nsIObserverService.h" // so we can be a profile change observer
#include "nsIPropertyBag2.h" // for the 64-bit content length

#ifdef XP_MACOSX
#include "nsILocalFileMac.h"
#include "nsIInternetConfigService.h"
#include "nsIAppleFileDecoder.h"
#elif defined(XP_OS2)
#include "nsILocalFileOS2.h"
#endif

#include "nsIPluginHost.h" // XXX needed for ext->type mapping (bug 233289)
#include "nsEscape.h"

#include "nsIStringBundle.h" // XXX needed to localize error msgs
#include "nsIPrompt.h"

#include "nsITextToSubURI.h" // to unescape the filename
#include "nsIMIMEHeaderParam.h"

#include "nsIPrefService.h"
#include "nsIWindowWatcher.h"

#include "nsIGlobalHistory.h" // to mark downloads as visited
#include "nsIGlobalHistory2.h" // to mark downloads as visited

#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDocShell.h"

#include "nsCRT.h"

#ifdef PR_LOGGING
PRLogModuleInfo* nsExternalHelperAppService::mLog = nsnull;
#endif

// Using level 3 here because the OSHelperAppServices use a log level
// of PR_LOG_DEBUG (4), and we want less detailed output here
// Using 3 instead of PR_LOG_WARN because we don't output warnings
#define LOG(args) PR_LOG(mLog, 3, args)
#define LOG_ENABLED() PR_LOG_TEST(mLog, 3)

static const char NEVER_ASK_PREF_BRANCH[] = "browser.helperApps.neverAsk.";
static const char NEVER_ASK_FOR_SAVE_TO_DISK_PREF[] = "saveToDisk";
static const char NEVER_ASK_FOR_OPEN_FILE_PREF[]    = "openFile";

#ifdef MOZ_RDF
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
#endif
static NS_DEFINE_CID(kPluginManagerCID, NS_PLUGINMANAGER_CID);

/**
 * Contains a pointer to the helper app service, set in its constructor
 */
static nsExternalHelperAppService* sSrv;

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

/** Gets the content-disposition header from a channel, using nsIHttpChannel
 * or nsIMultipartChannel if available
 * @param aChannel The channel to extract the disposition header from
 * @param aDisposition Reference to a string where the header is to be stored
 */
static void ExtractDisposition(nsIChannel* aChannel, nsACString& aDisposition)
{
  aDisposition.Truncate();
  // First see whether this is an http channel
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));
  if (httpChannel) 
  {
    httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("content-disposition"), aDisposition);
  }
  if (aDisposition.IsEmpty())
  {
    nsCOMPtr<nsIMultiPartChannel> multipartChannel(do_QueryInterface(aChannel));
    if (multipartChannel)
    {
      multipartChannel->GetContentDisposition(aDisposition);
    }
  }

}

/** Extracts the filename out of a content-disposition header
 * @param aFilename [out] The filename. Can be empty on error.
 * @param aDisposition Value of a Content-Disposition header
 * @param aURI Optional. Will be used to get a fallback charset for the
 *        filename, if it is QI'able to nsIURL
 * @param aMIMEHeaderParam Optional. Pointer to a nsIMIMEHeaderParam class, so
 *        that it doesn't need to be fetched by this function.
 */
static void GetFilenameFromDisposition(nsAString& aFilename,
                                       const nsACString& aDisposition,
                                       nsIURI* aURI = nsnull,
                                       nsIMIMEHeaderParam* aMIMEHeaderParam = nsnull)
{
  aFilename.Truncate();
  nsCOMPtr<nsIMIMEHeaderParam> mimehdrpar(aMIMEHeaderParam);
  if (!mimehdrpar) {
    mimehdrpar = do_GetService(NS_MIMEHEADERPARAM_CONTRACTID);
    if (!mimehdrpar)
      return;
  }

  nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);

  nsCAutoString fallbackCharset;
  if (url)
    url->GetOriginCharset(fallbackCharset);
  // Get the value of 'filename' parameter
  nsresult rv = mimehdrpar->GetParameter(aDisposition, "filename", fallbackCharset, 
                                         PR_TRUE, nsnull, aFilename);
  if (NS_FAILED(rv) || aFilename.IsEmpty())
    // Try 'name' parameter, instead.
    rv = mimehdrpar->GetParameter(aDisposition, "name", fallbackCharset, PR_TRUE, 
                                  nsnull, aFilename);
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
static PRBool GetFilenameAndExtensionFromChannel(nsIChannel* aChannel,
                                                 nsString& aFileName,
                                                 nsCString& aExtension,
                                                 PRBool aAllowURLExtension = PR_TRUE)
{
  aExtension.Truncate();
  /*
   * If the channel is an http or part of a multipart channel and we
   * have a content disposition header set, then use the file name
   * suggested there as the preferred file name to SUGGEST to the
   * user.  we shouldn't actually use that without their
   * permission... otherwise just use our temp file
   */
  nsCAutoString disp;
  ExtractDisposition(aChannel, disp);
  PRBool handleExternally = PR_FALSE;
  nsCOMPtr<nsIURI> uri;
  nsresult rv;
  aChannel->GetURI(getter_AddRefs(uri));
  // content-disposition: has format:
  // disposition-type < ; name=value >* < ; filename=value > < ; name=value >*
  if (!disp.IsEmpty()) 
  {
    nsCOMPtr<nsIMIMEHeaderParam> mimehdrpar = do_GetService(NS_MIMEHEADERPARAM_CONTRACTID, &rv);
    if (NS_FAILED(rv))
      return PR_FALSE;

    nsCAutoString fallbackCharset;
    uri->GetOriginCharset(fallbackCharset);
    // Get the disposition type
    nsAutoString dispToken;
    rv = mimehdrpar->GetParameter(disp, "", fallbackCharset, PR_TRUE, 
                                  nsnull, dispToken);
    // RFC 2183, section 2.8 says that an unknown disposition
    // value should be treated as "attachment"
    // XXXbz this code is duplicated in nsDocumentOpenInfo::DispatchContent.
    // Factor it out!  Maybe store it in the nsDocumentOpenInfo?
    if (NS_FAILED(rv) || 
        (// Some broken sites just send
         // Content-Disposition: ; filename="file"
         // screen those out here.
         !dispToken.IsEmpty() &&
         !dispToken.LowerCaseEqualsLiteral("inline") &&
        // Broken sites just send
        // Content-Disposition: filename="file"
        // without a disposition token... screen those out.
        !dispToken.EqualsIgnoreCase("filename", 8)) &&
        // Also in use is Content-Disposition: name="file"
        !dispToken.EqualsIgnoreCase("name", 4)) 
    {
      // We have a content-disposition of "attachment" or unknown
      handleExternally = PR_TRUE;
    }

    // We may not have a disposition type listed; some servers suck.
    // But they could have listed a filename anyway.
    GetFilenameFromDisposition(aFileName, disp, uri, mimehdrpar);

  } // we had a disp header 

  // If the disposition header didn't work, try the filename from nsIURL
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
      aExtension.Trim(".", PR_FALSE);
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
      aFileName.Trim(".", PR_FALSE);

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
  { "application/xhtml+xml", "xht" }
};

/**
 * This is a small private struct used to help us initialize some
 * default mime types.
 */
struct nsExtraMimeTypeEntry {
  const char* mMimeType; 
  const char* mFileExtensions;
  const char* mDescription;
  PRUint32 mMactype;
  PRUint32 mMacCreator;
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
  { APPLICATION_OCTET_STREAM, "exe,com,bin,sav,bck,pcsi,dcx_axpexe,dcx_vaxexe,sfx_axpexe,sfx_vaxexe", "Binary File", 0, 0 },
#elif defined(XP_MACOSX) // don't define .bin on the mac...use internet config to look that up...
  { APPLICATION_OCTET_STREAM, "exe,com", "Binary File", 0, 0 },
#else
  { APPLICATION_OCTET_STREAM, "exe,com,bin", "Binary File", 0, 0 },
#endif
  { APPLICATION_GZIP2, "gz", "gzip", 0, 0 },
  { "application/x-arj", "arj", "ARJ file", 0,0 },
  { APPLICATION_XPINSTALL, "xpi", "XPInstall Install", MAC_TYPE('xpi*'), MAC_TYPE('MOSS') },
  { APPLICATION_POSTSCRIPT, "ps,eps,ai", "Postscript File", 0, 0 },
  { APPLICATION_JAVASCRIPT, "js", "Javascript Source File", MAC_TYPE('TEXT'), MAC_TYPE('ttxt') },
  { IMAGE_ART, "art", "ART Image", 0, 0 },
  { IMAGE_BMP, "bmp", "BMP Image", 0, 0 },
  { IMAGE_GIF, "gif", "GIF Image", 0,0 },
  { IMAGE_ICO, "ico,cur", "ICO Image", 0, 0 },
  { IMAGE_JPG, "jpeg,jpg,jfif,pjpeg,pjp", "JPEG Image", 0, 0 },
  { IMAGE_PNG, "png", "PNG Image", 0, 0 },
  { IMAGE_TIFF, "tiff,tif", "TIFF Image", 0, 0 },
  { IMAGE_XBM, "xbm", "XBM Image", 0, 0 },
  { "image/svg+xml", "svg", "Scalable Vector Graphics", MAC_TYPE('svg '), MAC_TYPE('ttxt') },
  { MESSAGE_RFC822, "eml", "RFC-822 data", MAC_TYPE('TEXT'), MAC_TYPE('MOSS') },
  { TEXT_PLAIN, "txt,text", "Text File", MAC_TYPE('TEXT'), MAC_TYPE('ttxt') },
  { TEXT_HTML, "html,htm,shtml,ehtml", "HyperText Markup Language", MAC_TYPE('TEXT'), MAC_TYPE('MOSS') },
  { "application/xhtml+xml", "xhtml,xht", "Extensible HyperText Markup Language", MAC_TYPE('TEXT'), MAC_TYPE('ttxt') },
  { APPLICATION_RDF, "rdf", "Resource Description Framework", MAC_TYPE('TEXT'), MAC_TYPE('ttxt') },
  { TEXT_XUL, "xul", "XML-Based User Interface Language", MAC_TYPE('TEXT'), MAC_TYPE('ttxt') },
  { TEXT_XML, "xml,xsl,xbl", "Extensible Markup Language", MAC_TYPE('TEXT'), MAC_TYPE('ttxt') },
  { TEXT_CSS, "css", "Style Sheet", MAC_TYPE('TEXT'), MAC_TYPE('ttxt') },
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
: mDataSourceInitialized(PR_FALSE)
{
  sSrv = this;
}
nsresult nsExternalHelperAppService::Init()
{
  // Add an observer for profile change
  nsresult rv = NS_OK;
  nsCOMPtr<nsIObserverService> obs = do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef PR_LOGGING
  if (!mLog) {
    mLog = PR_NewLogModule("HelperAppService");
    if (!mLog)
      return NS_ERROR_OUT_OF_MEMORY;
  }
#endif

  return obs->AddObserver(this, "profile-before-change", PR_TRUE);
}

nsExternalHelperAppService::~nsExternalHelperAppService()
{
  sSrv = nsnull;
}

nsresult nsExternalHelperAppService::InitDataSource()
{
#ifdef MOZ_RDF
  nsresult rv = NS_OK;

  // don't re-initialize the data source if we've already done so...
  if (mDataSourceInitialized)
    return NS_OK;

  nsCOMPtr<nsIRDFService> rdf = do_GetService(kRDFServiceCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get URI of the mimeTypes.rdf data source.  Do this the same way it's done in
  // pref-applications-edit.xul, for example, to ensure we get the same data source!
  // Note that the way it was done previously (using nsIFileSpec) worked better, but it
  // produced a different uri (it had "file:///C|" instead of "file:///C:".  What can you do?
  nsCOMPtr<nsIFile> mimeTypesFile;
  rv = NS_GetSpecialDirectory(NS_APP_USER_MIMETYPES_50_FILE, getter_AddRefs(mimeTypesFile));
  NS_ENSURE_SUCCESS(rv, rv);
  
  // Get file url spec to be used to initialize the DS.
  nsCAutoString urlSpec;
  rv = NS_GetURLSpecFromFile(mimeTypesFile, urlSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the data source; if it is going to be created, then load is synchronous.
  rv = rdf->GetDataSourceBlocking( urlSpec.get(), getter_AddRefs( mOverRideDataSource ) );
  NS_ENSURE_SUCCESS(rv, rv);

  // initialize our resources if we haven't done so already...
  if (!kNC_Description)
  {
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_DESCRIPTION),
                     getter_AddRefs(kNC_Description));
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_VALUE),
                     getter_AddRefs(kNC_Value));
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_FILEEXTENSIONS),
                     getter_AddRefs(kNC_FileExtensions));
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_PATH),
                     getter_AddRefs(kNC_Path));
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_SAVETODISK),
                     getter_AddRefs(kNC_SaveToDisk));
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_USESYSTEMDEFAULT),
                     getter_AddRefs(kNC_UseSystemDefault));
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_HANDLEINTERNAL),
                     getter_AddRefs(kNC_HandleInternal));
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_ALWAYSASK),
                     getter_AddRefs(kNC_AlwaysAsk));  
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_PRETTYNAME),
                     getter_AddRefs(kNC_PrettyName));  
  }
  
  mDataSourceInitialized = PR_TRUE;

  return rv;
#else
  return NS_ERROR_NOT_AVAILABLE;
#endif
}

NS_IMETHODIMP nsExternalHelperAppService::DoContent(const nsACString& aMimeContentType,
                                                    nsIRequest *aRequest,
                                                    nsIInterfaceRequestor *aWindowContext,
                                                    nsIStreamListener ** aStreamListener)
{
  nsAutoString fileName;
  nsCAutoString fileExtension;
  PRUint32 reason = nsIHelperAppLauncherDialog::REASON_CANTHANDLE;
  nsresult rv;

  // Get the file extension and name that we will need later
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  if (channel) {
    // Check if we have a POST request, in which case we don't want to use
    // the url's extension
    PRBool allowURLExt = PR_TRUE;
    nsCOMPtr<nsIHttpChannel> httpChan = do_QueryInterface(channel);
    if (httpChan) {
      nsCAutoString requestMethod;
      httpChan->GetRequestMethod(requestMethod);
      allowURLExt = !requestMethod.Equals("POST");
    }

    nsCOMPtr<nsIURI> uri;
    channel->GetURI(getter_AddRefs(uri));

    // Check if we had a query string - we don't want to check the URL
    // extension if a query is present in the URI
    // If we already know we don't want to check the URL extension, don't
    // bother checking the query
    if (uri && allowURLExt) {
      nsCOMPtr<nsIURL> url = do_QueryInterface(uri);

      if (url) {
        nsCAutoString query;

        // We only care about the query for HTTP and HTTPS URLs
        PRBool isHTTP, isHTTPS;
        rv = uri->SchemeIs("http", &isHTTP);
        if (NS_FAILED(rv))
          isHTTP = PR_FALSE;
        rv = uri->SchemeIs("https", &isHTTPS);
        if (NS_FAILED(rv))
          isHTTPS = PR_FALSE;

        if (isHTTP || isHTTPS)
          url->GetQuery(query);

        // Only get the extension if the query is empty; if it isn't, then the
        // extension likely belongs to a cgi script and isn't helpful
        allowURLExt = query.IsEmpty();
      }
    }
    // Extract name & extension
    PRBool isAttachment = GetFilenameAndExtensionFromChannel(channel, fileName,
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

  // Try to find a mime object by looking at the mime type/extension
  nsCOMPtr<nsIMIMEInfo> mimeInfo;
  if (aMimeContentType.Equals(APPLICATION_GUESS_FROM_EXT, nsCaseInsensitiveCStringComparator())) {
    nsCAutoString mimeType;
    if (!fileExtension.IsEmpty()) {
      GetFromTypeAndExtension(EmptyCString(), fileExtension, getter_AddRefs(mimeInfo));
      if (mimeInfo) {
        mimeInfo->GetMIMEType(mimeType);

        LOG(("OS-Provided mime type '%s' for extension '%s'\n", 
             mimeType.get(), fileExtension.get()));
      }
    }

    if (fileExtension.IsEmpty() || mimeType.IsEmpty()) {
      // Extension lookup gave us no useful match
      GetFromTypeAndExtension(NS_LITERAL_CSTRING(APPLICATION_OCTET_STREAM), fileExtension,
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
    GetFromTypeAndExtension(aMimeContentType, fileExtension,
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
                                                            fileName,
                                                            reason);
  if (!handler)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aStreamListener = handler);
  
  return NS_OK;
}

NS_IMETHODIMP nsExternalHelperAppService::ApplyDecodingForExtension(const nsACString& aExtension,
                                                                    const nsACString& aEncodingType,
                                                                    PRBool *aApplyDecoding)
{
  *aApplyDecoding = PR_TRUE;
  PRUint32 i;
  for(i = 0; i < NS_ARRAY_LENGTH(nonDecodableExtensions); ++i) {
    if (aExtension.LowerCaseEqualsASCII(nonDecodableExtensions[i].mFileExtension) &&
        aEncodingType.LowerCaseEqualsASCII(nonDecodableExtensions[i].mMimeType)) {
      *aApplyDecoding = PR_FALSE;
      break;
    }
  }
  return NS_OK;
}

#ifdef MOZ_RDF
nsresult nsExternalHelperAppService::FillTopLevelProperties(nsIRDFResource * aContentTypeNodeResource, 
                                                            nsIRDFService * aRDFService, nsIMIMEInfo * aMIMEInfo)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFNode> target;
  nsCOMPtr<nsIRDFLiteral> literal;
  const PRUnichar * stringValue;
  
  rv = InitDataSource();
  if (NS_FAILED(rv)) return NS_OK;

  // set the pretty name description, if nonempty
  FillLiteralValueFromTarget(aContentTypeNodeResource,kNC_Description, &stringValue);
  if (stringValue && *stringValue)
    aMIMEInfo->SetDescription(nsDependentString(stringValue));

  // now iterate over all the file type extensions...
  nsCOMPtr<nsISimpleEnumerator> fileExtensions;
  mOverRideDataSource->GetTargets(aContentTypeNodeResource, kNC_FileExtensions, PR_TRUE, getter_AddRefs(fileExtensions));

  PRBool hasMoreElements = PR_FALSE;
  nsCAutoString fileExtension; 
  nsCOMPtr<nsISupports> element;

  if (fileExtensions)
  {
    fileExtensions->HasMoreElements(&hasMoreElements);
    while (hasMoreElements)
    { 
      fileExtensions->GetNext(getter_AddRefs(element));
      if (element)
      {
        literal = do_QueryInterface(element);
        if (!literal) return NS_ERROR_FAILURE;

        literal->GetValueConst(&stringValue);
        CopyUTF16toUTF8(stringValue, fileExtension);
        if (!fileExtension.IsEmpty())
          aMIMEInfo->AppendExtension(fileExtension);
      }
  
      fileExtensions->HasMoreElements(&hasMoreElements);
    } // while we have more extensions to parse....
  }

  return rv;
}

nsresult nsExternalHelperAppService::FillLiteralValueFromTarget(nsIRDFResource * aSource, nsIRDFResource * aProperty, const PRUnichar ** aLiteralValue)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFLiteral> literal;
  nsCOMPtr<nsIRDFNode> target;

  *aLiteralValue = nsnull;
  rv = InitDataSource();
  if (NS_FAILED(rv)) return rv;

  mOverRideDataSource->GetTarget(aSource, aProperty, PR_TRUE, getter_AddRefs(target));
  if (target)
  {
    literal = do_QueryInterface(target);    
    if (!literal)
      return NS_ERROR_FAILURE;
    literal->GetValueConst(aLiteralValue);
  }
  else
    rv = NS_ERROR_FAILURE;

  return rv;
}

nsresult nsExternalHelperAppService::FillContentHandlerProperties(const char * aContentType, 
                                                                  nsIRDFResource * aContentTypeNodeResource, 
                                                                  nsIRDFService * aRDFService, 
                                                                  nsIMIMEInfo * aMIMEInfo)
{
  nsCOMPtr<nsIRDFNode> target;
  nsCOMPtr<nsIRDFLiteral> literal;
  const PRUnichar * stringValue = nsnull;
  nsresult rv = NS_OK;

  rv = InitDataSource();
  if (NS_FAILED(rv)) return rv;

  nsCAutoString contentTypeHandlerNodeName(NC_CONTENT_NODE_HANDLER_PREFIX);
  contentTypeHandlerNodeName.Append(aContentType);

  nsCOMPtr<nsIRDFResource> contentTypeHandlerNodeResource;
  aRDFService->GetResource(contentTypeHandlerNodeName, getter_AddRefs(contentTypeHandlerNodeResource));
  NS_ENSURE_TRUE(contentTypeHandlerNodeResource, NS_ERROR_FAILURE); // that's not good! we have an error in the rdf file

  // now process the application handler information
  aMIMEInfo->SetPreferredAction(nsIMIMEInfo::useHelperApp);

  // save to disk
  FillLiteralValueFromTarget(contentTypeHandlerNodeResource,kNC_SaveToDisk, &stringValue);
  NS_NAMED_LITERAL_STRING(trueString, "true");
  NS_NAMED_LITERAL_STRING(falseString, "false");
  if (stringValue && trueString.Equals(stringValue))
       aMIMEInfo->SetPreferredAction(nsIMIMEInfo::saveToDisk);

  // use system default
  FillLiteralValueFromTarget(contentTypeHandlerNodeResource,kNC_UseSystemDefault, &stringValue);
  if (stringValue && trueString.Equals(stringValue))
      aMIMEInfo->SetPreferredAction(nsIMIMEInfo::useSystemDefault);

  // handle internal
  FillLiteralValueFromTarget(contentTypeHandlerNodeResource,kNC_HandleInternal, &stringValue);
  if (stringValue && trueString.Equals(stringValue))
       aMIMEInfo->SetPreferredAction(nsIMIMEInfo::handleInternally);
  
  // always ask
  FillLiteralValueFromTarget(contentTypeHandlerNodeResource,kNC_AlwaysAsk, &stringValue);
  // Only skip asking if we are absolutely sure the user does not want
  // to be asked.  Any sort of bofus data should mean we ask.
  aMIMEInfo->SetAlwaysAskBeforeHandling(!stringValue ||
                                        !falseString.Equals(stringValue));


  // now digest the external application information

  nsCAutoString externalAppNodeName (NC_CONTENT_NODE_EXTERNALAPP_PREFIX);
  externalAppNodeName.Append(aContentType);
  nsCOMPtr<nsIRDFResource> externalAppNodeResource;
  aRDFService->GetResource(externalAppNodeName, getter_AddRefs(externalAppNodeResource));

  // Clear out any possibly set preferred application, to match the datasource
  aMIMEInfo->SetApplicationDescription(EmptyString());
  aMIMEInfo->SetPreferredApplicationHandler(nsnull);
  if (externalAppNodeResource)
  {
    FillLiteralValueFromTarget(externalAppNodeResource, kNC_PrettyName, &stringValue);
    if (stringValue)
      aMIMEInfo->SetApplicationDescription(nsDependentString(stringValue));
 
    FillLiteralValueFromTarget(externalAppNodeResource, kNC_Path, &stringValue);
    if (stringValue && stringValue[0])
    {
      nsCOMPtr<nsIFile> application;
      GetFileTokenForPath(stringValue, getter_AddRefs(application));
      if (application)
        aMIMEInfo->SetPreferredApplicationHandler(application);
    }
  }

  return rv;
}
#endif /* MOZ_RDF */

PRBool nsExternalHelperAppService::MIMETypeIsInDataSource(const char * aContentType)
{
#ifdef MOZ_RDF
  nsresult rv = InitDataSource();
  if (NS_FAILED(rv)) return PR_FALSE;
  
  if (mOverRideDataSource)
  {
    // Get the RDF service.
    nsCOMPtr<nsIRDFService> rdf = do_GetService(kRDFServiceCID, &rv);
    if (NS_FAILED(rv)) return PR_FALSE;
    
    // Build uri for the mimetype resource.
    nsCAutoString contentTypeNodeName(NC_CONTENT_NODE_PREFIX);
    nsCAutoString contentType(aContentType);
    ToLowerCase(contentType);
    contentTypeNodeName.Append(contentType);
    
    // Get the mime type resource.
    nsCOMPtr<nsIRDFResource> contentTypeNodeResource;
    rv = rdf->GetResource(contentTypeNodeName, getter_AddRefs(contentTypeNodeResource));
    if (NS_FAILED(rv)) return PR_FALSE;
    
    // Test that there's a #value arc from the mimetype resource to the mimetype literal string.
    nsCOMPtr<nsIRDFLiteral> mimeLiteral;
    NS_ConvertUTF8toUTF16 mimeType(contentType);
    rv = rdf->GetLiteral( mimeType.get(), getter_AddRefs( mimeLiteral ) );
    if (NS_FAILED(rv)) return PR_FALSE;
    
    PRBool exists = PR_FALSE;
    rv = mOverRideDataSource->HasAssertion(contentTypeNodeResource, kNC_Value, mimeLiteral, PR_TRUE, &exists );
    
    if (NS_SUCCEEDED(rv) && exists) return PR_TRUE;
  }
#endif
  return PR_FALSE;
}

nsresult nsExternalHelperAppService::GetMIMEInfoForMimeTypeFromDS(const nsACString& aContentType, nsIMIMEInfo * aMIMEInfo)
{
#ifdef MOZ_RDF
  NS_ENSURE_ARG_POINTER(aMIMEInfo);
  nsresult rv = InitDataSource();
  if (NS_FAILED(rv)) return rv;

  // can't do anything if we have no datasource...
  if (!mOverRideDataSource)
    return NS_ERROR_FAILURE;

  // Get the RDF service.
  nsCOMPtr<nsIRDFService> rdf = do_GetService(kRDFServiceCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  
  // Build uri for the mimetype resource.
  nsCAutoString contentTypeNodeName(NC_CONTENT_NODE_PREFIX);
  nsCAutoString contentType(aContentType);
  ToLowerCase(contentType);
  contentTypeNodeName.Append(contentType);

  // Get the mime type resource.
  nsCOMPtr<nsIRDFResource> contentTypeNodeResource;
  rv = rdf->GetResource(contentTypeNodeName, getter_AddRefs(contentTypeNodeResource));
  NS_ENSURE_SUCCESS(rv, rv);

  // we need a way to determine if this content type resource is really in the graph or not...
  // ...Test that there's a #value arc from the mimetype resource to the mimetype literal string.
  nsCOMPtr<nsIRDFLiteral> mimeLiteral;
  NS_ConvertUTF8toUTF16 mimeType(contentType);
  rv = rdf->GetLiteral( mimeType.get(), getter_AddRefs( mimeLiteral ) );
  NS_ENSURE_SUCCESS(rv, rv);
  
  PRBool exists = PR_FALSE;
  rv = mOverRideDataSource->HasAssertion(contentTypeNodeResource, kNC_Value, mimeLiteral, PR_TRUE, &exists );

  if (NS_SUCCEEDED(rv) && exists)
  {
     // fill the mimeinfo in based on the values from the data source
     rv = FillTopLevelProperties(contentTypeNodeResource, rdf, aMIMEInfo);
     NS_ENSURE_SUCCESS(rv, rv);
     rv = FillContentHandlerProperties(contentType.get(), contentTypeNodeResource, rdf, aMIMEInfo);

  } // if we have a node in the graph for this content type
  // If we had success, but entry doesn't exist, we don't want to return success
  else if (NS_SUCCEEDED(rv)) {
    rv = NS_ERROR_NOT_AVAILABLE;
  }

  return rv;
#else
  return NS_ERROR_NOT_AVAILABLE;
#endif /* MOZ_RDF */
}

nsresult nsExternalHelperAppService::GetMIMEInfoForExtensionFromDS(const nsACString& aFileExtension, nsIMIMEInfo * aMIMEInfo)
{
  nsCAutoString type;
  PRBool found = GetTypeFromDS(aFileExtension, type);
  if (!found)
    return NS_ERROR_NOT_AVAILABLE;

  return GetMIMEInfoForMimeTypeFromDS(type, aMIMEInfo);
}

PRBool nsExternalHelperAppService::GetTypeFromDS(const nsACString& aExtension,
                                                 nsACString& aType)
{
#ifdef MOZ_RDF
  nsresult rv = InitDataSource();
  if (NS_FAILED(rv))
    return PR_FALSE;

  // Can't do anything without a datasource
  if (!mOverRideDataSource)
    return PR_FALSE;

  // Get the RDF service.
  nsCOMPtr<nsIRDFService> rdf = do_GetService(kRDFServiceCID, &rv);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  NS_ConvertUTF8toUTF16 extension(aExtension);
  ToLowerCase(extension);
  nsCOMPtr<nsIRDFLiteral> extensionLiteral;
  rv = rdf->GetLiteral(extension.get(), getter_AddRefs(extensionLiteral));
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  nsCOMPtr<nsIRDFResource> contentTypeNodeResource;
  rv = mOverRideDataSource->GetSource(kNC_FileExtensions,
                                      extensionLiteral,
                                      PR_TRUE,
                                      getter_AddRefs(contentTypeNodeResource));
  nsCAutoString contentTypeStr;
  if (NS_SUCCEEDED(rv) && contentTypeNodeResource)
  {
    const PRUnichar* contentType = nsnull;
    rv = FillLiteralValueFromTarget(contentTypeNodeResource, kNC_Value, &contentType);
    if (contentType) {
      LossyCopyUTF16toASCII(contentType, aType);
      return PR_TRUE;
    }
  }  // if we have a node in the graph for this extension
#endif /* MOZ_RDF */
  return PR_FALSE;
}

nsresult nsExternalHelperAppService::GetFileTokenForPath(const PRUnichar * aPlatformAppPath,
                                                         nsIFile ** aFile)
{
  nsDependentString platformAppPath(aPlatformAppPath);
  // First, check if we have an absolute path
  nsILocalFile* localFile = nsnull;
  nsresult rv = NS_NewLocalFile(platformAppPath, PR_TRUE, &localFile);
  if (NS_SUCCEEDED(rv)) {
    *aFile = localFile;
    PRBool exists;
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
      PRBool exists = PR_FALSE;
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
                                                                        PRBool * aHandlerExists)
{
  // this method should only be implemented by each OS specific implementation of this service.
  *aHandlerExists = PR_FALSE;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExternalHelperAppService::IsExposedProtocol(const char * aProtocolScheme, PRBool * aResult)
{
  // by default, no protocol is exposed.  i.e., by default all link clicks must
  // go through the external protocol service.  most applications override this
  // default behavior.
  *aResult = PR_FALSE;

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs)
  {
    PRBool val;
    nsresult rv;

    // check the per protocol setting first.  it always takes precidence.
    // if not set, then use the global setting.

    nsCAutoString name;
    name = NS_LITERAL_CSTRING("network.protocol-handler.expose.")
         + nsDependentCString(aProtocolScheme);
    rv = prefs->GetBoolPref(name.get(), &val);
    if (NS_SUCCEEDED(rv))
    {
      *aResult = val;
    }
    else
    {
      rv = prefs->GetBoolPref("network.protocol-handler.expose-all", &val);
      if (NS_SUCCEEDED(rv) && val)
        *aResult = PR_TRUE;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsExternalHelperAppService::LoadUrl(nsIURI * aURL)
{
  return LoadURI(aURL, nsnull);
}


//  nsExternalHelperAppService::LoadURI() may now pose a confirm dialog
//  that existing callers aren't expecting. We must do it on an event
//  callback to make sure we don't hang someone up.

class nsExternalLoadRequest : public nsRunnable {
  public:
    nsExternalLoadRequest(nsIURI *uri, nsIPrompt *prompt)
      : mURI(uri), mPrompt(prompt) {}

    NS_IMETHOD Run() {
      if (sSrv && sSrv->isExternalLoadOK(mURI, mPrompt))
        sSrv->LoadUriInternal(mURI);
      return NS_OK;
    }

  private:
    nsCOMPtr<nsIURI>    mURI;
    nsCOMPtr<nsIPrompt> mPrompt;
};

NS_IMETHODIMP nsExternalHelperAppService::LoadURI(nsIURI * aURL, nsIPrompt * aPrompt)
{
  nsCOMPtr<nsIRunnable> event = new nsExternalLoadRequest(aURL, aPrompt);
  return NS_DispatchToCurrentThread(event);
}

// helper routines used by LoadURI to check whether we're allowed
// to load external schemes and whether or not to warn the user

static const char kExternalProtocolPrefPrefix[]  = "network.protocol-handler.external.";
static const char kExternalProtocolDefaultPref[] = "network.protocol-handler.external-default";
static const char kExternalWarningPrefPrefix[]   = "network.protocol-handler.warn-external.";
static const char kExternalWarningDefaultPref[]  = "network.protocol-handler.warn-external-default";


PRBool nsExternalHelperAppService::isExternalLoadOK(nsIURI* aURL, nsIPrompt* aPrompt)
{
  if (!aURL)
    return PR_FALSE;

  nsCAutoString scheme;
  aURL->GetScheme(scheme);
  if (scheme.IsEmpty())
    return PR_FALSE; // must have a scheme

  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (!prefs)
    return PR_FALSE; // deny if we can't check prefs


  // Deny load if the prefs say to do so
  nsCAutoString externalPref(kExternalProtocolPrefPrefix);
  externalPref += scheme;
  PRBool allowLoad  = PR_FALSE;
  nsresult rv = prefs->GetBoolPref(externalPref.get(), &allowLoad);
  if (NS_FAILED(rv))
  {
    // no scheme-specific value, check the default
    rv = prefs->GetBoolPref(kExternalProtocolDefaultPref, &allowLoad);
  }
  if (NS_FAILED(rv) || !allowLoad)
    return PR_FALSE; // explicitly denied or missing default pref


  // allowLoad is now true. See whether we have to ask the user
  nsCAutoString warningPref(kExternalWarningPrefPrefix);
  warningPref += scheme;
  PRBool warn = PR_TRUE;
  rv = prefs->GetBoolPref(warningPref.get(), &warn);
  if (NS_FAILED(rv))
  {
    // no scheme-specific value, check the default
    rv = prefs->GetBoolPref(kExternalWarningDefaultPref, &warn);
  }


  if (NS_FAILED(rv) || warn)
  {
    // explicit "warn" setting or missing default pref:
    // we must ask the user before loading this type externally
    PRBool remember = PR_FALSE;
    allowLoad = promptForScheme(aURL, aPrompt, &remember);

    if (remember)
    {
      if (allowLoad)
        // suppress future warnings for this scheme
        prefs->SetBoolPref(warningPref.get(), PR_FALSE);
      else
        // prevent externally loading this scheme in the future
        prefs->SetBoolPref(externalPref.get(), PR_FALSE);
    }
  }

  return allowLoad;
}

PRBool nsExternalHelperAppService::promptForScheme(nsIURI* aURI,
                                                   nsIPrompt* aPrompt,
                                                   PRBool *aRemember)
{
  // if no prompt passed in get one from the windowwatcher
  nsCOMPtr<nsIPrompt> prompt(aPrompt);
  if (!prompt)
  {
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
    if (wwatch)
      wwatch->GetNewPrompter(0, getter_AddRefs(prompt));
  }
  if (!prompt) {
    NS_ERROR("No prompt to warn user about external load, denying");
    return PR_FALSE; // told to warn but no prompt: deny
  }

  // load the strings we need
  nsCOMPtr<nsIStringBundleService> sbSvc(do_GetService(NS_STRINGBUNDLE_CONTRACTID));
  if (!sbSvc) {
    NS_ERROR("Couldn't load StringBundleService");
    return PR_FALSE;
  }

  nsCOMPtr<nsIStringBundle> appstrings;
  nsresult rv = sbSvc->CreateBundle("chrome://global/locale/appstrings.properties",
                                    getter_AddRefs(appstrings));
  if (NS_FAILED(rv) || !appstrings) {
    NS_ERROR("Failed to create appstrings.properties bundle");
    return PR_FALSE;
  }

  nsCAutoString spec;
  aURI->GetSpec(spec);
  NS_ConvertUTF8toUTF16 uri(spec);

  // The maximum amount of space the URI should take up in the dialog.
  const PRUint32 maxWidth = 75;
  const PRUint32 maxLines = 12; // (not counting ellipsis line)
  const PRUint32 maxLength = maxWidth * maxLines;

  // If the URI seems too long, insert zero-width spaces to make it wrappable
  // and crop it in the middle (replacing the cropped portion with an ellipsis),
  // so the dialog doesn't grow so wide or tall it renders buttons off-screen.
  if (uri.Length() > maxWidth) {
    PRUint32 charIdx = maxWidth;
    PRUint32 lineIdx = 1;
    
    PRInt32 numCharsToCrop = uri.Length() - maxLength;

    while (charIdx < uri.Length()) {
      // Don't insert characters in the middle of a surrogate pair.
      if (NS_IS_LOW_SURROGATE(uri[charIdx]))
        --charIdx;

      if (numCharsToCrop > 0 && lineIdx == maxLines / 2) {
        NS_NAMED_LITERAL_STRING(ellipsis, "\n...\n");

        // Don't end the crop in the middle of a surrogate pair.
        if (NS_IS_HIGH_SURROGATE(uri[charIdx + numCharsToCrop - 1]))
          ++numCharsToCrop;

        uri.Replace(charIdx, numCharsToCrop, ellipsis);
        charIdx += ellipsis.Length();
      }
      else {
        // 0x200B is the zero-width breakable space character.
        uri.Insert(PRUnichar(0x200B), charIdx);
        charIdx += 1;
      }
  
      charIdx += maxWidth;
      ++lineIdx;
    }
  }

  nsCAutoString asciischeme;
  aURI->GetScheme(asciischeme);
  NS_ConvertUTF8toUTF16 scheme(asciischeme);

  nsXPIDLString desc;
  GetApplicationDescription(asciischeme, desc);

  nsXPIDLString title;
  appstrings->GetStringFromName(NS_LITERAL_STRING("externalProtocolTitle").get(),
                                getter_Copies(title));
  nsXPIDLString checkMsg;
  appstrings->GetStringFromName(NS_LITERAL_STRING("externalProtocolChkMsg").get(),
                                getter_Copies(checkMsg));
  nsXPIDLString launchBtn;
  appstrings->GetStringFromName(NS_LITERAL_STRING("externalProtocolLaunchBtn").get(),
                                getter_Copies(launchBtn));

  if (desc.IsEmpty())
    appstrings->GetStringFromName(NS_LITERAL_STRING("externalProtocolUnknown").get(),
                                  getter_Copies(desc));

  nsXPIDLString message;
  const PRUnichar* msgArgs[] = { scheme.get(), uri.get(), desc.get() };
  appstrings->FormatStringFromName(NS_LITERAL_STRING("externalProtocolPrompt").get(),
                                   msgArgs,
                                   NS_ARRAY_LENGTH(msgArgs),
                                   getter_Copies(message));

  if (scheme.IsEmpty() || uri.IsEmpty() || title.IsEmpty() ||
      checkMsg.IsEmpty() || launchBtn.IsEmpty() || message.IsEmpty() ||
      desc.IsEmpty())
    return PR_FALSE;

  // all pieces assembled, now we can pose the dialog
  PRInt32 choice = 1; // assume "cancel" in case of failure
  rv = prompt->ConfirmEx(title.get(), message.get(),
                         nsIPrompt::BUTTON_DELAY_ENABLE +
                         nsIPrompt::BUTTON_POS_1_DEFAULT +
                         (nsIPrompt::BUTTON_TITLE_IS_STRING * nsIPrompt::BUTTON_POS_0) +
                         (nsIPrompt::BUTTON_TITLE_CANCEL * nsIPrompt::BUTTON_POS_1),
                         launchBtn.get(), 0, 0, checkMsg.get(),
                         aRemember, &choice);

  if (NS_SUCCEEDED(rv) && choice == 0)
    return PR_TRUE;

  return PR_FALSE;
}

NS_IMETHODIMP nsExternalHelperAppService::GetApplicationDescription(const nsACString& aScheme, nsAString& _retval)
{
  // this method should only be implemented by each OS specific implementation of this service.
  return NS_ERROR_NOT_IMPLEMENTED;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
// Methods related to deleting temporary files on exit
//////////////////////////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsExternalHelperAppService::DeleteTemporaryFileOnExit(nsIFile * aTemporaryFile)
{
  nsresult rv = NS_OK;
  PRBool isFile = PR_FALSE;
  nsCOMPtr<nsILocalFile> localFile (do_QueryInterface(aTemporaryFile, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // as a safety measure, make sure the nsIFile is really a file and not a directory object.
  localFile->IsFile(&isFile);
  if (!isFile) return NS_OK;

  mTemporaryFilesList.AppendObject(localFile);

  return NS_OK;
}

void nsExternalHelperAppService::FixFilePermissions(nsILocalFile* aFile)
{
  // This space intentionally left blank
}

nsresult nsExternalHelperAppService::ExpungeTemporaryFiles()
{
  PRInt32 numEntries = mTemporaryFilesList.Count();
  nsILocalFile* localFile;
  for (PRInt32 index = 0; index < numEntries; index++)
  {
    localFile = mTemporaryFilesList[index];
    if (localFile)
      localFile->Remove(PR_FALSE);
  }

  mTemporaryFilesList.Clear();

  return NS_OK;
}

// XPCOM profile change observer
NS_IMETHODIMP
nsExternalHelperAppService::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData )
{
  if (!strcmp(aTopic, "profile-before-change")) {
    ExpungeTemporaryFiles();
#ifdef MOZ_RDF
    nsCOMPtr <nsIRDFRemoteDataSource> flushableDataSource = do_QueryInterface(mOverRideDataSource);
    if (flushableDataSource)
      flushableDataSource->Flush();
    mOverRideDataSource = nsnull;
    mDataSourceInitialized = PR_FALSE;
#endif
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
                                           const nsAString& aSuggestedFilename,
                                           PRUint32 aReason)
: mMimeInfo(aMIMEInfo)
, mWindowContext(aWindowContext)
, mWindowToClose(nsnull)
, mSuggestedFileName(aSuggestedFilename)
, mCanceled(PR_FALSE)
, mShouldCloseWindow(PR_FALSE)
, mReceivedDispositionInfo(PR_FALSE)
, mStopRequestIssued(PR_FALSE)
, mProgressListenerInitialized(PR_FALSE)
, mReason(aReason)
, mContentLength(-1)
, mProgress(0)
, mRequest(nsnull)
{

  // make sure the extention includes the '.'
  if (!aTempFileExtension.IsEmpty() && aTempFileExtension.First() != '.')
    mTempFileExtension = PRUnichar('.');
  AppendUTF8toUTF16(aTempFileExtension, mTempFileExtension);

  // replace platform specific path separator and illegal characters to avoid any confusion
  mSuggestedFileName.ReplaceChar(FILE_PATH_SEPARATOR FILE_ILLEGAL_CHARACTERS, '-');
  mTempFileExtension.ReplaceChar(FILE_PATH_SEPARATOR FILE_ILLEGAL_CHARACTERS, '-');
  
  // Make sure extension is correct.
  EnsureSuggestedFileName();

  sSrv->AddRef();
}

nsExternalAppHandler::~nsExternalAppHandler()
{
  // Not using NS_RELEASE, since we don't want to set sSrv to NULL
  sSrv->Release();
}

NS_IMETHODIMP nsExternalAppHandler::SetWebProgressListener(nsIWebProgressListener2 * aWebProgressListener)
{ 
  // this call back means we've successfully brought up the 
  // progress window so set the appropriate flag, even though
  // aWebProgressListener might be null
  
  if (mReceivedDispositionInfo)
    mProgressListenerInitialized = PR_TRUE;

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

NS_IMETHODIMP nsExternalAppHandler::GetTimeDownloadStarted(PRTime* aTime)
{
  *aTime = mTimeDownloadStarted;
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

#define SALT_SIZE 8
#define TABLE_SIZE 36
const PRUnichar table[] = 
  { 'a','b','c','d','e','f','g','h','i','j',
    'k','l','m','n','o','p','q','r','s','t',
    'u','v','w','x','y','z','0','1','2','3',
    '4','5','6','7','8','9'};

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
  nsresult rv;

#ifdef XP_MACOSX
 // create a temp file for the data...and open it for writing.
 // use NS_MAC_DEFAULT_DOWNLOAD_DIR which gets download folder from InternetConfig
 // if it can't get download folder pref, then it uses desktop folder
  rv = NS_GetSpecialDirectory(NS_MAC_DEFAULT_DOWNLOAD_DIR,
                              getter_AddRefs(mTempFile));
#else
  // create a temp file for the data...and open it for writing.
  rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(mTempFile));
#endif
  NS_ENSURE_SUCCESS(rv, rv);

  // We need to generate a name for the temp file that we are going to be streaming data to. 
  // We don't want this name to be predictable for security reasons so we are going to generate a 
  // "salted" name.....
  nsAutoString saltedTempLeafName;
  // this salting code was ripped directly from the profile manager.
  // turn PR_Now() into milliseconds since epoch
  // and salt rand with that. 
  double fpTime;
  LL_L2D(fpTime, PR_Now());
  srand((uint)(fpTime * 1e-6 + 0.5));
  PRInt32 i;
  for (i=0;i<SALT_SIZE;i++) 
  {
    saltedTempLeafName.Append(table[(rand()%TABLE_SIZE)]);
  }

  // now append our extension.
  nsCAutoString ext;
  mMimeInfo->GetPrimaryExtension(ext);
  if (!ext.IsEmpty()) {
    if (ext.First() != '.')
      saltedTempLeafName.Append(PRUnichar('.'));
    AppendUTF8toUTF16(ext, saltedTempLeafName);
  }

  mTempFile->Append(saltedTempLeafName); // make this file unique!!!
  mTempFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);

#ifdef XP_MACOSX
 // Now that the file exists set Mac type and creator
  if (mMimeInfo)
  {
    nsCOMPtr<nsILocalFileMac> macfile = do_QueryInterface(mTempFile);
    if (macfile)
    {
      PRUint32 type, creator;
      mMimeInfo->GetMacType(&type);
      mMimeInfo->GetMacCreator(&creator);
      macfile->SetFileType(type);
      macfile->SetFileCreator(creator);
    }
  }
#endif

  rv = NS_NewLocalFileOutputStream(getter_AddRefs(mOutStream), mTempFile,
                                   PR_WRONLY | PR_CREATE_FILE, 0600);
  if (NS_FAILED(rv)) {
    mTempFile->Remove(PR_FALSE);
    return rv;
  }

#ifdef XP_MACOSX
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
  nsCOMPtr<nsIPropertyBag2> props(do_QueryInterface(request, &rv));
  if (props) {
    rv = props->GetPropertyAsInt64(NS_CHANNEL_PROP_CONTENT_LENGTH,
                                   &mContentLength.mValue);
  }
  // If that failed, ask the channel
  if (NS_FAILED(rv) && aChannel) {
    PRInt32 len;
    aChannel->GetContentLength(&len);
    mContentLength = len;
  }

  // Determine whether a new window was opened specifically for this request
  if (props) {
    PRBool tmp = PR_FALSE;
    props->GetPropertyAsBool(NS_LITERAL_STRING("docshell.newWindowTarget"),
                             &tmp);
    mShouldCloseWindow = tmp;
  }

  // Now get the URI
  if (aChannel)
  {
    aChannel->GetURI(getter_AddRefs(mSourceUrl));
  }

  rv = SetUpTempFile(aChannel);
  if (NS_FAILED(rv)) {
    mCanceled = PR_TRUE;
    request->Cancel(rv);
    nsAutoString path;
    if (mTempFile)
      mTempFile->GetPath(path);
    SendStatusChange(kWriteError, rv, request, path);
    return NS_OK;
  }

  // Extract mime type for later use below.
  nsCAutoString MIMEType;
  mMimeInfo->GetMIMEType(MIMEType);

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
        mShouldCloseWindow = PR_FALSE;
      }
    }
  }

  // Close the underlying DOMWindow if there is no refresh header
  // and it was opened specifically for the download
  MaybeCloseWindow();

  nsCOMPtr<nsIEncodedChannel> encChannel = do_QueryInterface( aChannel );
  if (encChannel) 
  {
    // Turn off content encoding conversions if needed
    PRBool applyConversion = PR_TRUE;

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
          PRBool hasMore;
          rv = encEnum->HasMore(&hasMore);
          if (NS_SUCCEEDED(rv) && hasMore)
          {
            nsCAutoString encType;
            rv = encEnum->GetNext(encType);
            if (NS_SUCCEEDED(rv) && !encType.IsEmpty())
            {
              NS_ASSERTION(sSrv, "Where did the service go?");
              sSrv->ApplyDecodingForExtension(extension,
                                              encType,
                                              &applyConversion);
            }
          }
        }
      }    
    }

    encChannel->SetApplyConversion( applyConversion );
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

  PRBool alwaysAsk = PR_TRUE;
  mMimeInfo->GetAlwaysAskBeforeHandling(&alwaysAsk);
  if (alwaysAsk)
  {
    // But we *don't* ask if this mimeInfo didn't come from
    // our mimeTypes.rdf data source and the user has said
    // at some point in the distant past that they don't
    // want to be asked.  The latter fact would have been
    // stored in pref strings back in the old days.
    NS_ASSERTION(sSrv, "Service gone away!?");
    if (!sSrv->MIMETypeIsInDataSource(MIMEType.get()))
    {
      if (!GetNeverAskFlagFromPref(NEVER_ASK_FOR_SAVE_TO_DISK_PREF, MIMEType.get()))
      {
        // Don't need to ask after all.
        alwaysAsk = PR_FALSE;
        // Make sure action matches pref (save to disk).
        mMimeInfo->SetPreferredAction(nsIMIMEInfo::saveToDisk);
      }
      else if (!GetNeverAskFlagFromPref(NEVER_ASK_FOR_OPEN_FILE_PREF, MIMEType.get()))
      {
        // Don't need to ask after all.
        alwaysAsk = PR_FALSE;
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

  if (alwaysAsk)
  {
    // do this first! make sure we don't try to take an action until the user tells us what they want to do
    // with it...
    mReceivedDispositionInfo = PR_FALSE; 

    // invoke the dialog!!!!! use mWindowContext as the window context parameter for the dialog request
    mDialog = do_CreateInstance( NS_IHELPERAPPLAUNCHERDLG_CONTRACTID, &rv );
    NS_ENSURE_SUCCESS(rv, rv);

    // this will create a reference cycle (the dialog holds a reference to us as
    // nsIHelperAppLauncher), which will be broken in Cancel or
    // CreateProgressListener.
    rv = mDialog->Show( this, mWindowContext, mReason );

    // what do we do if the dialog failed? I guess we should call Cancel and abort the load....
  }
  else
  {
    mReceivedDispositionInfo = PR_TRUE; // no need to wait for a response from the user

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
    nsCOMPtr<nsIFile> prefApp;
    mMimeInfo->GetPreferredApplicationHandler(getter_AddRefs(prefApp));
    if (action != nsIMIMEInfo::useHelperApp || !prefApp) {
      nsCOMPtr<nsIFile> fileToTest;
      GetTargetFile(getter_AddRefs(fileToTest));
      if (fileToTest) {
        PRBool isExecutable;
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
        rv = LaunchWithApplication(nsnull, PR_FALSE);
    }
    else // Various unknown actions go here too
    {
        rv = SaveToDisk(nsnull, PR_FALSE);
    }
  }

  // Now let's mark the downloaded URL as visited
  nsCOMPtr<nsIGlobalHistory> history(do_GetService(NS_GLOBALHISTORY_CONTRACTID));
  nsCAutoString spec;
  mSourceUrl->GetSpec(spec);
  if (history && !spec.IsEmpty())
  {
    PRBool visited;
    rv = history->IsVisited(spec.get(), &visited);
    if (NS_FAILED(rv))
        return rv;
    history->AddPage(spec.get());
    if (!visited) {
      nsCOMPtr<nsIObserverService> obsService =
          do_GetService("@mozilla.org/observer-service;1");
      if (obsService) {
        obsService->NotifyObservers(mSourceUrl, NS_LINK_VISITED_EVENT_TOPIC, nsnull);
      }
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
    nsCOMPtr<nsIStringBundleService> s = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
    if (s)
    {
        nsCOMPtr<nsIStringBundle> bundle;
        if (NS_SUCCEEDED(s->CreateBundle("chrome://global/locale/nsWebBrowserPersist.properties", getter_AddRefs(bundle))))
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
              {
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
  if (mCanceled) // then go cancel our underlying channel too
    return request->Cancel(NS_BINDING_ABORTED);

  // read the data out of the stream and write it to the temp file.
  if (mOutStream && count > 0)
  {
    PRUint32 numBytesRead = 0; 
    PRUint32 numBytesWritten = 0;
    mProgress += count;
    PRBool readError = PR_TRUE;
    while (NS_SUCCEEDED(rv) && count > 0) // while we still have bytes to copy...
    {
      readError = PR_TRUE;
      rv = inStr->Read(mDataBuffer, PR_MIN(count, DATA_BUFFER_SIZE - 1), &numBytesRead);
      if (NS_SUCCEEDED(rv))
      {
        if (count >= numBytesRead)
          count -= numBytesRead; // subtract off the number of bytes we just read
        else
          count = 0;
        readError = PR_FALSE;
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
  mStopRequestIssued = PR_TRUE;
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
    nsMIMEInfoHandleAction action = nsIMIMEInfo::saveToDisk;
    mMimeInfo->GetPreferredAction(&action);
    if (action == nsIMIMEInfo::useHelperApp ||
        action == nsIMIMEInfo::useSystemDefault)
    {
      // Make sure the suggested name is unique since in this case we don't
      // have a file name that was guaranteed to be unique by going through
      // the File Save dialog
      rv = mFinalFileDestination->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
      if (NS_SUCCEEDED(rv))
      {
        // Source and dest dirs should be == so this should just do a rename
        rv = MoveFile(mFinalFileDestination);
        if (NS_SUCCEEDED(rv))
          rv = OpenWithApplication();
      }
    }
    else // Various unknown actions go here too
    {
      // XXX Put progress dialog in barber-pole mode
      //     and change text to say "Copying from:".
      rv = MoveFile(mFinalFileDestination);
      if (NS_SUCCEEDED(rv) && action == nsIMIMEInfo::saveToDisk)
      {
        nsCOMPtr<nsILocalFile> destfile(do_QueryInterface(mFinalFileDestination));
        sSrv->FixFilePermissions(destfile);
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
      mWebProgressListener->OnStateChange(nsnull, nsnull, nsIWebProgressListener::STATE_STOP, NS_OK);
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
  
  nsCOMPtr<nsILocalFile> lf(do_QueryInterface(mTempFile));
  rv = aTransfer->Init(mSourceUrl, target, EmptyString(),
                       mMimeInfo, mTimeDownloadStarted, lf, this);
  if (NS_FAILED(rv)) return rv;

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
    tr->OnStateChange(nsnull, mRequest, nsIWebProgressListener::STATE_START, NS_OK);

  // note we might not have a listener here if the QI() failed, or if
  // there is no nsITransfer object, but we still call
  // SetWebProgressListener() to make sure our progress state is sane
  // NOTE: This will set up a reference cycle (this nsITransfer has us set up as
  // its observer). This cycle will be broken in Cancel, CloseProgressWindow or
  // OnStopRequest.
  SetWebProgressListener(tr);

  return rv;
}

nsresult nsExternalAppHandler::PromptForSaveToFile(nsILocalFile ** aNewFile, const nsAFlatString &aDefaultFile, const nsAFlatString &aFileExtension)
{
  // invoke the dialog!!!!! use mWindowContext as the window context parameter for the dialog request
  // Convert to use file picker? No, then embeddors could not do any sort of
  // "AutoDownload" w/o showing a prompt
  nsresult rv = NS_OK;
  if (!mDialog)
  {
    // Get helper app launcher dialog.
    mDialog = do_CreateInstance( NS_IHELPERAPPLAUNCHERDLG_CONTRACTID, &rv );
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
                                    aNewFile);
  return rv;
}

nsresult nsExternalAppHandler::MoveFile(nsIFile * aNewFileLocation)
{
  nsresult rv = NS_OK;
  NS_ASSERTION(mStopRequestIssued, "uhoh, how did we get here if we aren't done getting data?");
 
  nsCOMPtr<nsILocalFile> fileToUse = do_QueryInterface(aNewFileLocation);

  // if the on stop request was actually issued then it's now time to actually perform the file move....
  if (mStopRequestIssued && fileToUse)
  {
    // Unfortunately, MoveTo will fail if a file already exists at the user specified location....
    // but the user has told us, this is where they want the file! (when we threw up the save to file dialog,
    // it told them the file already exists and do they wish to over write it. So it should be okay to delete
    // fileToUse if it already exists.
    PRBool equalToTempFile = PR_FALSE;
    PRBool filetoUseAlreadyExists = PR_FALSE;
    fileToUse->Equals(mTempFile, &equalToTempFile);
    fileToUse->Exists(&filetoUseAlreadyExists);
    if (filetoUseAlreadyExists && !equalToTempFile)
      fileToUse->Remove(PR_FALSE);

     // extract the new leaf name from the file location
     nsAutoString fileName;
     fileToUse->GetLeafName(fileName);
     nsCOMPtr<nsIFile> directoryLocation;
     rv = fileToUse->GetParent(getter_AddRefs(directoryLocation));
     if (directoryLocation)
     {
       rv = mTempFile->MoveTo(directoryLocation, fileName);
     }
     if (NS_FAILED(rv))
     {
       // Send error notification.        
       nsAutoString path;
       fileToUse->GetPath(path);
       SendStatusChange(kWriteError, rv, nsnull, path);
       Cancel(rv); // Cancel (and clean up temp file).
     }
#if defined(XP_OS2)
     else
     {
       // tag the file with its source URI
       nsCOMPtr<nsILocalFileOS2> localFileOS2 = do_QueryInterface(fileToUse);
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

NS_IMETHODIMP nsExternalAppHandler::SaveToDisk(nsIFile * aNewFileLocation, PRBool aRememberThisPreference)
{
  nsresult rv = NS_OK;
  if (mCanceled)
    return NS_OK;

  mMimeInfo->SetPreferredAction(nsIMIMEInfo::saveToDisk);

  // The helper app dialog has told us what to do.
  mReceivedDispositionInfo = PR_TRUE;

  nsCOMPtr<nsILocalFile> fileToUse = do_QueryInterface(aNewFileLocation);
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
      rv = NS_NewLocalFileOutputStream(getter_AddRefs(mOutStream), mTempFile,
                                         PR_WRONLY | PR_APPEND, 0600);
      if (NS_FAILED(rv)) { // (Re-)opening the output stream failed. bad luck.
        nsAutoString path;
        mTempFile->GetPath(path);
        SendStatusChange(kWriteError, rv, nsnull, path);
        Cancel(rv);
        return NS_OK;
      }
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
    rv = mMimeInfo->LaunchWithFile(mFinalFileDestination);
    if (NS_FAILED(rv))
    {
      // Send error notification.
      nsAutoString path;
      mFinalFileDestination->GetPath(path);
      SendStatusChange(kLaunchError, rv, nsnull, path);
      Cancel(rv); // Cancel, and clean up temp file.
    }
    else
    {
      PRBool deleteTempFileOnExit;
      nsresult result = NS_ERROR_NOT_AVAILABLE;  // don't return this!
      nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
      if (prefs) {
        result = prefs->GetBoolPref("browser.helperApps.deleteTempFileOnExit",
                                    &deleteTempFileOnExit);
      }
      if (NS_FAILED(result)) {
        // No pref set; use default value
#if !defined(XP_MACOSX)
          // Mac users have been very verbal about temp files being deleted on
          // app exit - they don't like it - but we'll continue to do this on
          // other platforms for now.
        deleteTempFileOnExit = PR_TRUE;
#else
        deleteTempFileOnExit = PR_FALSE;
#endif
      }
      if (deleteTempFileOnExit) {
        NS_ASSERTION(sSrv, "Service gone away!?");
        sSrv->DeleteTemporaryFileOnExit(mFinalFileDestination);
      }
    }
  }

  return rv;
}

// LaunchWithApplication should only be called by the helper app dialog which allows
// the user to say launch with application or save to disk. It doesn't actually 
// perform launch with application. That won't happen until we are done downloading
// the content and are sure we've showna progress dialog. This was done to simplify the 
// logic that was showing up in this method. 

NS_IMETHODIMP nsExternalAppHandler::LaunchWithApplication(nsIFile * aApplication, PRBool aRememberThisPreference)
{
  if (mCanceled)
    return NS_OK;

  // user has chosen to launch using an application, fire any refresh tags now...
  ProcessAnyRefreshTags(); 
  
  mReceivedDispositionInfo = PR_TRUE; 
  if (mMimeInfo && aApplication)
    mMimeInfo->SetPreferredApplicationHandler(aApplication);

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
  
  // The directories specified here must match those specified in SetUpTempFile().  This avoids
  // having to do a copy of the file when it finishes downloading and the potential for errors
  // that would introduce
#ifdef XP_MACOSX
  NS_GetSpecialDirectory(NS_MAC_DEFAULT_DOWNLOAD_DIR, getter_AddRefs(fileToUse));
#else
  NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(fileToUse));
#endif

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
  
  // We'll make sure this results in a unique name later

  mFinalFileDestination = do_QueryInterface(fileToUse);

  // launch the progress window now that the user has picked the desired action.
  if (!mProgressListenerInitialized)
   CreateProgressListener();

  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::Cancel(nsresult aReason)
{
  NS_ENSURE_ARG(NS_FAILED(aReason));
  // XXX should not ignore the reason

  mCanceled = PR_TRUE;
  // Break our reference cycle with the helper app dialog (set up in
  // OnStartRequest)
  mDialog = nsnull;
  // shutdown our stream to the temp file
  if (mOutStream)
  {
    mOutStream->Close();
    mOutStream = nsnull;
  }

  // clean up after ourselves and delete the temp file...
  // but only if we got asked to open the file. when saving,
  // we leave the file there - the partial file might be useful
  // But if we haven't received disposition info yet, then we're
  // here because the user cancelled the helper app dialog.
  // Delete the file in this case.
  nsMIMEInfoHandleAction action = nsIMIMEInfo::saveToDisk;
  mMimeInfo->GetPreferredAction(&action);
  if (mTempFile &&
      (!mReceivedDispositionInfo || action != nsIMIMEInfo::saveToDisk))
  {
    mTempFile->Remove(PR_FALSE);
    mTempFile = nsnull;
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

PRBool nsExternalAppHandler::GetNeverAskFlagFromPref(const char * prefName, const char * aContentType)
{
  // Search the obsolete pref strings.
  nsresult rv;
  nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  nsCOMPtr<nsIPrefBranch> prefBranch;
  if (prefs)
    rv = prefs->GetBranch(NEVER_ASK_PREF_BRANCH, getter_AddRefs(prefBranch));
  if (NS_SUCCEEDED(rv) && prefBranch)
  {
    nsXPIDLCString prefCString;
    nsCAutoString prefValue;
    rv = prefBranch->GetCharPref(prefName, getter_Copies(prefCString));
    if (NS_SUCCEEDED(rv) && !prefCString.IsEmpty())
    {
      NS_UnescapeURL(prefCString);
      nsACString::const_iterator start, end;
      prefCString.BeginReading(start);
      prefCString.EndReading(end);
      if (CaseInsensitiveFindInReadable(nsDependentCString(aContentType), start, end))
        return PR_FALSE;
    }
  }
  // Default is true, if not found in the pref string.
  return PR_TRUE;
}

nsresult nsExternalAppHandler::MaybeCloseWindow()
{
  nsCOMPtr<nsIDOMWindow> window(do_GetInterface(mWindowContext));
  nsCOMPtr<nsIDOMWindowInternal> internalWindow = do_QueryInterface(window);
  NS_ENSURE_STATE(internalWindow);

  if (mShouldCloseWindow) {
    // Reset the window context to the opener window so that the dependent
    // dialogs have a parent
    nsCOMPtr<nsIDOMWindowInternal> opener;
    internalWindow->GetOpener(getter_AddRefs(opener));

    if (opener) {
      mWindowContext = do_GetInterface(opener);

      // Now close the old window.  Do it on a timer so that we don't run
      // into issues trying to close the window before it has fully opened.
      NS_ASSERTION(!mTimer, "mTimer was already initialized once!");
      mTimer = do_CreateInstance("@mozilla.org/timer;1");
      if (!mTimer) {
        return NS_ERROR_FAILURE;
      }

      mTimer->InitWithCallback(this, 0, nsITimer::TYPE_ONE_SHOT);
      mWindowToClose = internalWindow;
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

  // (1) Ask the OS for a mime info
  PRBool found;
  *_retval = GetMIMEInfoFromOS(typeToUse, aFileExt, &found).get();
  LOG(("OS gave back 0x%p - found: %i\n", *_retval, found));
  // If we got no mimeinfo, something went wrong. Probably lack of memory.
  if (!*_retval)
    return NS_ERROR_OUT_OF_MEMORY;

  // (2) Now, let's see if we can find something in our datasource
  // This will not overwrite the OS information that interests us
  // (i.e. default application, default app. description)
  nsresult rv = GetMIMEInfoForMimeTypeFromDS(typeToUse, *_retval);
  found = found || NS_SUCCEEDED(rv);

  LOG(("Data source: Via type: retval 0x%08x\n", rv));

  if (!found || NS_FAILED(rv)) {
    // No type match, try extension match
    if (!aFileExt.IsEmpty()) {
      rv = GetMIMEInfoForExtensionFromDS(aFileExt, *_retval);
      LOG(("Data source: Via ext: retval 0x%08x\n", rv));
      found = found || NS_SUCCEEDED(rv);
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
      rv = GetMIMEInfoForMimeTypeFromExtras(typeToUse, *_retval);
    LOG(("Searched extras (by type), rv 0x%08X\n", rv));
    // If that didn't work out, try file extension from extras
    if (NS_FAILED(rv) && !aFileExt.IsEmpty()) {
      rv = GetMIMEInfoForExtensionFromExtras(aFileExt, *_retval);
      LOG(("Searched extras (by ext), rv 0x%08X\n", rv));
    }
  }

  // Finally, check if we got a file extension and if yes, if it is an
  // extension on the mimeinfo, in which case we want it to be the primary one
  if (!aFileExt.IsEmpty()) {
    PRBool matches = PR_FALSE;
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
  // 2. User-set preferences (mimeTypes.rdf)
  // 3. OS-provided information
  // 4. our "extras" array
  // 5. Information from plugins
  // 6. The "ext-to-type-mapping" category

  nsresult rv = NS_OK;
  // First of all, check our default entries
  for (size_t i = 0; i < NS_ARRAY_LENGTH(defaultMimeEntries); i++)
  {
    if (aFileExt.LowerCaseEqualsASCII(defaultMimeEntries[i].mFileExtension)) {
      aContentType = defaultMimeEntries[i].mMimeType;
      return rv;
    }
  }

  // Check RDF DS
  PRBool found = GetTypeFromDS(aFileExt, aContentType);
  if (found)
    return NS_OK;

  // Ask OS.
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
  nsCOMPtr<nsIPluginHost> pluginHost (do_GetService(kPluginManagerCID, &rv));
  if (NS_SUCCEEDED(rv)) {
    if (NS_SUCCEEDED(pluginHost->IsPluginEnabledForExtension(flatExt.get(), mimeType)))
    {
      aContentType = mimeType;
      return NS_OK;
    }
  }
  
  rv = NS_OK;
  // Let's see if an extension added something
  nsCOMPtr<nsICategoryManager> catMan(do_GetService("@mozilla.org/categorymanager;1"));
  if (catMan) {
    nsXPIDLCString type;
    rv = catMan->GetCategoryEntry("ext-to-type-mapping", flatExt.get(), getter_Copies(type));
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
  
#ifdef XP_MACOSX
  nsCOMPtr<nsILocalFileMac> macFile;
  macFile = do_QueryInterface( aFile, &rv );
  if (NS_SUCCEEDED( rv ) && fileExt.IsEmpty())
  {
    PRUint32 type, creator;
    macFile->GetFileType( (OSType*)&type );
    macFile->GetFileCreator( (OSType*)&creator );   
    nsCOMPtr<nsIInternetConfigService> icService (do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID));
    if (icService)
    {
      rv = icService->GetMIMEInfoFromTypeCreator(type, creator, fileExt.get(), getter_AddRefs(info));
      if (NS_SUCCEEDED(rv))
        return info->GetMIMEType(aContentType);
    }
  }
#endif

  // Windows, unix and mac when no type match occured.   
  if (fileExt.IsEmpty())
    return NS_ERROR_FAILURE;    
  return GetTypeFromExtension(fileExt, aContentType);
}

nsresult nsExternalHelperAppService::GetMIMEInfoForMimeTypeFromExtras(const nsACString& aContentType, nsIMIMEInfo * aMIMEInfo )
{
  NS_ENSURE_ARG( aMIMEInfo );

  NS_ENSURE_ARG( !aContentType.IsEmpty() );

  // Look for default entry with matching mime type.
  nsCAutoString MIMEType(aContentType);
  ToLowerCase(MIMEType);
  PRInt32 numEntries = NS_ARRAY_LENGTH(extraMimeEntries);
  for (PRInt32 index = 0; index < numEntries; index++)
  {
      if ( MIMEType.Equals(extraMimeEntries[index].mMimeType) )
      {
          // This is the one. Set attributes appropriately.
          aMIMEInfo->SetFileExtensions(nsDependentCString(extraMimeEntries[index].mFileExtensions));
          aMIMEInfo->SetDescription(NS_ConvertASCIItoUTF16(extraMimeEntries[index].mDescription));
          aMIMEInfo->SetMacType(extraMimeEntries[index].mMactype);
          aMIMEInfo->SetMacCreator(extraMimeEntries[index].mMacCreator);

          return NS_OK;
      }
  }

  return NS_ERROR_NOT_AVAILABLE;
}

nsresult nsExternalHelperAppService::GetMIMEInfoForExtensionFromExtras(const nsACString& aExtension, nsIMIMEInfo * aMIMEInfo)
{
  nsCAutoString type;
  PRBool found = GetTypeFromExtras(aExtension, type);
  if (!found)
    return NS_ERROR_NOT_AVAILABLE;
  return GetMIMEInfoForMimeTypeFromExtras(type, aMIMEInfo);
}

PRBool nsExternalHelperAppService::GetTypeFromExtras(const nsACString& aExtension, nsACString& aMIMEType)
{
  NS_ASSERTION(!aExtension.IsEmpty(), "Empty aExtension parameter!");

  // Look for default entry with matching extension.
  nsDependentCString::const_iterator start, end, iter;
  PRInt32 numEntries = NS_ARRAY_LENGTH(extraMimeEntries);
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
              return PR_TRUE;
          }
          if (iter != end) {
            ++iter;
          }
          start = iter;
      }
  }

  return PR_FALSE;
}
