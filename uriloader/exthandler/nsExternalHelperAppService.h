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
#include "nsIStreamListener.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsIOutputStream.h"
#include "nsCOMPtr.h"

class nsExternalAppHandler;

class nsExternalHelperAppService : public nsIExternalHelperAppService, nsPIExternalAppLauncher
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEXTERNALHELPERAPPSERVICE
  NS_DECL_NSPIEXTERNALAPPLAUNCHER

  nsExternalHelperAppService();
  virtual ~nsExternalHelperAppService();

  // create an external app handler and bind it with a cookie that came from the OS specific
  // helper app service subclass.
  virtual nsExternalAppHandler * CreateNewExternalHandler(nsISupports * aAppCookie);

protected:

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
  virtual nsresult Init(nsISupports * aExternalApplicationCookie);

protected:
  nsCOMPtr<nsIFile> mTempFile;
  nsCOMPtr<nsISupports> mExternalApplication;
  nsCOMPtr<nsIOutputStream> mOutStream; // output stream to the temp file...

  char * mDataBuffer;
};

#endif // nsExternalHelperAppService_h__
