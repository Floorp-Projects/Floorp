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

#ifndef nsExternalHelperAppService_h__
#define nsExternalHelperAppService_h__

#include "nsIExternalHelperAppService.h"
#include "nsIExternalProtocolService.h"

#include "nsIMIMEInfo.h"
#include "nsIMIMEService.h"
#include "nsIStreamListener.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsIOutputStream.h"
#include "nsString.h"

#include "nsIRDFDataSource.h"
#include "nsIRDFResource.h"
#include "nsHashtable.h"
#include "nsCOMPtr.h"

class nsExternalAppHandler;
class nsIMIMEInfo;
class nsIRDFService;

class nsExternalHelperAppService : public nsIExternalHelperAppService, public nsPIExternalAppLauncher, 
                                   public nsIExternalProtocolService, public nsIMIMEService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEXTERNALHELPERAPPSERVICE
  NS_DECL_NSPIEXTERNALAPPLAUNCHER
  NS_DECL_NSIEXTERNALPROTOCOLSERVICE
  NS_DECL_NSIMIMESERVICE

  nsExternalHelperAppService();
  virtual ~nsExternalHelperAppService();
  nsresult InitDataSource();

  // CreateNewExternalHandler is implemented only by the base class...
  // create an external app handler and binds it with a mime info object which represents
  // how we want to dispose of this content
  // aFileExtension --> the extension we need to append to our temp file INCLUDING the ".". i.e. .mp3
  nsExternalAppHandler * CreateNewExternalHandler(nsIMIMEInfo * aMIMEInfo, const char * aFileExtension, nsISupports * aWindowContext);
 
  // GetMIMEInfoForMimeTypeFromDS --> Given a content type, look up the user override information to 
  // see if we have a mime info object representing this content type. The user over ride information is contained
  // in a in memory data source....
  nsresult GetMIMEInfoForMimeTypeFromDS(const char * aContentType, nsIMIMEInfo ** aMIMEInfo);

  // GetFileTokenForPath must be implemented by each platform. 
  // platformAppPath --> a platform specific path to an application that we got out of the 
  //                     rdf data source. This can be a mac file spec, a unix path or a windows path depending on the platform
  // aFile --> an nsIFile representation of that platform application path.
  virtual nsresult GetFileTokenForPath(const PRUnichar * platformAppPath, nsIFile ** aFile) = 0;

protected:
  nsCOMPtr<nsIRDFDataSource> mOverRideDataSource;

	nsCOMPtr<nsIRDFResource> kNC_Description;
	nsCOMPtr<nsIRDFResource> kNC_Value;
	nsCOMPtr<nsIRDFResource> kNC_FileExtensions;
  nsCOMPtr<nsIRDFResource> kNC_Path;
  nsCOMPtr<nsIRDFResource> kNC_SaveToDisk;
  nsCOMPtr<nsIRDFResource> kNC_AlwaysAsk;
  nsCOMPtr<nsIRDFResource> kNC_HandleInternal;
  nsCOMPtr<nsIRDFResource> kNC_PrettyName;

  PRBool mDataSourceInitialized;

  // helper routines for digesting the data source and filling in a mime info object for a given
  // content type inside that data source.
  nsresult FillTopLevelProperties(const char * aContentType, nsIRDFResource * aContentTypeNodeResource, 
                                  nsIRDFService * aRDFService, nsIMIMEInfo * aMIMEInfo);
  nsresult FillContentHandlerProperties(const char * aContentType, nsIRDFResource * aContentTypeNodeResource, 
                                        nsIRDFService * aRDFService, nsIMIMEInfo * aMIMEInfo);

  // a small helper function which gets the target for a given source and property...QIs to a literal
  // and returns a CONST ptr to the string value of that target
  nsresult FillLiteralValueFromTarget(nsIRDFResource * aSource, nsIRDFResource * aProperty, const PRUnichar ** aLiteralValue);

  // in addition to the in memory data source which stores the user over ride mime types, we also use a hash table
  // for quick look ups of mime types...
  nsHashtable   *mMimeInfoCache; // used for fast access and multi index lookups
  // used to add entries to the mime info cache
  virtual nsresult AddMimeInfoToCache(nsIMIMEInfo * aMIMEInfo);
  virtual nsresult AddDefaultMimeTypesToCache();

};

// this is a small private struct used to help us initialize some
// default mime types.
struct nsDefaultMimeTypeEntry {
  const char* mMimeType; 
  const char* mFileExtensions;
  const char* mDescription;
  PRUint32 mMactype;
  PRUint32 mMacCreator;
};


// An external app handler is just a small little class that presents itself as 
// a nsIStreamListener. It saves the incoming data into a temp file. The handler
// is bound to an application when it is created. When it receives an OnStopRequest
// it launches the application using the temp file it has stored the data into.
// we create a handler every time we have to process data using a helper app.

// we need to read the data out of the incoming stream into a buffer which we can then use
// to write the data into the output stream representing the temp file...
#define DATA_BUFFER_SIZE (4096*2) 

class nsExternalAppHandler : public nsIStreamListener, public nsIHelperAppLauncher
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSISTREAMOBSERVER
  NS_DECL_NSIHELPERAPPLAUNCHER

  nsExternalAppHandler();
  virtual ~nsExternalAppHandler();

  virtual nsresult Init(nsIMIMEInfo * aMIMEInfo, const char * aFileExtension, nsISupports * aWindowContext);

protected:
  nsCOMPtr<nsIFile> mTempFile;
  nsCString mTempFileExtension;
  nsCOMPtr<nsIMIMEInfo> mMimeInfo;
  nsCOMPtr<nsIOutputStream> mOutStream; // output stream to the temp file...
  nsCOMPtr<nsISupports> mWindowContext; 
  // the following field is set if we were processing an http channel that had a content disposition header
  // which specified the SUGGESTED file name we should present to the user in the save to disk dialog. 
  nsString mHTTPSuggestedFileName;

  // the canceled flag is set if the user canceled the launching of this application before we finished
  // saving the data to a temp file...
  PRBool mCanceled;

  // have we received information from the user about how they want to dispose of this content...
  PRBool mReceivedDispostionInfo;
  PRBool mStopRequestIssued; 

  // when we are told to save the temp file to disk (in a more permament location) before we are done
  // writing the content to a temp file, then we need to remember the final destination until we are ready to
  // use it.
  nsCOMPtr<nsIFile> mFinalFileDestination;

  char * mDataBuffer;

  nsresult SetUpTempFile(nsIChannel * aChannel);
  nsresult PromptForSaveToFile(nsILocalFile ** aNewFile, const PRUnichar * aDefaultFile);
  // if the passed in channel is an nsIHTTPChannel, we'll attempt to extrace a suggested file name
  // from the content disposition header...
  void ExtractSuggestedFileNameFromChannel(nsIChannel * aChannel);
};

#endif // nsExternalHelperAppService_h__
