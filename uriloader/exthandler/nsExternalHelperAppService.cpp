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
#include "nsXPIDLString.h"
#include "nsMemory.h"
#include "nsIStreamListener.h"
#include "nsIMIMEService.h"

// used to manage our in memory data source of helper applications
#include "nsRDFCID.h"
#include "rdf.h"
#include "nsIRDFService.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIFileLocator.h"
#include "nsIFileSpec.h"
#include "nsFileLocations.h"
#include "nsHelperAppRDF.h"
#include "nsIMIMEInfo.h"

#include "nsCExternalHandlerService.h" // contains progids for the helper app service

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID, NS_RDFXMLDATASOURCE_CID);

NS_IMPL_THREADSAFE_ADDREF(nsExternalHelperAppService)
NS_IMPL_THREADSAFE_RELEASE(nsExternalHelperAppService)

NS_INTERFACE_MAP_BEGIN(nsExternalHelperAppService)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIExternalHelperAppService)
   NS_INTERFACE_MAP_ENTRY(nsIExternalHelperAppService)
   NS_INTERFACE_MAP_ENTRY(nsPIExternalAppLauncher)
   NS_INTERFACE_MAP_ENTRY(nsIExternalProtocolService)
NS_INTERFACE_MAP_END_THREADSAFE

nsExternalHelperAppService::nsExternalHelperAppService()
{
  NS_INIT_ISUPPORTS();
}

nsExternalHelperAppService::~nsExternalHelperAppService()
{
}

nsresult nsExternalHelperAppService::Init()
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFService> rdf = do_GetService(kRDFServiceCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRDFRemoteDataSource> remoteDS = do_CreateInstance(kRDFXMLDataSourceCID, &rv);
  mOverRideDataSource = do_QueryInterface(remoteDS);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIFileLocator> locator = do_GetService(NS_FILELOCATOR_PROGID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIFileSpec> mimeTypesFile;
  rv = locator->GetFileLocation(nsSpecialFileSpec::App_UsersMimeTypes50,
                                getter_AddRefs(mimeTypesFile));
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLCString url;
  rv = mimeTypesFile->GetURLString(getter_Copies(url));
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
    nsExternalAppHandler * app = CreateNewExternalHandler(mimeInfo, fileExtension);
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
                                                                            const char * aTempFileExtension)
{
  nsExternalAppHandler* handler = nsnull;
  NS_NEWXPCOM(handler, nsExternalAppHandler);
  // add any XP intialization code for an external handler that we may need here...
  // right now we don't have any but i bet we will before we are done.

  handler->Init(aMIMEInfo, aTempFileExtension);
  return handler;
}

nsresult nsExternalHelperAppService::FillTopLevelProperties(const char * aContentType, nsIRDFResource * aContentTypeNodeResource, 
                                                            nsIRDFService * aRDFService, nsIMIMEInfo * aMIMEInfo)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFNode> target;
  nsCOMPtr<nsIRDFLiteral> literal;
  const PRUnichar * stringValue;
  
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
  if (stringValue && nsCRT::strcasecmp(stringValue, "true") == 0)
       aMIMEInfo->SetPreferredAction(nsIMIMEInfo::alwaysAsk);

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
  NS_GetSpecialDirectory("system.DesktopDirectory", getter_AddRefs(mTempFile));
#else
  // create a temp file for the data...and open it for writing.
  NS_GetSpecialDirectory("system.OS_TemporaryDirectory", getter_AddRefs(mTempFile));
#endif

  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));
  nsCOMPtr<nsIURL> url = do_QueryInterface(uri);

  nsCAutoString tempLeafName ("test");  // WARNING THIS IS TEMPORARY CODE!!!
  tempLeafName.Append(mTempFileExtension);

#if 0
  if (url)
  {
    // try to extract the file name from the url and use that as a first pass as the
    // leaf name of our temp file...
    nsXPIDLCString leafName;
    url->GetFileName(getter_Copies(leafName));
    if (leafName)
    {
      nsCAutoString
      mTempFile->Append(leafName); // WARNING --> neeed a make Unique routine on nsIFile!!
    }
    else
      mTempFile->Append("test"); // WARNING THIS IS TEMPORARY CODE!!!
  }
  else
    mTempFile->Append("test"); // WARNING THIS IS TEMPORARY CODE!!!
#endif

  mTempFile->Append(tempLeafName); // make this file unique!!!
  mTempFile->CreateUnique(nsnull, nsIFile::NORMAL_FILE_TYPE, 0644);

  nsCOMPtr<nsIFileChannel> mFileChannel = do_CreateInstance(NS_LOCALFILECHANNEL_PROGID);
  if (mFileChannel)
  {
    rv = mFileChannel->Init(mTempFile, -1, 0);
    if (NS_FAILED(rv)) return rv; 
    rv = mFileChannel->OpenOutputStream(getter_AddRefs(mOutStream));
    if (NS_FAILED(rv)) return rv; 
  }

  return rv;
}

NS_IMETHODIMP nsExternalAppHandler::OnStartRequest(nsIChannel * aChannel, nsISupports * aCtxt)
{
  NS_ENSURE_ARG(aChannel);

  nsresult rv = SetUpTempFile(aChannel);

  // now that the temp file is set up, find out if we need to invoke a dialog asking the user what
  // they want us to do with this content...

  nsMIMEInfoHandleAction action = nsIMIMEInfo::alwaysAsk;
  mMimeInfo->GetPreferredAction(&action);
  if (action ==  nsIMIMEInfo::alwaysAsk)
  {
    // do this first! make sure we don't try to take an action until the user tells us what they want to do
    // with it...
    mReceivedDispostionInfo = PR_FALSE; 

    // invoke the dialog!!!!!
  }
  else
    mReceivedDispostionInfo = PR_TRUE; // no need to wait for a response from the user

  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::OnDataAvailable(nsIChannel * aChannel, nsISupports * aCtxt,
                                                  nsIInputStream * inStr, PRUint32 sourceOffset, PRUint32 count)
{
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

  // go ahead and execute the application passing in our temp file as an argument
  // this may involve us calling back into the OS external app service to make the call
  // for actually launching the helper app. It'd be great if nsIFile::spawn could be made to work
  // on the mac...right now the mac implementation ignores all arguments passed in.

  // close the stream...
  if (mOutStream)
    mOutStream->Close();

  if (mReceivedDispostionInfo && !mCanceled)
  {
    nsMIMEInfoHandleAction action = nsIMIMEInfo::alwaysAsk;
    mMimeInfo->GetPreferredAction(&action);
    if (action == nsIMIMEInfo::saveToDisk)
    {
#ifdef DEBUG_mscott
      // create a temp file for the data...and open it for writing.
      NS_GetSpecialDirectory("system.OS_TemporaryDirectory", getter_AddRefs(mFinalFileDestination));
      mFinalFileDestination->Append("foo.txt");
#endif
      rv = SaveToDisk(mFinalFileDestination, PR_FALSE);
    }
    else
    {
      rv = LaunchWithApplication(nsnull, PR_FALSE);
    }
  }

  return rv;
}

nsresult nsExternalAppHandler::Init(nsIMIMEInfo * aMIMEInfo, const char * aTempFileExtension)
{
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

NS_IMETHODIMP nsExternalAppHandler::SaveToDisk(nsIFile * aNewFileLocation, PRBool aRememberThisPreference)
{
  nsresult rv = NS_OK;
  mReceivedDispostionInfo = PR_TRUE;
  if (mStopRequestIssued)
  {
    if (aNewFileLocation)
    {
       // extract the new leaf name from the file location
       nsXPIDLCString fileName;
       aNewFileLocation->GetLeafName(getter_Copies(fileName));
       nsCOMPtr<nsIFile> directoryLocation;
       aNewFileLocation->GetParent(getter_AddRefs(directoryLocation));
       if (directoryLocation)
       {
         rv = mTempFile->MoveTo(directoryLocation, fileName);
       }
    }
  }
  // o.t. remember the new file location to save to.
  else
  {
    mFinalFileDestination = aNewFileLocation;
  }

  return rv;
}

NS_IMETHODIMP nsExternalAppHandler::LaunchWithApplication(nsIFile * aApplication, PRBool aRememberThisPreference)
{
  mReceivedDispostionInfo = PR_TRUE; 

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
  // o.t. remember the application we should use to launch the app and when the on stop request is issued, launch it.
  else
  {
    if (mMimeInfo)
      mMimeInfo->SetPreferredApplicationHandler(aApplication);
  }
  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::Cancel()
{
  mCanceled = PR_TRUE;
  return NS_OK;
}
