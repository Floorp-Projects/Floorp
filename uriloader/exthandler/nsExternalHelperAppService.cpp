/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 *   Bill Law <law@netscape.com>
 */

#include "nsExternalHelperAppService.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIFile.h"
#include "nsIChannel.h"
#include "nsIFileURL.h"
#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsMemory.h"
#include "nsIStreamListener.h"
#include "nsIMIMEService.h"
#include "nsILoadGroup.h"
#include "nsCURILoader.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsIDownload.h"
#include "nsReadableUtils.h"

// used to manage our in memory data source of helper applications
#include "nsRDFCID.h"
#include "rdf.h"
#include "nsIRDFService.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIFileSpec.h"
#include "nsHelperAppRDF.h"
#include "nsIMIMEInfo.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIRefreshURI.h"
#include "nsIDocumentLoader.h"
#include "nsIHelperAppLauncherDialog.h"
#include "nsITransport.h"
#include "nsIFileTransportService.h"
#include "nsCExternalHandlerService.h" // contains contractids for the helper app service
#include "nsIIOService.h"
#include "nsNetCID.h"

#include "nsMimeTypes.h"
// used for header disposition information.
#include "nsIHttpChannel.h"
#include "nsIEncodedChannel.h"
#include "nsIMultiPartChannel.h"
#include "nsIAtom.h"
#include "nsIObserverService.h" // so we can be an xpcom shutdown observer

#if defined(XP_MAC) || defined (XP_MACOSX)
#include "nsILocalFileMac.h"
#include "nsIInternetConfigService.h"
#endif // defined(XP_MAC) || defined (XP_MACOSX)
#ifdef XP_MAC
#include "nsIAppleFileDecoder.h"
#endif // XP_MAC

#include "nsIPluginHost.h"
#include "nsEscape.h"

#include "nsIStringBundle.h"
#include "nsIPromptService.h"
#include "nsIDOMWindow.h"

#include "nsITextToSubURI.h"

const char *FORCE_ALWAYS_ASK_PREF = "browser.helperApps.alwaysAsk.force";

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID, NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kPluginManagerCID, NS_PLUGINMANAGER_CID);


// forward declaration of a private helper function
static PRBool PR_CALLBACK DeleteEntry(nsHashKey *aKey, void *aData, void* closure);

// The following static table lists all of the "default" content type mappings we are going to use.                 
// These are only the ones handled internally.  These must be listed here so that we can do the
// proper file-extension to content-type mapping.  These cannot be overridden by user helper app
// prefs.
static nsDefaultMimeTypeEntry defaultMimeEntries [] = 
{
  { TEXT_PLAIN, "txt,text", "Text File", 'TEXT', 'ttxt' },
  { TEXT_HTML, "html,htm,shtml,ehtml", "HyperText Markup Language", 'TEXT', 'MOSS' },
  { TEXT_RDF, "rdf", "Resource Description Framework", 'TEXT','ttxt' },
  { TEXT_XUL, "xul", "XML-Based User Interface Language", 'TEXT', 'ttxt' },
  { TEXT_XML,          "xml,xsl,xbl", "Extensible Markup Language", 'TEXT', 'ttxt' },
  { "application/xhtml+xml", "xhtml,xht", "Extensible HyperText Markup Language", 'TEXT', 'ttxt' },
#ifdef MOZ_SVG
  { "image/svg+xml", "svg", "Scalable Vector Graphics", 'svg ', 'ttxt' },
#endif
  { TEXT_CSS, "css", "Style Sheet", 'TEXT', 'ttxt' },
  { APPLICATION_JAVASCRIPT, "js", "Javascript Source File", 'TEXT', 'ttxt' },
  { MESSAGE_RFC822, "eml", "RFC-822 data", 'TEXT', 'MOSS' },
  { IMAGE_GIF, "gif", "GIF Image", 0,0 },
  { IMAGE_JPG, "jpeg,jpg,jfif,pjpeg,pjp", "JPEG Image", 0, 0 },
  { IMAGE_PNG, "png", "PNG Image", 0, 0 },
  { IMAGE_BMP, "bmp", "BMP Image", 0, 0 },
  { IMAGE_ICO, "ico,cur", "ICO Image", 0, 0 },
  { IMAGE_MNG, "mng", "MNG Image", 0, 0 },
  { IMAGE_JNG, "jng", "JNG Image", 0, 0 },
  { IMAGE_XBM, "xbm", "XBM Image", 0, 0 },
  { APPLICATION_XPINSTALL, "xpi", "XPInstall Install", 'xpi*','MOSS' },
};

// This table lists all of the 'extra" content types that we can deduce from particular
// file extensions.  These entries also ensure that we provide a good descriptive name
// when we encounter files with these content types and/or extensions.  These can be
// overridden by user helper app prefs.
static nsDefaultMimeTypeEntry extraMimeEntries [] =
{
#if defined(VMS)
  { APPLICATION_OCTET_STREAM, "exe,bin,sav,bck,pcsi,dcx_axpexe,dcx_vaxexe,sfx_axpexe,sfx_vaxexe", "Binary Executable", 0, 0 },
#elif defined(XP_MAC) || defined (XP_MACOSX)// don't define .bin on the mac...use internet config to look that up...
  { APPLICATION_OCTET_STREAM, "exe", "Binary Executable", 0, 0 },
#else
  { APPLICATION_OCTET_STREAM, "exe,bin", "Binary Executable", 0, 0 },
#endif
  { APPLICATION_GZIP2, "gz", "gzip", 0, 0 },
  { IMAGE_ART, "art", "ART Image", 0, 0 },
  { IMAGE_TIFF, "tiff,tif", "TIFF Image", 0, 0 },
  { APPLICATION_POSTSCRIPT, "ps,eps,ai", "Postscript File",0, 0 },
  { "application/x-arj", "arj", "ARJ file", 0,0 },
};

static const char* const nonDecodableTypes [] = {
  "application/tar",
  "application/x-tar",
  0
};

static const char* const nonDecodableExtensions [] = {
  "gz",
  "zip",
  0
};

NS_IMPL_THREADSAFE_ADDREF(nsExternalHelperAppService)
NS_IMPL_THREADSAFE_RELEASE(nsExternalHelperAppService)

NS_INTERFACE_MAP_BEGIN(nsExternalHelperAppService)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIExternalHelperAppService)
   NS_INTERFACE_MAP_ENTRY(nsIExternalHelperAppService)
   NS_INTERFACE_MAP_ENTRY(nsPIExternalAppLauncher)
   NS_INTERFACE_MAP_ENTRY(nsIExternalProtocolService)
   NS_INTERFACE_MAP_ENTRY(nsIMIMEService)
   NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_THREADSAFE

nsExternalHelperAppService::nsExternalHelperAppService() : mDataSourceInitialized(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
  // we need a good guess for a size for our hash table...let's try O(n) where n = # of default
  // entries we'll be adding to the hash table. Of course, we'll be adding more entries as we 
  // discover those content types at run time...
  PRInt32 hashTableSize = sizeof(defaultMimeEntries) / sizeof(defaultMimeEntries[0]);
  mMimeInfoCache = new nsHashtable(hashTableSize);
  AddDefaultMimeTypesToCache();

  /* Add an observer to XPCOM shutdown */
  nsresult rv = NS_OK;
  nsCOMPtr<nsIObserverService> obs = do_GetService("@mozilla.org/observer-service;1", &rv);
  if (obs)
    rv = obs->AddObserver(NS_STATIC_CAST(nsIObserver*, this), NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);

}

nsExternalHelperAppService::~nsExternalHelperAppService()
{
  if (mMimeInfoCache)
  {
    mMimeInfoCache->Reset(DeleteEntry, nsnull);
    delete mMimeInfoCache;
  }
}

nsresult nsExternalHelperAppService::InitDataSource()
{
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
    rdf->GetResource(NC_RDF_DESCRIPTION,   getter_AddRefs(kNC_Description));
    rdf->GetResource(NC_RDF_VALUE,         getter_AddRefs(kNC_Value));
    rdf->GetResource(NC_RDF_FILEEXTENSIONS,getter_AddRefs(kNC_FileExtensions));
    rdf->GetResource(NC_RDF_PATH,          getter_AddRefs(kNC_Path));
    rdf->GetResource(NC_RDF_SAVETODISK,    getter_AddRefs(kNC_SaveToDisk));
    rdf->GetResource(NC_RDF_HANDLEINTERNAL,getter_AddRefs(kNC_HandleInternal));
    rdf->GetResource(NC_RDF_ALWAYSASK,     getter_AddRefs(kNC_AlwaysAsk));  
    rdf->GetResource(NC_RDF_PRETTYNAME,    getter_AddRefs(kNC_PrettyName));  
  }
  
  mDataSourceInitialized = PR_TRUE;
  return rv;
}

/* boolean canHandleContent (in string aMimeContentType); */
NS_IMETHODIMP nsExternalHelperAppService::CanHandleContent(const char *aMimeContentType, nsIURI * aURI, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsExternalHelperAppService::DoContent(const char *aMimeContentType, nsIURI *aURI, nsISupports *aWindowContext, 
                                                    PRBool *aAbortProcess, nsIStreamListener ** aStreamListener)
{
  nsCOMPtr<nsIMIMEInfo> mimeInfo;
  nsCAutoString fileExtension;

  // (1) Try to find a mime object by looking the mime type
  GetFromMIMEType(aMimeContentType, getter_AddRefs(mimeInfo));

  if (!mimeInfo)
  {
    // if we couldn't find one, don't give up yet! Try and see if there is an extension in the 
    // url itself...
    nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);
    if (url)
    {
      url->GetFileExtension(fileExtension);    
      GetFromExtension(fileExtension.get(), getter_AddRefs(mimeInfo));
      // only over write mimeInfo if we got a non-null mime info object.
      if (mimeInfo)
      {
        // The OS might have thought this extension was a different mime type.
        // We must reset this to match the actual mime type.  Otherwise, we
        // won't use this MIMEInfo when we see the real mime type next time.
        mimeInfo->SetMIMEType(aMimeContentType);
      }
    }
  }

  // (2) if we don't have a match yet, see if this type is in our list of extras.
  if (!mimeInfo)
  {
#ifdef XP_WIN
    /* XXX Gross hack to wallpaper over the most common Win32
     * extension issues caused by the fix for bug 116938.  See bug
     * 120327, comment 271 for why this is needed.  Not even sure we
     * want to remove this once we have fixed all this stuff to work
     * right; any info we get from extras on this type is pretty much
     * useless....
     */
    if (PL_strcasecmp(aMimeContentType, APPLICATION_OCTET_STREAM) != 0) {
#endif
      GetMIMEInfoForMimeTypeFromExtras(aMimeContentType, getter_AddRefs(mimeInfo));
#ifdef XP_WIN
    }
#endif
  }

  // (3) if we STILL don't have a mime object for this content type then give up
  // and create a new mime info object for it and use it
  if (!mimeInfo)
  {
    mimeInfo = do_CreateInstance(NS_MIMEINFO_CONTRACTID);
    if (mimeInfo)
    {
      // the file extension was conviently already filled in by our call to FindOSMimeInfoForType.
      mimeInfo->SetFileExtensions(fileExtension.get());
      mimeInfo->SetMIMEType(aMimeContentType);
      // we may need to add a new method to nsIMIMEService so we can add this mime info object to our mime service.
    }
  }

  *aStreamListener = nsnull;
  if (mimeInfo)
  {
    // The primary extension for the mime info may be different from
    // the URL extension.  If the URL extension matches the mime info,
    // set it as the primary extension.  In either case, fileExtension
    // should be the primary extension once we are doen.
    if (fileExtension.IsEmpty()) {
      nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);
      if (url) {
        url->GetFileExtension(fileExtension);
      }
    }

    PRBool matches = PR_FALSE;
    if (!fileExtension.IsEmpty()) {
      mimeInfo->ExtensionExists(fileExtension.get(), &matches);
    }
    if (matches) {
      mimeInfo->SetPrimaryExtension(fileExtension.get());
    } else {
      nsXPIDLCString buf;
      mimeInfo->GetPrimaryExtension(getter_Copies(buf));
      fileExtension = buf;
    }
    
    // this code is incomplete and just here to get things started..
    nsExternalAppHandler * handler = CreateNewExternalHandler(mimeInfo, fileExtension.get(), aWindowContext);
    handler->QueryInterface(NS_GET_IID(nsIStreamListener), (void **) aStreamListener);
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsExternalHelperAppService::ApplyDecodingForType(const char *aMimeContentType, PRBool *aApplyDecoding)
{
  NS_PRECONDITION(aMimeContentType, "Null MIME type");
  *aApplyDecoding = PR_TRUE;
  PRUint32 index;
  for (index = 0; nonDecodableTypes[index]; ++index) {
    if (!PL_strcasecmp(aMimeContentType, nonDecodableTypes[index])) {
      *aApplyDecoding = PR_FALSE;
      break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsExternalHelperAppService::ApplyDecodingForExtension(const char *aExtension, PRBool *aApplyDecoding)
{
  NS_PRECONDITION(aExtension, "Null Extension");
  *aApplyDecoding = PR_TRUE;
  PRUint32 index;
  for(index = 0; nonDecodableExtensions[index]; ++index) {
    if (!PL_strcasecmp(aExtension, nonDecodableExtensions[index])) {
      *aApplyDecoding = PR_FALSE;
      break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsExternalHelperAppService::LaunchAppWithTempFile(nsIMIMEInfo * aMimeInfo, nsIFile * aTempFile)
{
  // this method should only be implemented by each OS specific implementation of this service.
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsExternalAppHandler * nsExternalHelperAppService::CreateNewExternalHandler(nsIMIMEInfo * aMIMEInfo, 
                                                                            const char * aTempFileExtension,
                                                                            nsISupports * aWindowContext)
{
  nsExternalAppHandler* handler = nsnull;
  NS_NEWXPCOM(handler, nsExternalAppHandler);
  // add any XP intialization code for an external handler that we may need here...
  // right now we don't have any but i bet we will before we are done.

  handler->Init(aMIMEInfo, aTempFileExtension, aWindowContext);
  return handler;
}

nsresult nsExternalHelperAppService::FillTopLevelProperties(const char * aContentType, nsIRDFResource * aContentTypeNodeResource, 
                                                            nsIRDFService * aRDFService, nsIMIMEInfo * aMIMEInfo)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFNode> target;
  nsCOMPtr<nsIRDFLiteral> literal;
  const PRUnichar * stringValue;
  
  rv = InitDataSource();
  if (NS_FAILED(rv)) return NS_OK;

  // set the mime type
  aMIMEInfo->SetMIMEType(aContentType);
  
  // set the pretty name description
  FillLiteralValueFromTarget(aContentTypeNodeResource,kNC_Description, &stringValue);
  aMIMEInfo->SetDescription(stringValue);

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
        fileExtension.AssignWithConversion(stringValue);
        if (!fileExtension.IsEmpty())
          aMIMEInfo->AppendExtension(fileExtension.get());
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

  nsCString contentTypeHandlerNodeName (NC_CONTENT_NODE_HANDLER_PREFIX);
  contentTypeHandlerNodeName.Append(aContentType);

  nsCOMPtr<nsIRDFResource> contentTypeHandlerNodeResource;
  aRDFService->GetResource(contentTypeHandlerNodeName.get(), getter_AddRefs(contentTypeHandlerNodeResource));
  NS_ENSURE_TRUE(contentTypeHandlerNodeResource, NS_ERROR_FAILURE); // that's not good! we have an error in the rdf file

  // now process the application handler information
  aMIMEInfo->SetPreferredAction(nsIMIMEInfo::useHelperApp);

  // save to disk
  FillLiteralValueFromTarget(contentTypeHandlerNodeResource,kNC_SaveToDisk, &stringValue);
  NS_NAMED_LITERAL_STRING( trueString, "true" );
  if (stringValue && trueString.Equals(stringValue))
       aMIMEInfo->SetPreferredAction(nsIMIMEInfo::saveToDisk);

  // handle internal
  FillLiteralValueFromTarget(contentTypeHandlerNodeResource,kNC_HandleInternal, &stringValue);
  if (stringValue && trueString.Equals(stringValue))
       aMIMEInfo->SetPreferredAction(nsIMIMEInfo::handleInternally);
  
  // always ask --> these fields aren't stored in the data source anymore
  //FillLiteralValueFromTarget(contentTypeHandlerNodeResource,kNC_AlwaysAsk, &stringValue);
  //if (trueString.Equals(stringValue))
  //      aMIMEInfo->SetAlwaysAskBeforeHandling(PR_TRUE);
  // else
  //  aMIMEInfo->SetAlwaysAskBeforeHandling(PR_FALSE);


  // now digest the external application information

  nsCAutoString externalAppNodeName (NC_CONTENT_NODE_EXTERNALAPP_PREFIX);
  externalAppNodeName.Append(aContentType);
  nsCOMPtr<nsIRDFResource> externalAppNodeResource;
  aRDFService->GetResource(externalAppNodeName.get(), getter_AddRefs(externalAppNodeResource));

  if (externalAppNodeResource)
  {
    FillLiteralValueFromTarget(externalAppNodeResource, kNC_PrettyName, &stringValue);
    if (stringValue)
      aMIMEInfo->SetApplicationDescription(stringValue);
 
    FillLiteralValueFromTarget(externalAppNodeResource, kNC_Path, &stringValue);
    if (stringValue)
    {
      nsCOMPtr<nsIFile> application;
      GetFileTokenForPath(stringValue, getter_AddRefs(application));
      if (application)
        aMIMEInfo->SetPreferredApplicationHandler(application);
    }
  }

  return rv;
}

nsresult nsExternalHelperAppService::GetMIMEInfoForMimeTypeFromDS(const char * aContentType, nsIMIMEInfo ** aMIMEInfo)
{
  nsresult rv = NS_OK;

  rv = InitDataSource();
  if (NS_FAILED(rv)) return rv;

  // if we have a data source then use the information found in that...
  // if that fails....then try to the old mime service that i'm going to be
  // obsoleting soon...
  if (mOverRideDataSource)
  {
    // Get the RDF service.
    nsCOMPtr<nsIRDFService> rdf = do_GetService(kRDFServiceCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

  
    // Build uri for the mimetype resource.
    nsCString contentTypeNodeName (NC_CONTENT_NODE_PREFIX);
    nsCAutoString contentType(aContentType);
    ToLowerCase(contentType);
    contentTypeNodeName.Append(contentType);

    // Get the mime type resource.
    nsCOMPtr<nsIRDFResource> contentTypeNodeResource;
    rv = rdf->GetResource(contentTypeNodeName.get(), getter_AddRefs(contentTypeNodeResource));
    NS_ENSURE_SUCCESS(rv, rv);

    // we need a way to determine if this content type resource is really in the graph or not...
    // ...Test that there's a #value arc from the mimetype resource to the mimetype literal string.
    nsCOMPtr<nsIRDFLiteral> mimeLiteral;
    NS_ConvertUTF8toUCS2 mimeType(contentType);
    rv = rdf->GetLiteral( mimeType.get(), getter_AddRefs( mimeLiteral ) );
    NS_ENSURE_SUCCESS(rv, rv);
    
    PRBool exists = PR_FALSE;
    rv = mOverRideDataSource->HasAssertion(contentTypeNodeResource, kNC_Value, mimeLiteral, PR_TRUE, &exists );

    if (NS_SUCCEEDED(rv) && exists)
    {
       // create a mime info object and we'll fill it in based on the values from the data source
       nsCOMPtr<nsIMIMEInfo> mimeInfo (do_CreateInstance(NS_MIMEINFO_CONTRACTID, &rv));
       NS_ENSURE_SUCCESS(rv, rv);
       rv = FillTopLevelProperties(contentType.get(), contentTypeNodeResource, rdf, mimeInfo);
       NS_ENSURE_SUCCESS(rv, rv);
       rv = FillContentHandlerProperties(contentType.get(), contentTypeNodeResource, rdf, mimeInfo);

       *aMIMEInfo = mimeInfo;
       NS_IF_ADDREF(*aMIMEInfo);
    } // if we have a node in the graph for this content type
    else
      *aMIMEInfo = nsnull;
  } // if we have a data source
  else
    rv = NS_ERROR_FAILURE;
  return rv;
}

nsresult nsExternalHelperAppService::GetMIMEInfoForExtensionFromDS(const char * aFileExtension, nsIMIMEInfo ** aMIMEInfo)
{
  nsresult rv = NS_OK;
  *aMIMEInfo = nsnull;

  rv = InitDataSource();
  if (NS_FAILED(rv)) return rv;

  // if we have a data source then use the information found in that
  if (mOverRideDataSource)
  {
    // Get the RDF service.
    nsCOMPtr<nsIRDFService> rdf = do_GetService(kRDFServiceCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ConvertUTF8toUCS2 extension(aFileExtension);
    ToLowerCase(extension);
    nsCOMPtr<nsIRDFLiteral> extensionLiteral;
    rv = rdf->GetLiteral(extension.get(), getter_AddRefs( extensionLiteral));
    NS_ENSURE_SUCCESS(rv, rv);

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
      if (contentType)
        contentTypeStr.AssignWithConversion(contentType);
      if (NS_SUCCEEDED(rv))
      {
        // create a mime info object and we'll fill it in based on the values from the data source
        nsCOMPtr<nsIMIMEInfo> mimeInfo (do_CreateInstance(NS_MIMEINFO_CONTRACTID, &rv));
        NS_ENSURE_SUCCESS(rv, rv);
        rv = FillTopLevelProperties(contentTypeStr.get(), contentTypeNodeResource, rdf, mimeInfo);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = FillContentHandlerProperties(contentTypeStr.get(), contentTypeNodeResource, rdf, mimeInfo);
        
        *aMIMEInfo = mimeInfo;
        NS_IF_ADDREF(*aMIMEInfo);
      }
    }  // if we have a node in the graph for this extension
  } // if we have a data source
  else
    rv = NS_ERROR_FAILURE;
  return rv;
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

NS_IMETHODIMP nsExternalHelperAppService::LoadUrl(nsIURI * aURL)
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

  if (!mTemporaryFilesList)
    rv = NS_NewISupportsArray(getter_AddRefs(mTemporaryFilesList));

  NS_ENSURE_SUCCESS(rv, rv);

  mTemporaryFilesList->AppendElement(localFile);

  return NS_OK;
}

nsresult nsExternalHelperAppService::ExpungeTemporaryFiles()
{
  if (!mTemporaryFilesList) return NS_OK;

  PRUint32 numEntries = 0;
  mTemporaryFilesList->Count(&numEntries);
  nsCOMPtr<nsISupports> element;
  nsCOMPtr<nsILocalFile> localFile;
  for (PRUint32 index = 0; index < numEntries; index ++)
  {
    element = getter_AddRefs(mTemporaryFilesList->ElementAt(index));
    if (element)
    {
      localFile = do_QueryInterface(element);
      if (localFile)
        localFile->Remove(PR_FALSE);
    }
  }

  mTemporaryFilesList->Clear();

  return NS_OK;
}

/* XPCOM Shutdown observer */
NS_IMETHODIMP
nsExternalHelperAppService::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData )
{
  // we must be shutting down xpcom so remove our temporary files...
  ExpungeTemporaryFiles();
  nsresult rv = NS_OK;

  nsCOMPtr<nsIObserverService> obs = do_GetService("@mozilla.org/observer-service;1", &rv);
  if (obs)
    rv = obs->RemoveObserver(NS_STATIC_CAST(nsIObserver*, this), NS_XPCOM_SHUTDOWN_OBSERVER_ID);
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
   NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_THREADSAFE

nsExternalAppHandler::nsExternalAppHandler()
{
  NS_INIT_ISUPPORTS();
  mCanceled = PR_FALSE;
  mReceivedDispositionInfo = PR_FALSE;
  mHandlingAttachment = PR_FALSE;
  mStopRequestIssued = PR_FALSE;
  mDataBuffer = (char *) nsMemory::Alloc((sizeof(char) * DATA_BUFFER_SIZE));
  mProgressListenerInitialized = PR_FALSE;
  mContentLength = -1;
  mProgress      = 0;
}

nsExternalAppHandler::~nsExternalAppHandler()
{
  if (mDataBuffer)
    nsMemory::Free(mDataBuffer);
}

NS_IMETHODIMP nsExternalAppHandler::GetInterface(const nsIID & aIID, void * *aInstancePtr)
{
  NS_ENSURE_ARG_POINTER(aInstancePtr);
  return QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP nsExternalAppHandler::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData )
{
  if (PL_strcmp(aTopic, "oncancel") == 0)
  {
    // User pressed cancel button on dialog.
    return this->Cancel();
  }
  return NS_OK;
}


NS_IMETHODIMP nsExternalAppHandler::SetWebProgressListener(nsIWebProgressListener * aWebProgressListener)
{ 
  // this call back means we've succesfully brought up the 
  // progress window so set the appropriate flag, even though
  // aWebProgressListener might be null
  
  if (mReceivedDispositionInfo)
    mProgressListenerInitialized = PR_TRUE;

  // Go ahead and register the progress listener....

  if (mLoadCookie) 
  {
    nsCOMPtr<nsIWebProgress> webProgress(do_QueryInterface(mLoadCookie));

    if (webProgress) 
    {
      mWebProgressListener = aWebProgressListener;
    }
  }

  // while we were bringing up the progress dialog, we actually finished processing the
  // url. If that's the case then mStopRequestIssued will be true. We need to execute the
  // operation since we are actually done now.
  if (mStopRequestIssued && aWebProgressListener)
  {
    return ExecuteDesiredAction();
  }

  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::GetDownloadInfo(nsIURI ** aSourceUrl, PRInt64 * aTimeDownloadStarted, nsIFile ** aTarget)
{

  *aTimeDownloadStarted = mTimeDownloadStarted;

  if (mFinalFileDestination)
  {
    *aTarget = mFinalFileDestination;
  }
  else
    *aTarget = mTempFile;

  NS_IF_ADDREF(*aTarget);

  *aSourceUrl = mSourceUrl;
  NS_IF_ADDREF(*aSourceUrl);

  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::CloseProgressWindow()
{
  // make our docloader release the progress listener from the progress window...
  if (mLoadCookie && mWebProgressListener) 
  {
    nsCOMPtr<nsIWebProgress> webProgress(do_QueryInterface(mLoadCookie));

    if (webProgress) 
    {
      webProgress->RemoveProgressListener(mWebProgressListener);
    }
  }

  // release extra state...
  mWebProgressListener = nsnull;
  mLoadCookie = nsnull;
  return NS_OK;
}

void nsExternalAppHandler::ExtractSuggestedFileNameFromChannel(nsIChannel* aChannel)
{
  /*
   * If the channel is an http or part of a multipart channel and we
   * have a content disposition header set, then use the file name
   * suggested there as the preferred file name to SUGGEST to the
   * user.  we shouldn't actually use that without their
   * permission...o.t. just use our temp file
   */
  nsCAutoString disp;
  nsresult rv = NS_OK;
  // First see whether this is an http channel
  nsCOMPtr<nsIHttpChannel> httpChannel( do_QueryInterface( aChannel ) );
  if ( httpChannel ) 
  {
    rv = httpChannel->GetResponseHeader( NS_LITERAL_CSTRING("content-disposition"), disp );
  }
  if ( NS_FAILED(rv) || disp.IsEmpty() )
  {
    nsCOMPtr<nsIMultiPartChannel> multipartChannel( do_QueryInterface( aChannel ) );
    if ( multipartChannel )
    {
      rv = multipartChannel->GetContentDisposition( disp );
    }
  }
  // content-disposition: has format:
  // disposition-type < ; name=value >* < ; filename=value > < ; name=value >*
  if ( NS_SUCCEEDED( rv ) && !disp.IsEmpty() ) 
  {
    nsCAutoString::const_iterator start, end;
    disp.BeginReading(start);
    disp.EndReading(end);
    // skip leading whitespace
    while (start != end && nsCRT::IsAsciiSpace(*start)) {
      ++start;
    }
    nsCAutoString::const_iterator iter = start;
    // walk forward till we hit the next whitespace or semicolon
    while (iter != end && *iter != ';' && !nsCRT::IsAsciiSpace(*iter)) {
      ++iter;
    }
    if (start != iter &&
        Substring(start, iter).Equals(NS_LITERAL_CSTRING("attachment"),
                                      nsCaseInsensitiveCStringComparator())) {
      mHandlingAttachment = PR_TRUE;
    }

    // We may not have a disposition type listed; some servers suck.
    // But they could have listed a filename anyway.
    disp.BeginReading(start);
    iter = end;

    if (CaseInsensitiveFindInReadable(NS_LITERAL_CSTRING("filename="),
                                      start,
                                      iter))
    {
      // The value is either a string with no whitespace or a string
      // in double quotes.  See RFC 2183 and bug 66181. 

      // Search for the ';' if it's not in double quotes, then walk
      // back past any whitespace
      if (iter != end) { // otherwise our filename is empty...
        char endChar = ';';
        if (*iter == '"') {
          endChar = '"';
          ++iter; // since we had iter < end, this is not running us past the end of the string
        }
        start = iter;
        FindCharInReadable(endChar, iter, end);
        // Now start points at the beginning of the filename.  iter
        // points to just past its end if the name was quoted.  If we
        // looked for a semicolon, we need to step back past
        // whitespace.
        if (endChar == ';' && iter != start) {
          --iter;
          while (iter != start && nsCRT::IsAsciiSpace(*iter)) {
            --iter;
          }
          ++iter;
        }

        if (iter != start) { // not empty
          // ONLY if we got here, will we remember the suggested file name...
          // The filename must be ASCII, see RFC 2231
          // We ignore the filename in the header if the filename contains raw 8bit.
          // (and keep the URI filename instead).
          const nsACString& newFileName = Substring(start, iter);
          if (IsASCII(newFileName))
            CopyASCIItoUCS2(newFileName, mSuggestedFileName);

#ifdef XP_WIN
          // Make sure extension is still correct.
          EnsureSuggestedFileName();
#endif

          // replace platform specific path separator and illegal characters to avoid any confusion
          mSuggestedFileName.ReplaceChar(FILE_PATH_SEPARATOR FILE_ILLEGAL_CHARACTERS, '-');
        }
      }
    } // if we found a file name in the header disposition field
  } // we had a disp header 
}

nsresult nsExternalAppHandler::RetargetLoadNotifications(nsIRequest *request)
{
  // we are going to run the downloading of the helper app in our own little docloader / load group context. 
  // so go ahead and force the creation of a load group and doc loader for us to use...
  nsresult rv = NS_OK;
  
  nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(request);
  NS_ENSURE_TRUE(aChannel, NS_ERROR_FAILURE);

  nsCOMPtr<nsIURILoader> uriLoader(do_GetService(NS_URI_LOADER_CONTRACTID));
  NS_ENSURE_TRUE(uriLoader, NS_ERROR_FAILURE);

  nsCOMPtr<nsILoadGroup> newLoadGroup;
  nsCOMPtr<nsILoadGroup> oldLoadGroup;
  uriLoader->GetLoadGroupForContext(NS_STATIC_CAST(nsIURIContentListener*, this), getter_AddRefs(newLoadGroup));
  aChannel->GetLoadGroup(getter_AddRefs(oldLoadGroup));

  // we need to store off the original (pre redirect!) channel that initiated the load. We do
  // this so later on, we can pass any refresh urls associated with the original channel back to the 
  // window context which started the whole process. More comments about that are listed below....
  // HACK ALERT: it's pretty bogus that we are getting the document channel from the doc loader. 
  // ideally we should be able to just use mChannel (the channel we are extracting content from) or
  // the default load channel associated with the original load group. Unfortunately because
  // a redirect may have occurred, the doc loader is the only one with a ptr to the original channel 
  // which is what we really want....
  nsCOMPtr<nsIDocumentLoader> origContextLoader;
  uriLoader->GetDocumentLoaderForContext(mWindowContext, getter_AddRefs(origContextLoader));
  if (origContextLoader)
    origContextLoader->GetDocumentChannel(getter_AddRefs(mOriginalChannel));

  if(oldLoadGroup)
     oldLoadGroup->RemoveRequest(request, nsnull, NS_OK);
      
   aChannel->SetLoadGroup(newLoadGroup);
   nsCOMPtr<nsIInterfaceRequestor> req (do_QueryInterface(mLoadCookie));
   aChannel->SetNotificationCallbacks(req);
   rv = newLoadGroup->AddRequest(request, nsnull);
   return rv;
}

#define SALT_SIZE 8
#define TABLE_SIZE 36
const PRUnichar table[] = 
  { 'a','b','c','d','e','f','g','h','i','j',
    'k','l','m','n','o','p','q','r','s','t',
    'u','v','w','x','y','z','0','1','2','3',
    '4','5','6','7','8','9'};

// Make sure mSuggestedFileName has the extension indicated by mTempFileExtension.
// This is required so that the (renamed) temporary file has the correct extension
// after downloading to make sure the OS will launch the application corresponding
// to the MIME type (which was used to calculate mTempFileExtension).  This prevents
// a cgi-script named foobar.exe that returns application/zip from being named
// foobar.exe.  It also blocks content that a web site might provide with a
// content-disposition header indicating filename="foobar.exe" from being downloaded
// to a file with extension .exe.
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
    if (!fileExt.Equals(mTempFileExtension, nsCaseInsensitiveStringComparator()))
    {
      // Doesn't match, so force mSuggestedFileName to have the extension we want.
      mSuggestedFileName.Append(mTempFileExtension);
    }
  }
}

nsresult nsExternalAppHandler::SetUpTempFile(nsIChannel * aChannel)
{
  nsresult rv = NS_OK;

#if defined(XP_MAC) || defined (XP_MACOSX)
 // create a temp file for the data...and open it for writing.
 // use NS_MAC_DEFAULT_DOWNLOAD_DIR which gets download folder from InternetConfig
 // if it can't get download folder pref, then it uses desktop folder
  NS_GetSpecialDirectory(NS_MAC_DEFAULT_DOWNLOAD_DIR, getter_AddRefs(mTempFile));
#else
  // create a temp file for the data...and open it for writing.
  NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(mTempFile));
#endif

  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(mSourceUrl));
  nsCOMPtr<nsIURL> url = do_QueryInterface(mSourceUrl);

  // We need to do two things here, (1) extract the file name that's part of the url
  // and store this is as mSuggestedfileName. This way, when we show the user a file picker, 
  // we can have it pre filled with the suggested file name. 
  // (2) We need to generate a name for the temp file that we are going to be streaming data to. 
  // We don't want this name to be predictable for security reasons so we are going to generate a 
  // "salted" name.....

  if (url)
  {
    // try to extract the file name from the url and use that as a first pass as the
    // leaf name of our temp file...
    nsCAutoString leafName; // may be shortened by NS_UnescapeURL
    url->GetFileName(leafName);
    if (!leafName.IsEmpty())
    {

      nsCOMPtr<nsITextToSubURI> textToSubURI = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
      if (NS_SUCCEEDED(rv))
      {
        nsCAutoString originCharset;
        url->GetOriginCharset(originCharset);
        rv = textToSubURI->UnEscapeURIForUI(originCharset, leafName, 
                                            mSuggestedFileName);
      }

      if (NS_FAILED(rv))
      {
        mSuggestedFileName = NS_ConvertUTF8toUCS2(leafName); // use escaped name
        rv = NS_OK;
      }

#ifdef XP_WIN
      // Make sure extension is still correct.
      EnsureSuggestedFileName();
#endif

      // replace platform specific path separator and illegal characters to avoid any confusion
      mSuggestedFileName.ReplaceChar(FILE_PATH_SEPARATOR FILE_ILLEGAL_CHARACTERS, '-');
    }
  }

  // step (2), generate a salted file name for the temp file....
  nsAutoString saltedTempLeafName;
  // this salting code was ripped directly from the profile manager.
  // turn PR_Now() into milliseconds since epoch 1058 // and salt rand with that. 
  double fpTime;
  LL_L2D(fpTime, PR_Now());
  srand((uint)(fpTime * 1e-6 + 0.5));
  PRInt32 i;
  for (i=0;i<SALT_SIZE;i++) 
  {
    saltedTempLeafName.Append(table[(rand()%TABLE_SIZE)]);
  }

  // now append our extension.
  saltedTempLeafName.Append(mTempFileExtension);

  mTempFile->Append(saltedTempLeafName); // make this file unique!!!
  mTempFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);

#if defined(XP_MAC) || defined (XP_MACOSX)
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

  NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
  nsCOMPtr<nsIFileTransportService> fts = 
           do_GetService(kFileTransportServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsITransport> fileTransport;
  rv = fts->CreateTransport(mTempFile,
                            PR_WRONLY | PR_CREATE_FILE,
                            0600,
                            PR_TRUE,
                            getter_AddRefs(fileTransport));
  if (NS_FAILED(rv)) return rv;

  rv = fileTransport->OpenOutputStream(0, PRUint32(-1), 0, getter_AddRefs(mOutStream));  

#ifdef XP_MAC
    nsXPIDLCString contentType;
    mMimeInfo->GetMIMEType(getter_Copies(contentType));
    if (contentType &&
      (nsCRT::strcasecmp(contentType, APPLICATION_APPLEFILE) == 0) ||
      (nsCRT::strcasecmp(contentType, MULTIPART_APPLEDOUBLE) == 0))
    {
      nsCOMPtr<nsIAppleFileDecoder> appleFileDecoder = do_CreateInstance(NS_IAPPLEFILEDECODER_CONTRACTID, &rv);
      if (NS_SUCCEEDED(rv) && appleFileDecoder)
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
  NS_ENSURE_ARG(request);

  // first, check to see if we've been canceled....
  if (mCanceled) // then go cancel our underlying channel too
    return request->Cancel(NS_BINDING_ABORTED);

  nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(request);

  nsresult rv = SetUpTempFile(aChannel);
  
  // Get content length.
  if ( aChannel )
  {
    aChannel->GetContentLength( &mContentLength );
  }
  
  // retarget all load notifcations to our docloader instead of the original window's docloader...
  RetargetLoadNotifications(request);
  // ignore failure...
  ExtractSuggestedFileNameFromChannel(aChannel); 
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface( aChannel );
  if ( httpChannel ) 
  {
    // Turn off content encoding conversions if needed
    PRBool applyConversion = PR_TRUE;
    
    nsCOMPtr<nsIExternalHelperAppService> extHandler = do_GetService("@mozilla.org/uriloader/external-helper-app-service;1");
    if (extHandler) {
      nsXPIDLCString MIMEType;
      mMimeInfo->GetMIMEType( getter_Copies( MIMEType ) );
      extHandler->ApplyDecodingForType(MIMEType, &applyConversion);
      
      if (applyConversion) {
        // Now we double-check that it's OK to decode this extension
        nsCOMPtr<nsIURI> channelURI;
        aChannel->GetURI(getter_AddRefs(channelURI));
        nsCOMPtr<nsIURL> channelURL(do_QueryInterface(channelURI));
        nsCAutoString extension;
        if (channelURL) {
          channelURL->GetFileExtension(extension);
          if (!extension.IsEmpty())
            extHandler->ApplyDecodingForExtension(extension.get(), &applyConversion);
        }
      }
    }
    
    nsCOMPtr<nsIEncodedChannel> encodedChannel(do_QueryInterface(httpChannel));
    NS_ENSURE_TRUE(encodedChannel, NS_ERROR_UNEXPECTED);
    encodedChannel->SetApplyConversion( applyConversion );
  }

  mTimeDownloadStarted = PR_Now();

  // now that the temp file is set up, find out if we need to invoke a dialog asking the user what
  // they want us to do with this content...

  PRBool alwaysAsk = PR_TRUE;
  // If we're handling an attachment we want to default to saving but
  // always ask just in case
  if (mHandlingAttachment) {
    mMimeInfo->SetPreferredAction(nsIMIMEInfo::saveToDisk);
  } else {
    mMimeInfo->GetAlwaysAskBeforeHandling(&alwaysAsk);
  }
  if (alwaysAsk)
  {
    // do this first! make sure we don't try to take an action until the user tells us what they want to do
    // with it...
    mReceivedDispositionInfo = PR_FALSE; 

    // invoke the dialog!!!!! use mWindowContext as the window context parameter for the dialog request
    mDialog = do_CreateInstance( NS_IHELPERAPPLAUNCHERDLG_CONTRACTID, &rv );
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDialog->Show( this, mWindowContext );

    // what do we do if the dialog failed? I guess we should call Cancel and abort the load....
  }
  else
  {
    mReceivedDispositionInfo = PR_TRUE; // no need to wait for a response from the user

    // We need to do the save/open immediately, then.
    PRInt32 action = nsIMIMEInfo::saveToDisk;
    mMimeInfo->GetPreferredAction( &action );
#ifdef XP_WIN
    /* We need to see whether the file we've got here could be
     * executable.  If it could, we had better not try to open it!
     * This code mirrors the code in
     * nsExternalAppHandler::LaunchWithApplication so that what we
     * test here is as close as possible to what will really be
     * happening if we decide to execute
     */
    nsCOMPtr<nsIFile> fileToTest;
    nsCOMPtr<nsIURI> garbageURI;
    PRInt64 garbageTimestamp;
    GetDownloadInfo(getter_AddRefs(garbageURI), &garbageTimestamp,
                    getter_AddRefs(fileToTest));
    if (fileToTest) {
      PRBool isExecutable;
      rv = fileToTest->IsExecutable(&isExecutable);
      if ((NS_SUCCEEDED(rv) && isExecutable) ||
          NS_FAILED(rv)) {  // Paranoia is good
        action = nsIMIMEInfo::saveToDisk;
      }
    } else {   // Paranoia is good here too, though this really should not happen
      NS_WARNING("GetDownloadInfo returned a null file after the temp file has been set up! ");
      action = nsIMIMEInfo::saveToDisk;
    }

#endif
    if ( action == nsIMIMEInfo::saveToDisk )
    {
        rv = SaveToDisk(nsnull, PR_FALSE);
    }
    else
    {
        rv = LaunchWithApplication(nsnull, PR_FALSE);
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
    case NS_ERROR_FILE_DISK_FULL:
    case NS_ERROR_FILE_NO_DEVICE_SPACE:
        // Out of space on target volume.
        msgId = NS_LITERAL_STRING("diskFull");
        break;

    case NS_ERROR_FILE_READ_ONLY:
        // Attempt to write to read/only file.
        msgId = NS_LITERAL_STRING("readOnly");
        break;

    case NS_ERROR_FILE_ACCESS_DENIED:
        // Attempt to write without sufficient permissions.
        msgId = NS_LITERAL_STRING("accessError");
        break;

    default:
        // Generic read/write/launch error message.
        switch(type)
        {
        case kReadError:
          msgId = NS_LITERAL_STRING("readError");
          break;
        case kWriteError:
          msgId = NS_LITERAL_STRING("writeError");
          break;
        case kLaunchError:
          msgId = NS_LITERAL_STRING("launchError");
          break;
        }
        break;
    }
#ifdef DEBUG_law
printf( "\nError: %s, listener=0x%08X, rv=0x%08X\n\n", NS_LossyConvertUCS2toASCII(msgId).get(), (int)(void*)mWebProgressListener.get(), (int)rv );
#endif
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
                nsCOMPtr<nsIPromptService> prompter(do_GetService("@mozilla.org/embedcomp/prompt-service;1"));
                nsXPIDLString title;
                bundle->FormatStringFromName(NS_LITERAL_STRING("title").get(),
                                             strings,
                                             1,
                                             getter_Copies(title));
                if (prompter)
                {
                  // Extract parent window from context.
                  nsCOMPtr<nsIDOMWindow> parent(do_GetInterface(mWindowContext));
                  prompter->Alert(parent, title, msgText);
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
  if (mOutStream && mDataBuffer && count > 0)
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
      // Set content length if we haven't already got it.
      if (mContentLength == -1)
      {
        nsCOMPtr<nsIChannel> aChannel(do_QueryInterface(request));
        if (aChannel)
        {
          aChannel->GetContentLength(&mContentLength);
        }
      }
      // Send progress notification.
      if (mWebProgressListener)
      {
        mWebProgressListener->OnProgressChange(nsnull, request, mProgress, mContentLength, mProgress, mContentLength);
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
      Cancel();
    }
  }
  return rv;
}

NS_IMETHODIMP nsExternalAppHandler::OnStopRequest(nsIRequest *request, nsISupports *aCtxt, 
                                                  nsresult aStatus)
{
  mStopRequestIssued = PR_TRUE;

  // Cancel if the request did not complete successfully.
  if (!mCanceled && NS_FAILED(aStatus))
  {
    // Send error notification.
    nsAutoString tempFilePath;
    if (mTempFile)
      mTempFile->GetPath(tempFilePath);
    SendStatusChange( kReadError, aStatus, request, tempFilePath );

    Cancel();
  }

  // first, check to see if we've been canceled....
  if (mCanceled) { // then go cancel our underlying channel too
    nsresult rv = request->Cancel(NS_BINDING_ABORTED);
    return rv;
  }

  // go ahead and execute the application passing in our temp file as an argument
  // this may involve us calling back into the OS external app service to make the call
  // for actually launching the helper app. It'd be great if nsIFile::spawn could be made to work
  // on the mac...right now the mac implementation ignores all arguments passed in.

  // close the stream...
  if (mOutStream)
  {
    mOutStream->Close();
    mOutStream = nsnull;
  }

  return ExecuteDesiredAction();
}

nsresult nsExternalAppHandler::ExecuteDesiredAction()
{
  nsresult rv = NS_OK;
  if (mProgressListenerInitialized && !mCanceled)
  {
    nsMIMEInfoHandleAction action = nsIMIMEInfo::saveToDisk;
    mMimeInfo->GetPreferredAction(&action);
    if (action == nsIMIMEInfo::saveToDisk)
      // XXX Put progress dialog in barber-pole mode
      //     and change text to say "Copying from:".
      rv = MoveFile(mFinalFileDestination);
    else
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
          rv = OpenWithApplication(nsnull);
      }
    }

    // Notify dialog that download is complete.
    // By waiting till this point, it ensures that the progress dialog doesn't indicate
    // success until we're really done.
    if(mWebProgressListener)
    {
      if (!mCanceled)
      {
        mWebProgressListener->OnProgressChange(nsnull, nsnull, mContentLength, mContentLength, mContentLength, mContentLength);
      }
      mWebProgressListener->OnStateChange(nsnull, nsnull, nsIWebProgressListener::STATE_STOP, NS_OK);
    }
  }
  
  return rv;
}

nsresult nsExternalAppHandler::Init(nsIMIMEInfo * aMIMEInfo, const char * aTempFileExtension, nsISupports * aWindowContext)
{
  mWindowContext = aWindowContext;
  mMimeInfo = aMIMEInfo;
  
  // make sure the extention includes the '.'
  if (aTempFileExtension && *aTempFileExtension != '.')
    mTempFileExtension = PRUnichar('.');
  mTempFileExtension.AppendWithConversion(aTempFileExtension);

  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::GetMIMEInfo(nsIMIMEInfo ** aMIMEInfo)
{
  *aMIMEInfo = mMimeInfo;
  NS_IF_ADDREF(*aMIMEInfo);
  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::GetSource(nsIURI ** aSourceURI)
{
  NS_ENSURE_ARG(aSourceURI);
  *aSourceURI = mSourceUrl;
  NS_IF_ADDREF(*aSourceURI);
  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::GetSuggestedFileName(PRUnichar ** aSuggestedFileName)
{
  *aSuggestedFileName = ToNewUnicode(mSuggestedFileName);
  return NS_OK;
}

nsresult nsExternalAppHandler::InitializeDownload(nsIDownload* aDownload)
{
  nsresult rv;
  
  nsCOMPtr<nsILocalFile> local = do_QueryInterface(mFinalFileDestination);
  
  rv = aDownload->Init(mSourceUrl, local, nsnull,
                       mMimeInfo, mTimeDownloadStarted, nsnull);
  if (NS_FAILED(rv)) return rv;
  
  rv = aDownload->SetObserver(this);

  return rv;
}

nsresult nsExternalAppHandler::CreateProgressListener()
{
  // we are back from the helper app dialog (where the user chooses to save or open), but we aren't
  // done processing the load. in this case, throw up a progress dialog so the user can see what's going on...
  nsresult rv;

  nsCOMPtr<nsIWebProgressListener> listener;
  
  nsCOMPtr<nsIDownload> dl = do_CreateInstance("@mozilla.org/download;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    InitializeDownload(dl);
    listener = do_QueryInterface(dl);
  }

  // note we might not have a listener here if the QI() failed, or if
  // there is no nsIDownload object, but we still call
  // SetWebProgressListener() to make sure our progress state is sane
  SetWebProgressListener(listener);

  return rv;
}

nsresult nsExternalAppHandler::PromptForSaveToFile(nsILocalFile ** aNewFile, const nsAFlatString &aDefaultFile, const nsAFlatString &aFileExtension)
{
  // invoke the dialog!!!!! use mWindowContext as the window context parameter for the dialog request
  // XXX Convert to use file picker?
  nsresult rv = NS_OK;
  if (!mDialog)
  {
    // Get helper app launcher dialog.
    mDialog = do_CreateInstance( NS_IHELPERAPPLAUNCHERDLG_CONTRACTID, &rv );
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // we want to explicitly unescape aDefaultFile b4 passing into the dialog. we can't unescape
  // it because the dialog is implemented by a JS component which doesn't have a window so no unescape routine is defined...
  
  rv = mDialog->PromptForSaveToFile(mWindowContext,
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
     nsCAutoString fileName;
     fileToUse->GetNativeLeafName(fileName);
     nsCOMPtr<nsIFile> directoryLocation;
     fileToUse->GetParent(getter_AddRefs(directoryLocation));
     if (directoryLocation)
     {
       rv = mTempFile->MoveToNative(directoryLocation, fileName);
     }
     if (NS_FAILED(rv))
     {
       // Send error notification.        
       nsAutoString path;
       fileToUse->GetPath(path);
       SendStatusChange(kWriteError, rv, nsnull, path);
       Cancel(); // Cancel (and clean up temp file).
     }
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

  if (!aNewFileLocation)
  {
    nsAutoString leafName;
    nsCOMPtr<nsILocalFile> fileToUse;
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

    if (NS_FAILED(rv) || !fileToUse) 
      return Cancel();
    
    mFinalFileDestination = do_QueryInterface(fileToUse);

    if (!mProgressListenerInitialized)
      CreateProgressListener();

    // now that the user has chosen the file location to save to, it's okay to fire the refresh tag
    // if there is one. We don't want to do this before the save as dialog goes away because this dialog
    // is modal and we do bad things if you try to load a web page in the underlying window while a modal
    // dialog is still up. 
    ProcessAnyRefreshTags();
  }

  return rv;
}


nsresult nsExternalAppHandler::OpenWithApplication(nsIFile * aApplication)
{
  nsresult rv = NS_OK;
  if (mCanceled)
    return NS_OK;
  
  // we only should have gotten here if the on stop request had been fired already.

  NS_ASSERTION(mStopRequestIssued, "uhoh, how did we get here if we aren't done getting data?");
  // if a stop request was already issued then proceed with launching the application.
  if (mStopRequestIssued)
  {
    nsCOMPtr<nsPIExternalAppLauncher> helperAppService (do_GetService(NS_EXTERNALHELPERAPPSERVICE_CONTRACTID));
    if (helperAppService)
    {
      rv = helperAppService->LaunchAppWithTempFile(mMimeInfo, mFinalFileDestination);
      if (NS_FAILED(rv))
      {
        // Send error notification.
        nsAutoString path;
        mFinalFileDestination->GetPath(path);
        SendStatusChange(kLaunchError, rv, nsnull, path);
        Cancel(); // Cancel, and clean up temp file.
      }
      else
      {
#if !defined(XP_MAC) && !defined (XP_MACOSX)
      // Mac users have been very verbal about temp files being deleted on app exit - they
      // don't like it - but we'll continue to do this on other platforms for now
      helperAppService->DeleteTemporaryFileOnExit(mFinalFileDestination);
#endif
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

  mMimeInfo->SetPreferredAction(nsIMIMEInfo::useHelperApp);

  // user has chosen to launch using an application, fire any refresh tags now...
  ProcessAnyRefreshTags(); 
  
  mReceivedDispositionInfo = PR_TRUE; 
  if (mMimeInfo && aApplication)
    mMimeInfo->SetPreferredApplicationHandler(aApplication);

  // Now that the user has elected to launch the downloaded file with a helper app, we're justified in
  // removing the 'salted' name.  We'll rename to what was specified in mSuggestedFileName after the
  // download is done prior to launching the helper app.  So that any existing file of that name won't
  // be overwritten we call CreateUnique() before calling MoveFile().  Also note that we use the same
  // directory as originally downloaded to so that MoveFile() just does an in place rename.
   
  nsCOMPtr<nsIFile> fileToUse;
  
  // The directories specified here must match those specified in SetUpTempFile().  This avoids
  // having to do a copy of the file when it finishes downloading and the potential for errors
  // that would introduce
#if defined(XP_MAC) || defined (XP_MACOSX)
  NS_GetSpecialDirectory(NS_MAC_DEFAULT_DOWNLOAD_DIR, getter_AddRefs(fileToUse));
#else
  NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(fileToUse));
#endif

  if (mSuggestedFileName.IsEmpty())
  {
    // Keep using the leafname of the temp file, since we're just starting a helper
    mTempFile->GetLeafName(mSuggestedFileName);
  }

  fileToUse->Append(mSuggestedFileName);
  // We'll make sure this results in a unique name later

  mFinalFileDestination = do_QueryInterface(fileToUse);

  // launch the progress window now that the user has picked the desired action.
  if (!mProgressListenerInitialized)
   CreateProgressListener();

  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::Cancel()
{
  mCanceled = PR_TRUE;
  // shutdown our stream to the temp file
  if (mOutStream)
  {
    mOutStream->Close();
    mOutStream = nsnull;
  }

  // clean up after ourselves and delete the temp file...
  if (mTempFile)
  {
    mTempFile->Remove(PR_TRUE);
    mTempFile = nsnull;
  }

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
     if (refreshHandler)
        refreshHandler->SetupRefreshURI(mOriginalChannel);
     mOriginalChannel = nsnull;
   }
}

// nsIURIContentListener implementation
NS_IMETHODIMP
nsExternalAppHandler::OnStartURIOpen(nsIURI* aURI, PRBool* aAbortOpen)
{
  return NS_OK;
}

NS_IMETHODIMP
nsExternalAppHandler::IsPreferred(const char * aContentType,
                                  char ** aDesiredContentType,
                                  PRBool * aCanHandleContent)

{
  NS_NOTREACHED("IsPreferred");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsExternalAppHandler::CanHandleContent(const char * aContentType,
                                       PRBool aIsContentPreferred,
                                       char ** aDesiredContentType,
                                       PRBool * aCanHandleContent)

{
  NS_NOTREACHED("CanHandleContent");
  return NS_ERROR_NOT_IMPLEMENTED;
} 

NS_IMETHODIMP
nsExternalAppHandler::DoContent(const char * aContentType,
                                PRBool aIsContentPreferred,
                                nsIRequest * aRequest,
                                nsIStreamListener ** aContentHandler,
                                PRBool * aAbortProcess)
{
  NS_NOTREACHED("DoContent");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsExternalAppHandler::GetParentContentListener(nsIURIContentListener** aParent)
{
  *aParent = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsExternalAppHandler::SetParentContentListener(nsIURIContentListener* aParent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsExternalAppHandler::GetLoadCookie(nsISupports ** aLoadCookie)
{
  *aLoadCookie = mLoadCookie;
  NS_IF_ADDREF(*aLoadCookie);
  return NS_OK;
}

NS_IMETHODIMP
nsExternalAppHandler::SetLoadCookie(nsISupports * aLoadCookie)
{
  mLoadCookie = aLoadCookie;
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The following section contains our nsIMIMEService implementation and related methods.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

// nsIMIMEService methods
NS_IMETHODIMP nsExternalHelperAppService::GetFromExtension(const char *aFileExt, nsIMIMEInfo **_retval) 
{
  nsresult rv = NS_OK;
  nsCAutoString fileExt(aFileExt);
  if (fileExt.IsEmpty()) return NS_ERROR_FAILURE;

  ToLowerCase(fileExt);
  // if the file extension contains a '.', our hash key doesn't include the '.'
  // so skip over it...
  if (fileExt.First() == '.') 
    fileExt.Cut(0, 1); // cut the '.'
 
  nsCStringKey key(fileExt.get());

  nsIMIMEInfo *cachedInfo = (nsIMIMEInfo *) mMimeInfoCache->Get(&key);
  if (cachedInfo) {
    cachedInfo->Clone(_retval);
  }

  // if we don't have a match in our hash table, then query the user provided
  // data source
  if (!*_retval) 
    rv = GetMIMEInfoForExtensionFromDS(aFileExt, _retval);

  // if we still don't have a match, then we give up, we don't know
  // anything about it...  return an error.

  if (!*_retval) rv = NS_ERROR_FAILURE;
  return rv;
}

NS_IMETHODIMP nsExternalHelperAppService::GetFromMIMEType(const char *aMIMEType, nsIMIMEInfo **_retval) 
{
  nsresult rv = NS_OK;
  nsCAutoString MIMEType(aMIMEType);
  ToLowerCase(MIMEType);

  nsCStringKey key(MIMEType.get());

  nsIMIMEInfo *cachedInfo = (nsIMIMEInfo *) mMimeInfoCache->Get(&key);
  if (cachedInfo) {
    cachedInfo->Clone(_retval);
  }

  // if we don't have a match in our hash table, then query the user provided
  // data source containing additional content types...
  if (!*_retval) 
    rv = GetMIMEInfoForMimeTypeFromDS(aMIMEType, _retval);

  // if we still don't have a match, then we give up, we don't know anything about it...
  // return an error. 

  if (!*_retval) rv = NS_ERROR_FAILURE;
  return rv;
}

NS_IMETHODIMP nsExternalHelperAppService::GetTypeFromExtension(const char *aFileExt, char **aContentType) 
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMIMEInfo> info;
  rv = GetFromExtension(aFileExt, getter_AddRefs(info));
  if (NS_FAILED(rv)) {
    /* Try the plugins */
    const char* mimeType;
    nsCOMPtr<nsIPluginHost> pluginHost (do_GetService(kPluginManagerCID, &rv));
    if (NS_SUCCEEDED(rv)) {
      if (pluginHost->IsPluginEnabledForExtension(aFileExt, mimeType) == NS_OK)
      {
        *aContentType = nsCRT::strdup(mimeType);
        rv = NS_OK;
        return rv;
      }
      else 
      {
        rv = NS_ERROR_FAILURE;
      }
    }
  }
  if (NS_FAILED(rv)) {
    rv = GetMIMEInfoForExtensionFromExtras(aFileExt, getter_AddRefs(info));
  }
  if (NS_FAILED(rv)) {
    return rv;
  } /* endif */

  return info->GetMIMEType(aContentType);
}

NS_IMETHODIMP nsExternalHelperAppService::GetTypeFromURI(nsIURI *aURI, char **aContentType) 
{
  NS_PRECONDITION(aContentType, "Null out param!");
  nsresult rv = NS_ERROR_NOT_AVAILABLE;
  *aContentType = nsnull;

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
    if (NS_FAILED(rv)) return rv;
    if (ext.IsEmpty()) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    return GetTypeFromExtension(ext.get(), aContentType);
  }
    
  // no url, let's give the raw spec a shot
  nsCAutoString specStr;
  rv = aURI->GetSpec(specStr);
  if (NS_FAILED(rv)) return rv;

  // find the file extension (if any)
  PRInt32 extLoc = specStr.RFindChar('.');
  if (-1 != extLoc &&
      extLoc != specStr.Length() - 1 &&
      // nothing over 20 chars long can be sanely considered an
      // extension.... Dat dere would be just data.
      specStr.Length() - extLoc < 20) 
  {
    return GetTypeFromExtension(PromiseFlatCString(
             Substring(specStr, extLoc, specStr.Length() - extLoc - 1)
           ).get(), aContentType);
  }

  // We found no information; say so.
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP nsExternalHelperAppService::GetTypeFromFile( nsIFile* aFile, char **aContentType )
{
  nsresult rv;
  nsCOMPtr<nsIMIMEInfo> info;
	
  // Get the Extension
  nsCAutoString fileName;
  const char* ext = nsnull;
  rv = aFile->GetNativeLeafName(fileName);
  if (NS_FAILED(rv)) return rv;
 
  if (!fileName.IsEmpty())
  {
    PRInt32 len = fileName.Length(); 
    for (PRInt32 i = len; i >= 0; i--) 
    {
      if (fileName[i] == '.') 
      {
        ext = fileName.get() + i + 1;
        break;
      }
    }
  }
  
  nsCString fileExt( ext );       
  // Handle the mac case
#if defined(XP_MAC) || defined (XP_MACOSX)
  nsCOMPtr<nsILocalFileMac> macFile;
  macFile = do_QueryInterface( aFile, &rv );
  if ( NS_SUCCEEDED( rv ) && fileExt.IsEmpty())
  {
	PRUint32 type, creator;
	macFile->GetFileType( (OSType*)&type );
	macFile->GetFileCreator( (OSType*)&creator );   
  	nsCOMPtr<nsIInternetConfigService> icService (do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID));
    if (icService)
    {
      rv = icService->GetMIMEInfoFromTypeCreator(type, creator, fileExt.get(), getter_AddRefs(info));		 							
      if ( NS_SUCCEEDED( rv) )
	    return info->GetMIMEType(aContentType);
	}
  }
#endif
  // Windows, unix and mac when no type match occured.   
  if (fileExt.IsEmpty())
	  return NS_ERROR_FAILURE;    
  return GetTypeFromExtension( fileExt.get(), aContentType );
}

nsresult nsExternalHelperAppService::GetMIMEInfoForMimeTypeFromExtras(const char * aContentType, nsIMIMEInfo ** aMIMEInfo )
{
  nsresult rv = NS_OK;
  NS_ENSURE_ARG( aMIMEInfo );

  *aMIMEInfo = 0;

  // Look for default entry with matching mime type.
  nsCAutoString MIMEType(aContentType);
  ToLowerCase(MIMEType);
  PRInt32 numEntries = sizeof(extraMimeEntries) / sizeof(extraMimeEntries[0]);
  for (PRInt32 index = 0; !*aMIMEInfo && index < numEntries; index++)
  {
      if ( MIMEType.Equals(extraMimeEntries[index].mMimeType) ) {
          // This is the one.  Create MIMEInfo object and set attributes appropriately.
          nsCOMPtr<nsIMIMEInfo> mimeInfo (do_CreateInstance(NS_MIMEINFO_CONTRACTID,&rv));
          NS_ENSURE_SUCCESS( rv, rv );

          mimeInfo->SetFileExtensions(extraMimeEntries[index].mFileExtensions);
          mimeInfo->SetMIMEType(extraMimeEntries[index].mMimeType);
          mimeInfo->SetDescription(NS_ConvertASCIItoUCS2(extraMimeEntries[index].mDescription).get());
          mimeInfo->SetMacType(extraMimeEntries[index].mMactype);
          mimeInfo->SetMacCreator(extraMimeEntries[index].mMacCreator);

          *aMIMEInfo = mimeInfo;
          NS_ADDREF(*aMIMEInfo);
      }
  }

  return rv;
}

nsresult nsExternalHelperAppService::GetMIMEInfoForExtensionFromExtras(const char* aExtension, nsIMIMEInfo ** aMIMEInfo )
{
  nsresult rv = NS_ERROR_NOT_AVAILABLE;
  NS_ENSURE_ARG( aMIMEInfo );
  
  *aMIMEInfo = 0;

  // Look for default entry with matching extension.
  nsDependentCString extension(aExtension);
  nsDependentCString::const_iterator start, end, iter;
  PRInt32 numEntries = sizeof(extraMimeEntries) / sizeof(extraMimeEntries[0]);
  for (PRInt32 index = 0; !*aMIMEInfo && index < numEntries; index++)
  {
      nsDependentCString extList(extraMimeEntries[index].mFileExtensions);
      extList.BeginReading(start);
      extList.EndReading(end);
      iter = start;
      while (start != end)
      {
          FindCharInReadable(',', iter, end);
          if (Substring(start, iter).Equals(extension,
                                            nsCaseInsensitiveCStringComparator()))
          {
              // This is the one.  Create MIMEInfo object and set
              // attributes appropriately.
              nsCOMPtr<nsIMIMEInfo> mimeInfo (do_CreateInstance(NS_MIMEINFO_CONTRACTID,&rv));
              NS_ENSURE_SUCCESS( rv, rv );

              mimeInfo->SetFileExtensions(extraMimeEntries[index].mFileExtensions);
              mimeInfo->SetMIMEType(extraMimeEntries[index].mMimeType);
              mimeInfo->SetDescription(NS_ConvertASCIItoUCS2(extraMimeEntries[index].mDescription).get());
              mimeInfo->SetMacType(extraMimeEntries[index].mMactype);
              mimeInfo->SetMacCreator(extraMimeEntries[index].mMacCreator);
              *aMIMEInfo = mimeInfo;
              NS_ADDREF(*aMIMEInfo);
              break;
          }
          if (iter != end) {
            ++iter;
          }
          start = iter;
      }
  }

  return rv;
}

nsresult nsExternalHelperAppService::AddDefaultMimeTypesToCache()
{
  PRInt32 numEntries = sizeof(defaultMimeEntries) / sizeof(defaultMimeEntries[0]);
  for (PRInt32 index = 0; index < numEntries; index++)
  {
    // create a mime info object for each default mime entry and add it to our cache
    nsCOMPtr<nsIMIMEInfo> mimeInfo (do_CreateInstance(NS_MIMEINFO_CONTRACTID));
    mimeInfo->SetFileExtensions(defaultMimeEntries[index].mFileExtensions);
    mimeInfo->SetMIMEType(defaultMimeEntries[index].mMimeType);
    mimeInfo->SetDescription(NS_ConvertASCIItoUCS2(defaultMimeEntries[index].mDescription).get());
    mimeInfo->SetMacType(defaultMimeEntries[index].mMactype);
    mimeInfo->SetMacCreator(defaultMimeEntries[index].mMacCreator);
    AddMimeInfoToCache(mimeInfo);
  }

  return NS_OK;
}

nsresult nsExternalHelperAppService::AddMimeInfoToCache(nsIMIMEInfo * aMIMEInfo)
{
  NS_ENSURE_ARG(aMIMEInfo);
  nsresult rv = NS_OK;

  // Next add the new root MIME mapping.
  nsXPIDLCString mimeType;
  nsIMIMEInfo* oldInfo;
  rv = aMIMEInfo->GetMIMEType(getter_Copies(mimeType));
  
  if (NS_SUCCEEDED(rv)) {
    nsCStringKey key(mimeType);
    oldInfo = (nsIMIMEInfo*)mMimeInfoCache->Put(&key, aMIMEInfo);

    // release the old info to prevent a leak
    NS_IF_RELEASE(oldInfo);
    // add a reference for the hash table entry....
    NS_ADDREF(aMIMEInfo); 
  }

  // now we need to add entries for each file extension 
  char** extensions = nsnull;
  PRUint32 count = 0;
  rv = aMIMEInfo->GetFileExtensions(&count, &extensions );
  if (NS_FAILED(rv) || !extensions)
    return NS_OK; 

  for ( PRUint32 i = 0; i < count; i++ )
  {
     nsCStringKey key(extensions[i]);
     oldInfo = (nsIMIMEInfo*) mMimeInfoCache->Put(&key, aMIMEInfo);
     NS_IF_RELEASE(oldInfo); // release the old info to prevent a leak
     NS_ADDREF(aMIMEInfo); // addref this new entry in the table
     nsMemory::Free( extensions[i] );
  }
  nsMemory::Free( extensions ); 

  return NS_OK;
}

// static helper function to help us release hash table entries...
static PRBool DeleteEntry(nsHashKey *aKey, void *aData, void* closure) 
{
  nsIMIMEInfo *entry = (nsIMIMEInfo*) aData;
	NS_RELEASE(entry);
  return PR_TRUE;   
};

