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
 */

#include "nsExternalHelperAppService.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIFile.h"
#include "nsIChannel.h"
#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsXPIDLString.h"
#include "nsMemory.h"
#include "nsIStreamListener.h"
#include "nsIMIMEService.h"

// used to manage our in memory data source of helper applications
#include "nsRDFCID.h"
#include "rdf.h"
#include "nsIRDFService.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIFileSpec.h"
#include "nsHelperAppRDF.h"
#include "nsIMIMEInfo.h"
#include "nsDirectoryServiceDefs.h"

#include "nsIHelperAppLauncherDialog.h"

#include "nsCExternalHandlerService.h" // contains contractids for the helper app service

#include "nsMimeTypes.h"
// used for http content header disposition information.
#include "nsIHTTPChannel.h"
#include "nsIAtom.h"

#ifdef XP_MAC
#include "nsILocalFileMac.h"
#include "nsIInternetConfigService.h"
#endif // XP_MAC

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID, NS_RDFXMLDATASOURCE_CID);

// forward declaration of a private helper function
static PRBool DeleteEntry(nsHashKey *aKey, void *aData, void* closure);

// The following static table lists all of the "default" content type mappings we are going to use.                 
static nsDefaultMimeTypeEntry defaultMimeEntries [] = 
{
  { TEXT_PLAIN, "txt,text", "Text File", 'TEXT', 'ttxt' },
#if defined(VMS)
  { APPLICATION_OCTET_STREAM, "exe,bin,sav,bck,pcsi,dcx_axpexe,dcx_vaxexe,sfx_axpexe,sfx_vaxexe", "Binary Executable", PRUint32(0x3F3F3F3F), PRUint32(0x3F3F3F3F) },
#else
  { APPLICATION_OCTET_STREAM, "exe,bin", "Binary Executable", PRUint32(0x3F3F3F3F), PRUint32(0x3F3F3F3F) },
#endif


  { TEXT_HTML, "htm,html,shtml,ehtml", "Hyper Text Markup Language", PRUint32(0x3F3F3F3F), PRUint32(0x3F3F3F3F) },
  { TEXT_RDF, "rdf", "Resource Description Framework", 'TEXT','ttxt' },
  { TEXT_XUL, "xul", "XML-Based User Interface Language", 'TEXT', 'ttxt' },
  { TEXT_XML, "xml,xsl", "Extensible Markup Language", 'TEXT', 'ttxt' },
  { TEXT_CSS, "css", "Style Sheet", 'TEXT', 'ttxt' },
  { APPLICATION_JAVASCRIPT, "js", "Javascript Source File", 'TEXT', 'ttxt' },
  { MESSAGE_RFC822, "eml", "RFC-822 data", PRUint32(0x3F3F3F3F), PRUint32(0x3F3F3F3F) },
  { APPLICATION_GZIP2, "gz", "gzip", PRUint32(0x3F3F3F3F), PRUint32(0x3F3F3F3F) },
  { IMAGE_GIF, "gif", "GIF Image", 'GIFf','GCon' },
  { IMAGE_JPG, "jpeg,jpg", "JPEG Image", 'JPEG', 'GCon' },
  { IMAGE_PNG, "png", "PNG Image", PRUint32(0x3F3F3F3F), PRUint32(0x3F3F3F3F) },
  { IMAGE_ART, "art", "ART Image", PRUint32(0x3F3F3F3F), PRUint32(0x3F3F3F3F) },
  { IMAGE_TIFF, "tiff,tif", "TIFF Image", PRUint32(0x3F3F3F3F), PRUint32(0x3F3F3F3F) },
  { APPLICATION_POSTSCRIPT, "ps,eps,ai", "Postscript File", PRUint32(0x3F3F3F3F), PRUint32(0x3F3F3F3F) },
  { TEXT_RTF, "rtf", "Rich Text Format", PRUint32(0x3F3F3F3F), PRUint32(0x3F3F3F3F) },
  { TEXT_CPP, "cpp", "CPP file", 'TEXT','CWIE' },
  { "application/x-arj", "arj", "ARJ file", PRUint32(0x3F3F3F3F), PRUint32(0x3F3F3F3F) },
  { APPLICATION_XPINSTALL, "xpi", "XPInstall Install", 'xpi*','MOSS' },
};

NS_IMPL_THREADSAFE_ADDREF(nsExternalHelperAppService)
NS_IMPL_THREADSAFE_RELEASE(nsExternalHelperAppService)

NS_INTERFACE_MAP_BEGIN(nsExternalHelperAppService)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIExternalHelperAppService)
   NS_INTERFACE_MAP_ENTRY(nsIExternalHelperAppService)
   NS_INTERFACE_MAP_ENTRY(nsPIExternalAppLauncher)
   NS_INTERFACE_MAP_ENTRY(nsIExternalProtocolService)
   NS_INTERFACE_MAP_ENTRY(nsIMIMEService)
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
}

nsExternalHelperAppService::~nsExternalHelperAppService()
{
  if (mMimeInfoCache)
  {
    mMimeInfoCache->Reset((nsHashtableEnumFunc)DeleteEntry, nsnull);
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

  nsCOMPtr<nsIRDFRemoteDataSource> remoteDS = do_CreateInstance(kRDFXMLDataSourceCID, &rv);
  mOverRideDataSource = do_QueryInterface(remoteDS);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIFile> mimeTypesFile;
  nsXPIDLCString pathBuf;
  nsCOMPtr<nsIFileSpec> mimeTypesFileSpec;

  rv = NS_GetSpecialDirectory(NS_APP_USER_MIMETYPES_50_FILE, getter_AddRefs(mimeTypesFile));
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = mimeTypesFile->GetPath(getter_Copies(pathBuf));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = NS_NewFileSpec(getter_AddRefs(mimeTypesFileSpec));
  NS_ENSURE_SUCCESS(rv, rv);
  mimeTypesFileSpec->SetNativePath(pathBuf);

  nsXPIDLCString url;
  rv = mimeTypesFileSpec->GetURLString(getter_Copies(url));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = remoteDS->Init(url);
  NS_ENSURE_SUCCESS(rv, rv);

  // for now load synchronously (async seems to be busted)
  rv = remoteDS->Refresh(PR_TRUE);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed refresh?\n");

#ifdef DEBUG_mscott
    PRBool loaded;
    rv = remoteDS->GetLoaded(&loaded);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed getload\n");
    printf("After refresh: datasource is %s\n", loaded ? "loaded" : "not loaded");
#endif

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
  return NS_ERROR_NOT_IMPLEMENTED;
}

// it's ESSENTIAL that this method return an error code if we were unable to determine how this content should be handle
// this allows derived OS implementations of Docontent to step in and look for OS specific solutions.
NS_IMETHODIMP nsExternalHelperAppService::DoContent(const char *aMimeContentType, nsIURI *aURI, nsISupports *aWindowContext, 
                                                    PRBool *aAbortProcess, nsIStreamListener ** aStreamListener)
{
  InitDataSource();

  // (1) try to get a mime info object for the content type....if we don't know anything about the type, then
  // we certainly can't handle it and we'll just return without creating a stream listener.
  *aStreamListener = nsnull;

  nsCOMPtr<nsIMIMEInfo> mimeInfo;
  nsresult rv = GetMIMEInfoForMimeTypeFromDS(aMimeContentType, getter_AddRefs(mimeInfo));

  if (NS_SUCCEEDED(rv) && mimeInfo)
  {
    // ask the OS specific subclass to create a stream listener for us that binds this suggested application
    // even if this fails, return NS_OK...
    nsXPIDLCString fileExtension;
    mimeInfo->FirstExtension(getter_Copies(fileExtension));
    nsExternalAppHandler * app = CreateNewExternalHandler(mimeInfo, fileExtension, aWindowContext);
    if (app)
      app->QueryInterface(NS_GET_IID(nsIStreamListener), (void **) aStreamListener);
    return NS_OK;
  }

  // if we made it here, then we were unable to handle this ourselves..return an error so the
  // derived class will know to try OS specific wonders on it.
  return NS_ERROR_FAILURE;
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
        literal->GetValueConst(&stringValue);
        fileExtension.AssignWithConversion(stringValue);
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

  nsCString contentTypeHandlerNodeName (NC_CONTENT_NODE_HANDLER_PREFIX);
  contentTypeHandlerNodeName.Append(aContentType);

  nsCOMPtr<nsIRDFResource> contentTypeHandlerNodeResource;
  aRDFService->GetResource(contentTypeHandlerNodeName, getter_AddRefs(contentTypeHandlerNodeResource));
  NS_ENSURE_TRUE(contentTypeHandlerNodeResource, NS_ERROR_FAILURE); // that's not good! we have an error in the rdf file

  // now process the application handler information
  aMIMEInfo->SetPreferredAction(nsIMIMEInfo::useHelperApp);

  // save to disk
  FillLiteralValueFromTarget(contentTypeHandlerNodeResource,kNC_SaveToDisk, &stringValue);
  if (stringValue && nsCRT::strcasecmp(stringValue, "true") == 0)
       aMIMEInfo->SetPreferredAction(nsIMIMEInfo::saveToDisk);

  // handle internal
  FillLiteralValueFromTarget(contentTypeHandlerNodeResource,kNC_HandleInternal, &stringValue);
  if (stringValue && nsCRT::strcasecmp(stringValue, "true") == 0)
       aMIMEInfo->SetPreferredAction(nsIMIMEInfo::handleInternally);
  
  // always ask
  FillLiteralValueFromTarget(contentTypeHandlerNodeResource,kNC_AlwaysAsk, &stringValue);
  if (stringValue && nsCRT::strcasecmp(stringValue, "false") == 0)
       aMIMEInfo->SetAlwaysAskBeforeHandling(PR_FALSE);
  else
    aMIMEInfo->SetAlwaysAskBeforeHandling(PR_TRUE);


  // now digest the external application information

  nsCAutoString externalAppNodeName (NC_CONTENT_NODE_EXTERNALAPP_PREFIX);
  externalAppNodeName.Append(aContentType);
  nsCOMPtr<nsIRDFResource> externalAppNodeResource;
  aRDFService->GetResource(externalAppNodeName, getter_AddRefs(externalAppNodeResource));

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
    nsCOMPtr<nsIRDFNode> target;
    nsCOMPtr<nsIRDFResource> source;
    nsCOMPtr<nsIRDFResource> contentTypeNodeResource;
  
    nsCString contentTypeNodeName (NC_CONTENT_NODE_PREFIX);
    contentTypeNodeName.Append(aContentType);

    nsCOMPtr<nsIRDFService> rdf = do_GetService(kRDFServiceCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = rdf->GetResource(contentTypeNodeName, getter_AddRefs(contentTypeNodeResource));
    // we need a way to determine if this content type resource is really in the graph or not...
    // every mime type should have a value field so test for that target..
    mOverRideDataSource->GetTarget(contentTypeNodeResource, kNC_Value, PR_TRUE, getter_AddRefs(target));

    if (NS_SUCCEEDED(rv) && target)
    {
       // create a mime info object and we'll fill it in based on the values from the data source
       nsCOMPtr<nsIMIMEInfo> mimeInfo (do_CreateInstance(NS_MIMEINFO_CONTRACTID));
       rv = FillTopLevelProperties(aContentType, contentTypeNodeResource, rdf, mimeInfo);
       NS_ENSURE_SUCCESS(rv, rv);
       rv = FillContentHandlerProperties(aContentType, contentTypeNodeResource, rdf, mimeInfo);

       *aMIMEInfo = mimeInfo;
       NS_IF_ADDREF(*aMIMEInfo);
       AddMimeInfoToCache(mimeInfo);
       
       // now that we found a mime object for this type, DON'T forget to add it to our hash table
       // so we won't query the data source for it the next time around...

    } // if we have a node in the graph for this content type
    else
      *aMIMEInfo = nsnull;
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
// begin external app handler implementation 
//////////////////////////////////////////////////////////////////////////////////////////////////////

NS_IMPL_THREADSAFE_ADDREF(nsExternalAppHandler)
NS_IMPL_THREADSAFE_RELEASE(nsExternalAppHandler)

NS_INTERFACE_MAP_BEGIN(nsExternalAppHandler)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIStreamObserver)
   NS_INTERFACE_MAP_ENTRY(nsIHelperAppLauncher)   
NS_INTERFACE_MAP_END_THREADSAFE

nsExternalAppHandler::nsExternalAppHandler()
{
  NS_INIT_ISUPPORTS();
  mCanceled = PR_FALSE;
  mReceivedDispostionInfo = PR_FALSE;
  mStopRequestIssued = PR_FALSE;
  mDataBuffer = (char *) nsMemory::Alloc((sizeof(char) * DATA_BUFFER_SIZE));
}

nsExternalAppHandler::~nsExternalAppHandler()
{
  if (mDataBuffer)
    nsMemory::Free(mDataBuffer);
}

void nsExternalAppHandler::ExtractSuggestedFileNameFromChannel(nsIChannel * aChannel)
{
  // if the channel is an http channel and we have a content disposition header set, 
  // then use the file name suggested there as the preferred file name to SUGGEST to the user.
  // we shouldn't actually use that without their permission...o.t. just use our temp file
  // Try to get HTTP channel....if we have a content-disposition header then we can
  nsCOMPtr<nsIHTTPChannel> httpChannel = do_QueryInterface( aChannel );
  if ( httpChannel ) 
  {
    // Get content-disposition response header and extract a file name if there is one...
    // content-disposition: has format: disposition-type < ; filename=value >
    nsCOMPtr<nsIAtom> atom = NS_NewAtom( "content-disposition" );
    if (atom) 
    {
      nsXPIDLCString disp; 
      nsresult rv = httpChannel->GetResponseHeader( atom, getter_Copies( disp ) );
      if ( NS_SUCCEEDED( rv ) && disp ) 
      {
        nsCAutoString dispositionValue;
        dispositionValue = disp;
        PRInt32 pos = dispositionValue.Find("filename=", PR_TRUE);
        if (pos > 0)
        {
          // extract everything after the filename= part and treat that as the file name...
          nsCAutoString dispFileName;
          dispositionValue.Mid(dispFileName, pos + nsCRT::strlen("filename="), -1);
          if (!dispFileName.IsEmpty()) // if we got a file name back..
          {
            pos = dispFileName.FindChar(';', PR_TRUE);
            if (pos > 0)
              dispFileName.Truncate(pos);

            // ONLY if we got here, will we remember the suggested file name...
            mHTTPSuggestedFileName.AssignWithConversion(dispFileName);
          }
        } // if we found a file name in the header disposition field
      } // we had a disp header 
    } // we created the atom correctly
  } // if we had an http channel
}

nsresult nsExternalAppHandler::SetUpTempFile(nsIChannel * aChannel)
{
  nsresult rv = NS_OK;

#ifdef XP_MAC
 // create a temp file for the data...and open it for writing.
 // use NS_MAC_DEFAULT_DOWNLOAD_DIR which gets download folder from InternetConfig
 // if it can't get download folder pref, then it uses desktop folder
  NS_GetSpecialDirectory(NS_MAC_DEFAULT_DOWNLOAD_DIR, getter_AddRefs(mTempFile));
 // while we're here, also set Mac type and creator
 if (mMimeInfo)
 {
   nsCOMPtr<nsILocalFileMac> macfile = do_QueryInterface(mTempFile);
   if (macfile)
   {
     PRUint32 type, creator;
     mMimeInfo->GetMacType(&type);
     mMimeInfo->GetMacCreator(&creator);
     macfile->SetFileTypeAndCreator(type, creator);
   }
 }
#else
  // create a temp file for the data...and open it for writing.
  NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(mTempFile));
#endif

  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));
  nsCOMPtr<nsIURL> url = do_QueryInterface(uri);

  nsCAutoString tempLeafName;   

  if (url)
  {
    // try to extract the file name from the url and use that as a first pass as the
    // leaf name of our temp file...
    nsXPIDLCString leafName;
    url->GetFileName(getter_Copies(leafName));
    if (leafName)
    {
      tempLeafName = leafName;
      // strip off whatever extension this file may have and force our own extension.
      PRInt32 pos = tempLeafName.RFindCharInSet(".");
      if (pos > 0) 
        tempLeafName.Truncate(pos); // truncate everything after the first comma (including the comma)
    }
  }

  if (tempLeafName.IsEmpty())
    tempLeafName = "test"; // this is bogus...what do i do if i can't get a file name from the url.
  
  // now append our extension.
  tempLeafName.Append(mTempFileExtension);

  mTempFile->Append(tempLeafName); // make this file unique!!!
  mTempFile->CreateUnique(nsnull, nsIFile::NORMAL_FILE_TYPE, 0644);

  nsCOMPtr<nsIFileChannel> fileChannel = do_CreateInstance(NS_LOCALFILECHANNEL_CONTRACTID);
  if (fileChannel)
  {
    rv = fileChannel->Init(mTempFile, -1, 0);
    if (NS_FAILED(rv)) return rv; 
    rv = fileChannel->OpenOutputStream(getter_AddRefs(mOutStream));
    if (NS_FAILED(rv)) return rv; 
  }

  return rv;
}

NS_IMETHODIMP nsExternalAppHandler::OnStartRequest(nsIChannel * aChannel, nsISupports * aCtxt)
{
  NS_ENSURE_ARG(aChannel);

  // first, check to see if we've been canceled....
  if (mCanceled) // then go cancel our underlying channel too
    return aChannel->Cancel(NS_BINDING_ABORTED);

  nsresult rv = SetUpTempFile(aChannel);
  ExtractSuggestedFileNameFromChannel(aChannel); 

  // now that the temp file is set up, find out if we need to invoke a dialog asking the user what
  // they want us to do with this content...

  PRBool alwaysAsk = PR_FALSE;
  mMimeInfo->GetAlwaysAskBeforeHandling(&alwaysAsk);
  if (alwaysAsk)
  {
    // do this first! make sure we don't try to take an action until the user tells us what they want to do
    // with it...
    mReceivedDispostionInfo = PR_FALSE; 

    // invoke the dialog!!!!! use mWindowContext as the window context parameter for the dialog service
    nsCOMPtr<nsIHelperAppLauncherDialog> dlgService( do_GetService( NS_IHELPERAPPLAUNCHERDLG_CONTRACTID ) );
    if ( dlgService ) 
      rv = dlgService->Show( this, mWindowContext );

    // what do we do if the dialog failed? I guess we should call Cancel and abort the load....
  }
  else
    mReceivedDispostionInfo = PR_TRUE; // no need to wait for a response from the user

  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::OnDataAvailable(nsIChannel * aChannel, nsISupports * aCtxt,
                                                  nsIInputStream * inStr, PRUint32 sourceOffset, PRUint32 count)
{
  // first, check to see if we've been canceled....
  if (mCanceled) // then go cancel our underlying channel too
    return aChannel->Cancel(NS_BINDING_ABORTED);

  // read the data out of the stream and write it to the temp file.
  PRUint32 numBytesRead = 0;
  if (mOutStream && mDataBuffer && count > 0)
  {
    PRUint32 numBytesRead = 0; 
    PRUint32 numBytesWritten = 0;
    while (count > 0) // while we still have bytes to copy...
    {
      inStr->Read(mDataBuffer, PR_MIN(count, DATA_BUFFER_SIZE - 1), &numBytesRead);
      if (count >= numBytesRead)
        count -= numBytesRead; // subtract off the number of bytes we just read
      else
        count = 0;
      mOutStream->Write(mDataBuffer, numBytesRead, &numBytesWritten);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::OnStopRequest(nsIChannel * aChannel, nsISupports *aCtxt, 
                                                nsresult aStatus, const PRUnichar * errorMsg)
{
  nsresult rv = NS_OK;
  mStopRequestIssued = PR_TRUE;

  // first, check to see if we've been canceled....
  if (mCanceled) // then go cancel our underlying channel too
    return aChannel->Cancel(NS_BINDING_ABORTED);

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

  if (mReceivedDispostionInfo && !mCanceled)
  {
    nsMIMEInfoHandleAction action = nsIMIMEInfo::saveToDisk;
    mMimeInfo->GetPreferredAction(&action);
    if (action == nsIMIMEInfo::saveToDisk)
    {
      rv = SaveToDisk(mFinalFileDestination, PR_FALSE);
    }
    else
    {
      rv = LaunchWithApplication(nsnull, PR_FALSE);
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
    mTempFileExtension = ".";
  mTempFileExtension.Append(aTempFileExtension);
  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::GetMIMEInfo(nsIMIMEInfo ** aMIMEInfo)
{
  *aMIMEInfo = mMimeInfo;
  NS_IF_ADDREF(*aMIMEInfo);
  return NS_OK;
}

nsresult nsExternalAppHandler::PromptForSaveToFile(nsILocalFile ** aNewFile, const PRUnichar * aDefaultFile)
{
  // invoke the dialog!!!!! use mWindowContext as the window context parameter for the dialog service
  nsCOMPtr<nsIHelperAppLauncherDialog> dlgService( do_GetService( NS_IHELPERAPPLAUNCHERDLG_CONTRACTID ) );
  nsresult rv = NS_OK;
  if ( dlgService ) 
    rv = dlgService->PromptForSaveToFile(mWindowContext, aDefaultFile, NS_ConvertASCIItoUCS2(mTempFileExtension).get(), aNewFile);

  return rv;
}

NS_IMETHODIMP nsExternalAppHandler::SaveToDisk(nsIFile * aNewFileLocation, PRBool aRememberThisPreference)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsILocalFile> fileToUse;
  if (mCanceled)
    return NS_OK;

  if (!aNewFileLocation)
  {
    nsXPIDLString leafName;
    mTempFile->GetUnicodeLeafName(getter_Copies(leafName));
    if (mHTTPSuggestedFileName.IsEmpty())
      rv = PromptForSaveToFile(getter_AddRefs(fileToUse), leafName);
    else
      rv = PromptForSaveToFile(getter_AddRefs(fileToUse), mHTTPSuggestedFileName.GetUnicode());

    if (NS_FAILED(rv)) 
      return Cancel();
    mFinalFileDestination = do_QueryInterface(fileToUse);
  }
  else
   fileToUse = do_QueryInterface(aNewFileLocation);

  mReceivedDispostionInfo = PR_TRUE;
  // if the on stop request was actually issued then it's now time to actually perform the file move....
  if (mStopRequestIssued && fileToUse)
  {
     // extract the new leaf name from the file location
     nsXPIDLCString fileName;
     fileToUse->GetLeafName(getter_Copies(fileName));
     nsCOMPtr<nsIFile> directoryLocation;
     fileToUse->GetParent(getter_AddRefs(directoryLocation));
     if (directoryLocation)
     {
       rv = mTempFile->MoveTo(directoryLocation, fileName);
     }
  }

  return rv;
}

NS_IMETHODIMP nsExternalAppHandler::LaunchWithApplication(nsIFile * aApplication, PRBool aRememberThisPreference)
{
  if (mCanceled)
    return NS_OK;

  mReceivedDispostionInfo = PR_TRUE; 
  if (mMimeInfo && aApplication)
    mMimeInfo->SetPreferredApplicationHandler(aApplication);

  // if a stop request was already issued then proceed with launching the application.
  nsresult rv = NS_OK;
  if (mStopRequestIssued)
  {
    nsCOMPtr<nsPIExternalAppLauncher> helperAppService (do_GetService(NS_EXTERNALHELPERAPPSERVICE_CONTRACTID));
    if (helperAppService)
    {
      rv = helperAppService->LaunchAppWithTempFile(mMimeInfo, mTempFile);
    }
  }

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
    mTempFile->Delete(PR_TRUE);
    mTempFile = nsnull;
  }
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

  fileExt.ToLowerCase();
  // if the file extension contains a '.', our hash key doesn't include the '.'
  // so skip over it...
  if (fileExt.First() == '.') 
    fileExt.Cut(0, 1); // cut the '.'
 
  nsCStringKey key(fileExt.GetBuffer());

  *_retval = (nsIMIMEInfo *) mMimeInfoCache->Get(&key);
  NS_IF_ADDREF(*_retval);
  if (!*_retval) rv = NS_ERROR_FAILURE;
  return rv;
}

NS_IMETHODIMP nsExternalHelperAppService::GetFromMIMEType(const char *aMIMEType, nsIMIMEInfo **_retval) 
{
  nsresult rv = NS_OK;
  nsCAutoString MIMEType(aMIMEType);
  MIMEType.ToLowerCase();

  nsCStringKey key(MIMEType.GetBuffer());

  *_retval = (nsIMIMEInfo *) mMimeInfoCache->Get(&key);
  NS_IF_ADDREF(*_retval);

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
  if (NS_FAILED(rv)) return rv;
  return info->GetMIMEType(aContentType);
}

NS_IMETHODIMP nsExternalHelperAppService::GetTypeFromURI(nsIURI *aURI, char **aContentType) 
{
  nsresult rv = NS_ERROR_FAILURE;
  // first try to get a url out of the uri so we can skip post
  // filename stuff (i.e. query string)
  nsCOMPtr<nsIURL> url = do_QueryInterface(aURI, &rv);
    
#ifdef XP_MAC
 	if (NS_SUCCEEDED(rv))
 	{
    nsXPIDLCString fileExt;
    url->GetFileExtension(getter_Copies(fileExt));     
    
    nsresult rv2;
    nsCOMPtr<nsIFileURL> fileurl = do_QueryInterface( url, &rv2 );
    if ( NS_SUCCEEDED ( rv2 ) )
    {
    	nsCOMPtr <nsIFile> file;
    	rv2 = fileurl->GetFile( getter_AddRefs( file ) );
    	if ( NS_SUCCEEDED( rv2 ) )
    	{
    		rv2 = GetTypeFromFile( file, aContentType );
				if( NS_SUCCEEDED ( rv2 ) )
					return rv2;
			}			
    }
  }
#endif
    
  if (NS_SUCCEEDED(rv)) 
  {
      nsXPIDLCString ext;
      rv = url->GetFileExtension(getter_Copies(ext));
      if (NS_FAILED(rv)) return rv;
      rv = GetTypeFromExtension(ext, aContentType);
      return rv;
  }

  nsXPIDLCString cStrSpec;
  // no url, let's give the raw spec a shot
  rv = aURI->GetSpec(getter_Copies(cStrSpec));
  if (NS_FAILED(rv)) return rv;

  nsAutoString specStr; specStr.AssignWithConversion(cStrSpec);

  // find the file extension (if any)
  nsAutoString extStr;
  PRInt32 extLoc = specStr.RFindChar('.');
  if (-1 != extLoc) 
  {
      specStr.Right(extStr, specStr.Length() - extLoc - 1);
      char *ext = extStr.ToNewCString();
      if (!ext) return NS_ERROR_OUT_OF_MEMORY;
      rv = GetTypeFromExtension(ext, aContentType);
      nsMemory::Free(ext);
  }
  else
      return NS_ERROR_FAILURE;
  return rv;
}

NS_IMETHODIMP nsExternalHelperAppService::GetTypeFromFile( nsIFile* aFile, char **aContentType )
{
	nsresult rv;
	nsCOMPtr<nsIMIMEInfo> info;
	
	// Get the Extension
	char* fileName;
	const char* ext = nsnull;
  rv = aFile->GetLeafName(&fileName);
  if (NS_FAILED(rv)) return rv;
 
  if (fileName != nsnull) 
  {
    PRInt32 len = nsCRT::strlen(fileName); 
    for (PRInt32 i = len; i >= 0; i--) 
    {
      if (fileName[i] == '.') 
      {
        ext = &fileName[i + 1];
        break;
      }
    }
  }
  
  nsCString fileExt( ext );       
  nsCRT::free(fileName);
  // Handle the mac case
#ifdef XP_MAC
  nsCOMPtr<nsILocalFileMac> macFile;
  macFile = do_QueryInterface( aFile, &rv );
  if ( NS_SUCCEEDED( rv ) && fileExt.IsEmpty())
  {
	PRUint32 type, creator;
	macFile->GetFileTypeAndCreator( (OSType*)&type,(OSType*) &creator );   
  	nsCOMPtr<nsIInternetConfigService> icService (do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID));
    if (icService)
    {
      rv = icService->GetMIMEInfoFromTypeCreator(type, creator, fileExt, getter_AddRefs(info));		 							
      if ( NS_SUCCEEDED( rv) )
	    return info->GetMIMEType(aContentType);
	}
  }
#endif
  // Windows, unix and mac when no type match occured.   
  if (fileExt.IsEmpty())
	  return NS_ERROR_FAILURE;    
  return GetTypeFromExtension( fileExt, aContentType );
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
    mimeInfo->SetDescription(NS_ConvertASCIItoUCS2(defaultMimeEntries[index].mDescription));
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
  aMIMEInfo->GetMIMEType(getter_Copies(mimeType));

  nsCStringKey key(mimeType);
  nsIMIMEInfo * oldInfo = (nsIMIMEInfo*)mMimeInfoCache->Put(&key, aMIMEInfo);

  // add a reference for the hash table entry....
  NS_ADDREF(aMIMEInfo); 

  // now we need to add entries for each file extension 
  char** extensions;
  PRUint32 count;
  rv = aMIMEInfo->GetFileExtensions(&count, &extensions );
  if (NS_FAILED(rv)) return NS_OK; 

  for ( PRUint32 i = 0; i < count; i++ )
  {
     key = extensions[i];
     oldInfo = (nsIMIMEInfo*) mMimeInfoCache->Put(&key, aMIMEInfo);
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

