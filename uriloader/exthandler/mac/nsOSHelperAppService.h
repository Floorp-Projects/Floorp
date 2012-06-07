/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsOSHelperAppService_h__
#define nsOSHelperAppService_h__

// The OS helper app service is a subclass of nsExternalHelperAppService and is implemented on each
// platform. It contains platform specific code for finding helper applications for a given mime type
// in addition to launching those applications. This is the Mac version.

#include "nsExternalHelperAppService.h"
#include "nsCExternalHandlerService.h"
#include "nsMIMEInfoImpl.h"
#include "nsCOMPtr.h"

class nsOSHelperAppService : public nsExternalHelperAppService
{
public:
  nsOSHelperAppService();
  virtual ~nsOSHelperAppService();

  // override nsIExternalProtocolService methods
  NS_IMETHOD GetApplicationDescription(const nsACString& aScheme, nsAString& _retval);
  
  // method overrides --> used to hook the mime service into internet config....
  NS_IMETHOD GetFromTypeAndExtension(const nsACString& aType, const nsACString& aFileExt, nsIMIMEInfo ** aMIMEInfo);
  already_AddRefed<nsIMIMEInfo> GetMIMEInfoFromOS(const nsACString& aMIMEType, const nsACString& aFileExt, bool * aFound);
  NS_IMETHOD GetProtocolHandlerInfoFromOS(const nsACString &aScheme,
                                          bool *found,
                                          nsIHandlerInfo **_retval);

  // GetFileTokenForPath must be implemented by each platform. 
  // platformAppPath --> a platform specific path to an application that we got out of the 
  //                     rdf data source. This can be a mac file spec, a unix path or a windows path depending on the platform
  // aFile --> an nsIFile representation of that platform application path.
  virtual nsresult GetFileTokenForPath(const PRUnichar * platformAppPath, nsIFile ** aFile);

  nsresult OSProtocolHandlerExists(const char * aScheme,
                                   bool * aHandlerExists);

protected:
  virtual void FixFilePermissions(nsIFile* aFile);

private:
  PRUint32 mPermissions;
};

#endif // nsOSHelperAppService_h__
