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

#include "nsCExternalHandlerService.h" // contains progids for the helper app service

#ifdef XP_MAC
#include "nsILocalFileMac.h"
#endif // XP_MAC

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID, NS_RDFXMLDATASOURCE_CID);

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
}

nsExternalHelperAppService::~nsExternalHelperAppService()
{
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
  nsresult rv = GetMIMEInfoForMimeType(aMimeContentType, getter_AddRefs(mimeInfo));

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

nsresult nsExternalHelperAppService::GetMIMEInfoForMimeType(const char * aContentType, nsIMIMEInfo ** aMIMEInfo)
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
       nsCOMPtr<nsIMIMEInfo> mimeInfo (do_CreateInstance(NS_MIMEINFO_PROGID));
       rv = FillTopLevelProperties(aContentType, contentTypeNodeResource, rdf, mimeInfo);
       NS_ENSURE_SUCCESS(rv, rv);
       rv = FillContentHandlerProperties(aContentType, contentTypeNodeResource, rdf, mimeInfo);

       *aMIMEInfo = mimeInfo;
       NS_IF_ADDREF(*aMIMEInfo);
    } // if we have a node in the graph for this content type
    else
      *aMIMEInfo = nsnull;
  } // if we have a data source


  if (!*aMIMEInfo)
  {
    // try the old mime service.
    nsCOMPtr<nsIMIMEService> mimeService (do_GetService("component:||netscape|mime"));
    if (mimeService)
      mimeService->GetFromMIMEType(aContentType, aMIMEInfo);
    // if it doesn't have an entry then we really know nothing about this type...
  }

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
      if (pos > 0) // we have a comma separated list of languages...
        tempLeafName.Truncate(pos); // truncate everything after the first comma (including the comma)
    }
  }

  if (tempLeafName.IsEmpty())
    tempLeafName = "test"; // this is bogus...what do i do if i can't get a file name from the url.
  
  // now append our extension.
  tempLeafName.Append(mTempFileExtension);

  mTempFile->Append(tempLeafName); // make this file unique!!!
  mTempFile->CreateUnique(nsnull, nsIFile::NORMAL_FILE_TYPE, 0644);

  nsCOMPtr<nsIFileChannel> fileChannel = do_CreateInstance(NS_LOCALFILECHANNEL_PROGID);
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
    nsCOMPtr<nsIHelperAppLauncherDialog> dlgService( do_GetService( NS_IHELPERAPPLAUNCHERDLG_PROGID ) );
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
  nsCOMPtr<nsIHelperAppLauncherDialog> dlgService( do_GetService( NS_IHELPERAPPLAUNCHERDLG_PROGID ) );
  nsresult rv = NS_OK;
  if ( dlgService ) 
    rv = dlgService->PromptForSaveToFile(mWindowContext, aDefaultFile, NS_ConvertASCIItoUCS2(mTempFileExtension), aNewFile);

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
    rv = PromptForSaveToFile(getter_AddRefs(fileToUse), leafName);
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
    nsCOMPtr<nsPIExternalAppLauncher> helperAppService (do_GetService(NS_EXTERNALHELPERAPPSERVICE_PROGID));
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
  // temporary implementation --> call through to original implementation...
  nsCOMPtr<nsIMIMEService> oldService (do_GetService("component:||netscape|mimeold"));
  NS_ENSURE_TRUE(oldService, NS_ERROR_FAILURE);
  
  return oldService->GetFromExtension(aFileExt, _retval);
}



NS_IMETHODIMP nsExternalHelperAppService::GetFromMIMEType(const char *aMIMEType, nsIMIMEInfo **_retval) 
{
  nsresult rv = NS_OK;
  // temporary implementation --> call through to original implementation...
  nsCOMPtr<nsIMIMEService> oldService (do_GetService("component:||netscape|mimeold"));
  NS_ENSURE_TRUE(oldService, NS_ERROR_FAILURE);
  
  return oldService->GetFromMIMEType(aMIMEType, _retval);
}

NS_IMETHODIMP nsExternalHelperAppService::GetTypeFromExtension(const char *aFileExt, char **aContentType) 
{
  nsresult rv = NS_OK;
  // temporary implementation --> call through to original implementation...
  nsCOMPtr<nsIMIMEService> oldService (do_GetService("component:||netscape|mimeold"));
  NS_ENSURE_TRUE(oldService, NS_ERROR_FAILURE);
  
  return oldService->GetTypeFromExtension(aFileExt, aContentType);  
}

NS_IMETHODIMP nsExternalHelperAppService::GetTypeFromURI(nsIURI *aURI, char **aContentType) 
{
  nsresult rv = NS_OK;
  // temporary implementation --> call through to original implementation...
  nsCOMPtr<nsIMIMEService> oldService (do_GetService("component:||netscape|mimeold"));
  NS_ENSURE_TRUE(oldService, NS_ERROR_FAILURE);
  
  return oldService->GetTypeFromURI(aURI, aContentType);  
}

NS_IMETHODIMP nsExternalHelperAppService::GetTypeFromFile( nsIFile* aFile, char **aContentType )
{
  nsresult rv = NS_OK;
  // temporary implementation --> call through to original implementation...
  nsCOMPtr<nsIMIMEService> oldService (do_GetService("component:||netscape|mimeold"));
  NS_ENSURE_TRUE(oldService, NS_ERROR_FAILURE);
  
  return oldService->GetTypeFromFile(aFile, aContentType);  
}
