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

#include "nsIStreamListener.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsIOutputStream.h"
#include "nsString.h"

#include "nsIRDFDataSource.h"
#include "nsIRDFResource.h"
#include "nsCOMPtr.h"

class nsExternalAppHandler;
class nsIMIMEInfo;
class nsIRDFService;
class nsIMIMEInfo;

class nsExternalHelperAppService : public nsIExternalHelperAppService, public nsPIExternalAppLauncher, public nsIExternalProtocolService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEXTERNALHELPERAPPSERVICE
  NS_DECL_NSPIEXTERNALAPPLAUNCHER
  NS_DECL_NSIEXTERNALPROTOCOLSERVICE

  nsExternalHelperAppService();
  virtual ~nsExternalHelperAppService();
  nsresult Init();

  // CreateNewExternalHandler is implemented only by the base class...
  // create an external app handler and bind it with a cookie that came from the OS specific
  // helper app service subclass.
  // aFileExtension --> the extension we need to append to our temp file INCLUDING the ".". i.e. .mp3
  nsExternalAppHandler * CreateNewExternalHandler(nsISupports * aAppCookie, const char * aFileExtension);
 
  // GetMIMEInfoForMimeType --> this will eventually be part of an interface but for now
  // it's only used internally. Given a content type, look up the user override information to 
  // see if we have a mime info object representing this content type
  nsresult GetMIMEInfoForMimeType(const char * aContentType, nsIMIMEInfo ** aMIMEInfo);

  // GetFileTokenForPath must be implemented by each platform. 
  // platformAppPath --> a platform specific path to an application that we got out of the 
  //                     rdf data source. This can be a mac file spec, a unix path or a windows path depending on the platform
  // aFile --> an nsIFile representation of that platform application path.
  virtual nsresult GetFileTokenForPath(const PRUnichar * platformAppPath, nsIFile ** aFile) = 0;

  // CreateStreamListenerWithApp --> must be implemented by each platform.
  // aApplicationToUse --> the application the user wishes to launch with the incoming data
  // aFileExtensionForData --> the extension we are going to use for the temp file in the external app handler
  // aStreamListener --> the stream listener (really a external app handler) we're going to use for retrieving the data
  virtual nsresult CreateStreamListenerWithApp(nsIFile * aApplicationToUse, const char * aFileExtensionForData, nsIStreamListener ** aStreamListener) = 0;

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

  // helper routines for digesting the data source and filling in a mime info object for a given
  // content type inside that data source.
  nsresult FillTopLevelProperties(const char * aContentType, nsIRDFResource * aContentTypeNodeResource, 
                                  nsIRDFService * aRDFService, nsIMIMEInfo * aMIMEInfo);
  nsresult FillContentHandlerProperties(const char * aContentType, nsIRDFResource * aContentTypeNodeResource, 
                                        nsIRDFService * aRDFService, nsIMIMEInfo * aMIMEInfo);

  // a small helper function which gets the target for a given source and property...QIs to a literal
  // and returns a CONST ptr to the string value of that target
  nsresult FillLiteralValueFromTarget(nsIRDFResource * aSource, nsIRDFResource * aProperty, const PRUnichar ** aLiteralValue);

};

// An external app handler is just a small little class that presents itself as 
// a nsIStreamListener. It saves the incoming data into a temp file. The handler
// is bound to an application when it is created. When it receives an OnStopRequest
// it launches the application using the temp file it has stored the data into.
// we create a handler every time we have to process data using a helper app.

// we need to read the data out of the incoming stream into a buffer which we can then use
// to write the data into the output stream representing the temp file...
#define DATA_BUFFER_SIZE (4096*2) 

class nsExternalAppHandler : public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSISTREAMOBSERVER

  nsExternalAppHandler();
  virtual ~nsExternalAppHandler();

  // initialize the handler with a cookie that represents the external
  // application associated with this handler.
  virtual nsresult Init(nsISupports * aExternalApplicationCookie, const char * aFileExtension);

protected:
  nsCOMPtr<nsIFile> mTempFile;
  nsCString mTempFileExtension;
  nsCOMPtr<nsISupports> mExternalApplication;
  nsCOMPtr<nsIOutputStream> mOutStream; // output stream to the temp file...

  char * mDataBuffer;
};

#endif // nsExternalHelperAppService_h__
